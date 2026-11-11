/*!
 * Nukemail 渠道实现（nukemail.app）
 * 契约（抓前端 JS + curl/PoW 全流程实测）：
 * - PoW：GET /api/pow/challenge?difficulty=4 → {"id","challenge","difficulty"}；
 *   求 nonce 使 SHA-256(challenge+nonce) 十六进制前 difficulty 位为 0。
 * - 建箱：POST /api/inbox/create body {"address","domain","pow_id","pow_nonce"}
 *   → {"token":"NUKE-xxxxxxxx","email":"名@域名"}。
 * - 读信：GET /api/inbox（Cookie: nukemail_token=<token>），
 *   messages 元素字段 sender/sender_name/subject/body_html/body_text/received_at/read。
 *
 * 会话隔离：nukemail_token 由生成结果持久化为 token，
 *   读信时以显式 Cookie 头逐请求携带。
 */

use crate::config::{block_on, get_current_ua, http_client, http_client_no_cookie_jar};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use rand::RngExt;
use serde_json::{json, Value};
use sha2::{Digest, Sha256};

const BASE_URL: &str = "https://nukemail.app";

/// 求解 PoW：返回使 SHA-256(challenge+nonce) 前 difficulty 位为 0 的最小 nonce
fn solve_pow(challenge: &str, difficulty: usize) -> i64 {
    let prefix = "0".repeat(difficulty);
    let mut nonce: i64 = 0;
    loop {
        let mut hasher = Sha256::new();
        hasher.update(format!("{challenge}{nonce}").as_bytes());
        let digest = hex::encode(hasher.finalize());
        if digest.starts_with(&prefix) {
            return nonce;
        }
        nonce += 1;
    }
}

/// 生成本地随机名（与前端 generateRandomName 等价形态）
fn random_address() -> String {
    const CHARS: &[u8] = b"abcdefghijklmnopqrstuvwxyz0123456789";
    let mut rng = rand::rng();
    let body: String = (0..10)
        .map(|_| {
            let idx = rng.random_range(0..CHARS.len());
            CHARS[idx] as char
        })
        .collect();
    format!("nuke{body}")
}

/// 取第一个非 premium 的活跃域名
async fn pick_domain() -> Result<String, String> {
    let resp = http_client()
        .get(format!("{BASE_URL}/api/domains"))
        .header("Accept", "application/json")
        .header("User-Agent", get_current_ua())
        .send()
        .await
        .map_err(|e| format!("nukemail domains: {e}"))?;
    let status = resp.status();
    let body = resp
        .text()
        .await
        .map_err(|e| format!("nukemail domains: {e}"))?;
    if !status.is_success() {
        return Err(format!("nukemail domains: http {status}"));
    }
    let data: Value =
        serde_json::from_str(&body).map_err(|e| format!("nukemail domains: 解析失败: {e}"))?;
    for item in data
        .get("domains")
        .and_then(|v| v.as_array())
        .cloned()
        .unwrap_or_default()
    {
        let premium = item
            .get("is_premium_only")
            .and_then(|v| v.as_bool())
            .unwrap_or(false);
        let domain = item
            .get("domain")
            .and_then(|v| v.as_str())
            .unwrap_or("")
            .to_string();
        if !premium && !domain.is_empty() {
            return Ok(domain);
        }
    }
    Err("nukemail generate: 无可用非 premium 域名".into())
}

