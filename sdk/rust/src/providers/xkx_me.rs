/*!
 * XKX.me — https://xkx.me
 * 流程：GET / 获取 CSRF + cookie → POST /mailbox/create/random 创建随机邮箱
 * 收信：GET /mailbox/{email}/messages (JSON)
 */

use crate::config::{block_on, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use regex::Regex;
use serde_json::{json, Value};

const BASE_URL: &str = "https://xkx.me";

/// 从首页 HTML 提取 CSRF token
fn extract_csrf(html: &str) -> Result<String, String> {
    let re = Regex::new(r#"csrf-token" content="([^"]+)""#).unwrap();
    let caps = re.captures(html).ok_or("xkx-me: 无法获取 CSRF token")?;
    Ok(caps[1].to_string())
}

/// 从响应头提取 Set-Cookie
fn extract_cookies(headers: &wreq::header::HeaderMap) -> String {
    headers
        .get_all("set-cookie")
        .iter()
        .filter_map(|v| v.to_str().ok())
        .map(|s| s.split(';').next().unwrap_or("").trim().to_string())
        .filter(|s| !s.is_empty())
        .collect::<Vec<_>>()
        .join("; ")
}

/// 创建 xkx.me 临时邮箱
pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let client = http_client();

        // 获取首页 CSRF + cookie
        let resp = client
            .get(BASE_URL)
            .header("Accept", "text/html")
            .header(
                "User-Agent",
                "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
            )
            .send()
            .await
            .map_err(|e| format!("xkx-me: 获取首页失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("xkx-me: 获取首页失败 HTTP {}", resp.status()));
        }

        let cookies = extract_cookies(resp.headers());
        let html = resp.text().await.map_err(|e| e.to_string())?;
        let csrf = extract_csrf(&html)?;

        // 创建邮箱
        let create_resp = client
            .post(format!("{}/mailbox/create/random", BASE_URL))
            .header("Content-Type", "application/x-www-form-urlencoded")
            .header("Cookie", &cookies)
            .header(
                "User-Agent",
                "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
            )
            .body(format!("_token={}", urlencoding::encode(&csrf)))
            .send()
            .await
            .map_err(|e| format!("xkx-me: 创建邮箱失败: {}", e))?;

        // 从 Location 或 body 中提取邮箱地址
        let location = create_resp
            .headers()
            .get("location")
            .and_then(|v| v.to_str().ok())
            .unwrap_or("")
            .to_string();

        let source = if location.is_empty() {
            create_resp.text().await.unwrap_or_default()
        } else {
            location
        };

        let email_re = Regex::new(r#"mailbox/([^\s/"'<>]+@xkx\.me)"#).unwrap();
        let caps = email_re
            .captures(&source)
            .ok_or("xkx-me: 无法从响应中提取邮箱地址")?;

        Ok(EmailInfo {
            channel: Channel::XkxMe,
            email: caps[1].to_string(),
            token: Some(cookies),
            expires_at: None,
            created_at: None,
        })
    })
}

/// 获取 xkx.me 邮件列表
pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let address = email.trim();
    if address.is_empty() {
        return Err("xkx-me: 邮箱地址为空".into());
    }

    block_on(async {
        let client = http_client();
        let url = format!(
            "{}/mailbox/{}/messages",
            BASE_URL,
            urlencoding::encode(address)
        );

        let resp = client
            .get(&url)
            .header("Accept", "application/json")
            .header("X-Requested-With", "XMLHttpRequest")
            .header("Cookie", token)
            .header(
                "User-Agent",
                "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
            )
            .send()
            .await
            .map_err(|e| format!("xkx-me: 获取邮件失败: {}", e))?;

        if resp.status().as_u16() == 404 {
            return Ok(Vec::new());
        }
        if !resp.status().is_success() {
            return Err(format!("xkx-me: 获取邮件失败 HTTP {}", resp.status()));
        }

        let body = resp.text().await.map_err(|e| e.to_string())?;

        let messages: Vec<serde_json::Map<String, Value>> =
            serde_json::from_str(&body).unwrap_or_else(|_| {
                #[derive(serde::Deserialize)]
                struct Wrapper {
                    #[serde(default)]
                    messages: Vec<serde_json::Map<String, Value>>,
                }
                serde_json::from_str::<Wrapper>(&body)
                    .map(|w| w.messages)
                    .unwrap_or_default()
            });

        if messages.is_empty() {
            return Ok(Vec::new());
        }

        let mut out = Vec::with_capacity(messages.len());
        for msg in &messages {
            let html = msg
                .get("html")
                .or_else(|| msg.get("body"))
                .and_then(|v| v.as_str())
                .unwrap_or("");
            let raw: Value = json!({
                "id": msg.get("id"),
                "from": msg.get("from"),
                "to": address,
                "subject": msg.get("subject"),
                "date": msg.get("date"),
                "html": html,
                "text": msg.get("text").and_then(|v| v.as_str()).unwrap_or(""),
            });
            out.push(normalize_email(&raw, address));
        }
        Ok(out)
    })
}
