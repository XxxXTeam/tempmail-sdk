/*!
 * mail.chatgpt.org.uk 渠道实现
 *
 * 新流程（与 npm SDK 对齐）：
 * 1. GET  /api/domains/public 获取可用域名（过滤 is_active=1）
 * 2. 本地生成随机用户名，组合邮箱地址
 * 3. POST /api/inbox-token 创建收件箱，返回 auth.token，响应头 set-cookie 提供 gm_sid
 * 4. GET  /api/emails?email=xxx 拉取邮件（Cookie: gm_sid, x-inbox-token: jwt）
 */

use rand::Rng;
use serde::{Deserialize, Serialize};
use serde_json::Value;

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};

const BASE_URL: &str = "https://mail.chatgpt.org.uk/api";
const REFERER: &str = "https://mail.chatgpt.org.uk/zh/";
const ORIGIN: &str = "https://mail.chatgpt.org.uk";

/// 打包 token：同时持久化 gm_sid 与 inbox jwt
#[derive(Debug, Serialize, Deserialize)]
struct ChatgptPackedToken {
    #[serde(rename = "gmSid")]
    gm_sid: String,
    inbox: String,
}

/// 判定是否为需要重新建立会话的鉴权错误（401 / 403）
fn is_auth_error(message: &str) -> bool {
    message.contains("401") || message.contains("403")
}

/// 生成指定长度的随机用户名（小写字母 + 数字）
fn random_username(len: usize) -> String {
    const CHARS: &[u8] = b"abcdefghijklmnopqrstuvwxyz0123456789";
    let mut rng = rand::thread_rng();
    (0..len)
        .map(|_| CHARS[rng.gen_range(0..CHARS.len())] as char)
        .collect()
}

/// 解析已打包的 token，返回 (gm_sid, inbox)
fn parse_packed_token(packed: &str) -> (String, String) {
    let t = packed.trim();
    if t.starts_with('{') {
        if let Ok(p) = serde_json::from_str::<ChatgptPackedToken>(t) {
            return (p.gm_sid, p.inbox);
        }
    }
    (String::new(), packed.to_string())
}

/// 获取可用域名列表（仅保留 is_active == 1）
fn fetch_domains() -> Result<Vec<String>, String> {
    block_on(async {
        let resp = http_client()
            .get(format!("{}/domains/public", BASE_URL))
            .header("User-Agent", get_current_ua())
            .header("Accept", "*/*")
            .header("Referer", REFERER)
            .header("Origin", ORIGIN)
            .header("DNT", "1")
            .send()
            .await
            .map_err(|e| format!("chatgpt-org-uk domains request failed: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("chatgpt-org-uk domains failed: {}", resp.status()));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("parse failed: {}", e))?;
        if !data["success"].as_bool().unwrap_or(false) {
            return Err("chatgpt-org-uk: 域名响应格式无效".into());
        }

        let domains: Vec<String> = data["data"]["domains"]
            .as_array()
            .map(|arr| {
                arr.iter()
                    .filter(|d| d["is_active"].as_i64().unwrap_or(0) == 1)
                    .filter_map(|d| d["domain_name"].as_str().map(|s| s.to_string()))
                    .collect()
            })
            .unwrap_or_default();

        if domains.is_empty() {
            return Err("chatgpt-org-uk: 无可用域名".into());
        }
        Ok(domains)
    })
}

/// 从 set-cookie 响应头提取 gm_sid
fn extract_gm_sid(resp: &wreq::Response) -> String {
    resp.headers()
        .get_all("set-cookie")
        .iter()
        .filter_map(|v| v.to_str().ok())
        .find_map(|s| {
            s.split(';').find_map(|part| {
                let part = part.trim();
                part.strip_prefix("gm_sid=").map(|v| v.to_string())
            })
        })
        .unwrap_or_default()
}

/// 创建收件箱：POST /api/inbox-token，返回 (inbox_jwt, gm_sid)
fn create_inbox(email: &str) -> Result<(String, String), String> {
    let email = email.to_string();
    block_on(async {
        let resp = http_client()
            .post(format!("{}/inbox-token", BASE_URL))
            .header("User-Agent", get_current_ua())
            .header("Accept", "*/*")
            .header("Referer", REFERER)
            .header("Origin", ORIGIN)
            .header("DNT", "1")
            .header("Content-Type", "application/json")
            .json(&serde_json::json!({ "email": email }))
            .send()
            .await
            .map_err(|e| format!("chatgpt-org-uk inbox-token request failed: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!(
                "chatgpt-org-uk inbox-token failed: {}",
                resp.status()
            ));
        }

        let gm_sid = extract_gm_sid(&resp);
        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("parse failed: {}", e))?;
        if !data["success"].as_bool().unwrap_or(false) {
            return Err("chatgpt-org-uk: inbox-token 响应无效".into());
        }
        let token = data["auth"]["token"].as_str().unwrap_or("");
        if token.is_empty() {
            return Err("chatgpt-org-uk: inbox-token 响应缺少 token".into());
        }

        Ok((token.to_string(), gm_sid))
    })
}