/// 创建 nukemail 临时邮箱（PoW 建箱）
pub fn generate_email() -> Result<EmailInfo, String> {
    let client = http_client_no_cookie_jar();

    block_on(async {
        /* 1) 取 PoW 挑战 */
        let resp = client
            .get(format!("{BASE_URL}/api/pow/challenge?difficulty=4"))
            .header("Accept", "application/json")
            .header("User-Agent", get_current_ua())
            .send()
            .await
            .map_err(|e| format!("nukemail generate: challenge 请求失败: {e}"))?;
        let status = resp.status();
        let body = resp
            .text()
            .await
            .map_err(|e| format!("nukemail generate: challenge 读取失败: {e}"))?;
        if !status.is_success() {
            return Err(format!("nukemail generate: challenge http {status}"));
        }
        let ch: Value = serde_json::from_str(&body)
            .map_err(|e| format!("nukemail generate: challenge 解析失败: {e}"))?;
        let challenge_id = ch
            .get("id")
            .and_then(|v| v.as_str())
            .unwrap_or("")
            .to_string();
        let challenge = ch
            .get("challenge")
            .and_then(|v| v.as_str())
            .unwrap_or("")
            .to_string();
        if challenge_id.is_empty() || challenge.is_empty() {
            return Err("nukemail generate: challenge 响应缺少 id/challenge".into());
        }
        let mut difficulty = ch.get("difficulty").and_then(|v| v.as_u64()).unwrap_or(4) as usize;
        if difficulty == 0 {
            difficulty = 4;
        }

        /* 2) 本地求 PoW 解（原生 sha2） */
        let nonce = solve_pow(&challenge, difficulty);

        /* 3) 取域名并建箱 */
        let domain = pick_domain().await?;
        let body = json!({
            "address": random_address(),
            "domain": domain,
            "pow_id": challenge_id,
            "pow_nonce": nonce.to_string(),
        });
        let resp = client
            .post(format!("{BASE_URL}/api/inbox/create"))
            .header("Content-Type", "application/json")
            .header("Accept", "application/json")
            .header("User-Agent", get_current_ua())
            .body(body.to_string())
            .send()
            .await
            .map_err(|e| format!("nukemail generate: create 请求失败: {e}"))?;
        let status = resp.status();
        let raw = resp
            .text()
            .await
            .map_err(|e| format!("nukemail generate: create 读取失败: {e}"))?
            .trim()
            .to_string();
        if !status.is_success() {
            return Err(format!("nukemail generate: create http {status}: {raw}"));
        }
        let data: Value = serde_json::from_str(&raw)
            .map_err(|e| format!("nukemail generate: create 解析失败: {e}"))?;
        let token = data
            .get("token")
            .and_then(|v| v.as_str())
            .unwrap_or("")
            .trim()
            .to_string();
        let email = data
            .get("email")
            .and_then(|v| v.as_str())
            .unwrap_or("")
            .trim()
            .to_string();
        if token.is_empty() || email.is_empty() {
            return Err(format!(
                "nukemail generate: create 响应缺少 token/email: {raw}"
            ));
        }

        Ok(EmailInfo {
            channel: Channel::Nukemail,
            email,
            token: Some(token),
            expires_at: None,
            created_at: None,
        })
    })
}

/// 读取 nukemail 收件箱（GET /api/inbox，Cookie: nukemail_token=<token>；
/// 会话过期经 resume 重设后重试一次）
pub fn get_emails(email: &str, token: &str) -> Result<Vec<Email>, String> {
    let em = email.trim();
    let access = token.trim();
    if access.is_empty() {
        return Err("nukemail: token 为空".into());
    }
    let client = http_client_no_cookie_jar();
    let cookie = format!("nukemail_token={access}");

    block_on(async {
        let mut data = fetch_inbox(&client, &cookie).await?;
        if data.get("state").and_then(|v| v.as_str()).unwrap_or("") == "expired"
            || data.get("state").is_none()
        {
            /* 会话可能已过期：经 resume 重设会话后重试 */
            let _ = client
                .post(format!("{BASE_URL}/api/inbox/resume"))
                .header("Content-Type", "application/json")
                .header("User-Agent", get_current_ua())
                .body(json!({"accessCode": access}).to_string())
                .send()
                .await;
            data = fetch_inbox(&client, &cookie).await?;
        }

        let messages = data
            .get("messages")
            .and_then(|v| v.as_array())
            .cloned()
            .unwrap_or_default();

        let out: Vec<Email> = messages
            .iter()
            .map(|m| {
                let mut flat = m.clone();
                if let Some(obj) = flat.as_object_mut() {
                    obj.insert("to".to_string(), json!(em));
                }
                /* 平台消息字段为 body_html/body_text：缺失 text/html 时补齐候选 */
                if flat.get("text").is_none() {
                    if let Some(v) = m.get("body_text").cloned() {
                        if let Some(obj) = flat.as_object_mut() {
                            obj.insert("text".to_string(), v);
                        }
                    }
                }
                if flat.get("html").is_none() {
                    if let Some(v) = m.get("body_html").cloned() {
                        if let Some(obj) = flat.as_object_mut() {
                            obj.insert("html".to_string(), v);
                        }
                    }
                }
                if let Some(obj) = flat.as_object_mut() {
                    obj.insert(
                        "date".to_string(),
                        m.get("received_at").cloned().unwrap_or(Value::Null),
                    );
                    obj.insert(
                        "read".to_string(),
                        m.get("read").cloned().unwrap_or(Value::Null),
                    );
                    obj.insert(
                        "sender_email".to_string(),
                        m.get("sender").cloned().unwrap_or(Value::Null),
                    );
                }
                normalize_email(&flat, em)
            })
            .collect();
        Ok(out)
    })
}

/// 拉取收件箱（GET /api/inbox，显式 Cookie）
async fn fetch_inbox(client: &wreq::Client, cookie: &str) -> Result<Value, String> {
    let resp = client
        .get(format!("{BASE_URL}/api/inbox"))
        .header("Accept", "application/json")
        .header("User-Agent", get_current_ua())
        .header("Cookie", cookie)
        .send()
        .await
        .map_err(|e| format!("nukemail 读信: 请求失败: {e}"))?;
    let status = resp.status();
    let body = resp
        .text()
        .await
        .map_err(|e| format!("nukemail 读信: 读取失败: {e}"))?;
    if !status.is_success() {
        return Err(format!("nukemail 读信: http {status}"));
    }
    serde_json::from_str(&body).map_err(|e| format!("nukemail 读信: 解析失败: {e}"))
}
