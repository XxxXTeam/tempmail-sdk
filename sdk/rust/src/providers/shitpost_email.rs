/*!
 * ShitpostEmail 渠道实现（shitpost.email 公共实例）
 *
 * 无认证 REST：
 *   建箱: POST /api/create（body {username,domain,ttl}），响应 email/token/type/expires
 *   读信: GET /api/inbox?email=&token=（messages[] 含 from/fromName/subject/text/html/date）
 * 域名池: shitpost.email / letsfuckingpiss.party
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use rand::RngExt;
use serde_json::{json, Value};

const BASE_URL: &str = "https://shitpost.email";

const DOMAINS: [&str; 2] = ["shitpost.email", "letsfuckingpiss.party"];

/// 生成 "sdk"+10 位随机本地名
fn random_local() -> String {
    const CHARS: &[u8] = b"abcdefghijklmnopqrstuvwxyz0123456789";
    let mut rng = rand::rng();
    let suffix: String = (0..10)
        .map(|_| CHARS[rng.random_range(0..CHARS.len())] as char)
        .collect();
    format!("sdk{suffix}")
}

/// 设置 shitpost.email 请求通用请求头
fn set_headers(req: wreq::RequestBuilder) -> wreq::RequestBuilder {
    req.header("User-Agent", get_current_ua())
        .header("Accept", "application/json")
}

/// 创建临时邮箱
/// POST /api/create body {username,domain,ttl}
pub fn generate_email() -> Result<EmailInfo, String> {
    let dom = DOMAINS[rand::rng().random_range(0..DOMAINS.len())];
    let body = json!({
        "username": random_local(),
        "domain": dom,
        "ttl": 3600,
    });

    block_on(async {
        let resp = set_headers(
            http_client()
                .post(format!("{BASE_URL}/api/create"))
                .header("Content-Type", "application/json")
                .body(body.to_string()),
        )
        .send()
        .await
        .map_err(|e| format!("shitpost-email: 创建邮箱请求失败: {e}"))?;

        if !resp.status().is_success() {
            return Err(format!("shitpost-email create: http {}", resp.status()));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("shitpost-email: 解析创建响应失败: {e}"))?;

        let email = data
            .get("email")
            .and_then(|v| v.as_str())
            .unwrap_or("")
            .trim()
            .to_string();
        let token = data
            .get("token")
            .and_then(|v| v.as_str())
            .unwrap_or("")
            .trim()
            .to_string();
        if email.is_empty() || token.is_empty() {
            return Err("shitpost-email create: missing email or token".into());
        }

        /* expires 为秒级时间戳，转为毫秒 */
        let expires_at = data
            .get("expires")
            .and_then(|v| v.as_f64())
            .filter(|n| *n > 0.0)
            .map(|n| (n as i64) * 1000);

        Ok(EmailInfo {
            channel: Channel::ShitpostEmail,
            email,
            token: Some(token),
            expires_at,
            created_at: None,
        })
    })
}

/// 读取收件箱
/// GET /api/inbox?email=&token=，messages[] 按 from/text/html/date 映射归一化
pub fn get_emails(email: &str, token: &str) -> Result<Vec<Email>, String> {
    let em = email.trim();
    let tok = token.trim();
    if em.is_empty() || tok.is_empty() {
        return Err("shitpost-email: 邮箱或 token 为空".into());
    }

    let url = format!(
        "{BASE_URL}/api/inbox?email={}&token={}",
        urlencoding::encode(em),
        urlencoding::encode(tok)
    );

    block_on(async {
        let resp = set_headers(http_client().get(&url))
            .send()
            .await
            .map_err(|e| format!("shitpost-email: 获取收件箱失败: {e}"))?;

        if !resp.status().is_success() {
            return Err(format!("shitpost-email inbox: http {}", resp.status()));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("shitpost-email: 解析收件箱响应失败: {e}"))?;

        let messages = data
            .get("messages")
            .and_then(|v| v.as_array())
            .cloned()
            .unwrap_or_default();

        let mut out = Vec::with_capacity(messages.len());
        for m in &messages {
            let flat = json!({
                "id": m.get("id").and_then(|v| v.as_str()).unwrap_or(""),
                "from": m.get("from").and_then(|v| v.as_str()).unwrap_or(""),
                "to": em,
                "subject": m.get("subject").and_then(|v| v.as_str()).unwrap_or(""),
                "text": m.get("text").and_then(|v| v.as_str()).unwrap_or(""),
                "html": m.get("html").and_then(|v| v.as_str()).unwrap_or(""),
                "date": m.get("date").and_then(|v| v.as_str()).unwrap_or(""),
            });
            out.push(normalize_email(&flat, em));
        }
        Ok(out)
    })
}