pub fn generate_email() -> Result<EmailInfo, String> {
    let domains = fetch_domains()?;
    let domain = {
        let mut rng = rand::thread_rng();
        &domains[rng.gen_range(0..domains.len())]
    };
    let email = format!("{}@{}", random_username(10), domain);

    let (inbox, gm_sid) = create_inbox(&email)?;

    let packed =
        serde_json::to_string(&ChatgptPackedToken { gm_sid, inbox }).map_err(|e| e.to_string())?;

    Ok(EmailInfo {
        channel: Channel::ChatgptOrgUk,
        email,
        token: Some(packed),
        expires_at: None,
        created_at: None,
    })
}

/// 拉取邮件列表
fn fetch_emails(inbox: &str, email: &str, gm_sid: &str) -> Result<Vec<Email>, String> {
    let inbox = inbox.to_string();
    let email = email.to_string();
    let gm_sid = gm_sid.to_string();
    block_on(async {
        let resp = http_client()
            .get(format!(
                "{}/emails?email={}",
                BASE_URL,
                urlencoding::encode(&email)
            ))
            .header("User-Agent", get_current_ua())
            .header("Accept", "*/*")
            .header("Referer", REFERER)
            .header("Origin", ORIGIN)
            .header("DNT", "1")
            .header("Cookie", format!("gm_sid={}", gm_sid))
            .header("x-inbox-token", inbox)
            .send()
            .await
            .map_err(|e| format!("chatgpt-org-uk request failed: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!(
                "chatgpt-org-uk get emails failed: {}",
                resp.status()
            ));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("parse failed: {}", e))?;
        if !data["success"].as_bool().unwrap_or(false) {
            return Ok(Vec::new());
        }

        Ok(data["data"]["emails"]
            .as_array()
            .map(|arr| arr.iter().map(|raw| normalize_email(raw, &email)).collect())
            .unwrap_or_default())
    })
}

pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    if token.is_empty() {
        return Err("token is required for chatgpt-org-uk".into());
    }

    let (mut gm_sid, mut inbox) = parse_packed_token(token);
    if inbox.is_empty() {
        return Err("chatgpt-org-uk: inbox token 缺失".into());
    }

    // gm_sid 丢失时重建会话
    if gm_sid.is_empty() {
        let (new_inbox, new_sid) = create_inbox(email)?;
        inbox = new_inbox;
        gm_sid = new_sid;
    }

    match fetch_emails(&inbox, email, &gm_sid) {
        Ok(emails) => Ok(emails),
        Err(err) => {
            if is_auth_error(&err) {
                let (new_inbox, new_sid) = create_inbox(email)?;
                return fetch_emails(&new_inbox, email, &new_sid);
            }
            Err(err)
        }
    }
}
