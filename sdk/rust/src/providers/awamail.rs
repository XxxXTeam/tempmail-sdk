/*!
 * awamail.com 渠道实现
 * 需要保存 session cookie
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::Value;

const BASE_URL: &str = "https://awamail.com/welcome";

pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let resp = http_client()
            .post(format!("{}/change_mailbox", BASE_URL))
            .header("Content-Length", "0")
            .header("User-Agent", get_current_ua())
            .header("dnt", "1")
            .header("origin", "https://awamail.com")
            .header("referer", "https://awamail.com/?lang=zh")
            .send()
            .await
            .map_err(|e| format!("awamail request failed: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("awamail generate failed: {}", resp.status()));
        }

        /* 从 Set-Cookie 提取 session */
        let session_cookie = resp
            .headers()
            .get_all("set-cookie")
            .iter()
            .filter_map(|v| v.to_str().ok())
            .find(|s| s.contains("awamail_session="))
            .and_then(|s| {
                s.split(';')
                    .next()
                    .filter(|p| p.starts_with("awamail_session="))
                    .map(|p| p.to_string())
            })
            .ok_or("Failed to extract session cookie")?;

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("parse failed: {}", e))?;
        if !data["success"].as_bool().unwrap_or(false) {
            return Err("Failed to generate email".into());
        }

        Ok(EmailInfo {
            channel: Channel::Awamail,
            email: data["data"]["email_address"]
                .as_str()
                .unwrap_or("")
                .to_string(),
            token: Some(session_cookie),
            expires_at: data["data"]["expired_at"].as_i64(),
            created_at: data["data"]["created_at"].as_str().map(|s| s.to_string()),
        })
    })
}

pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let token = token.to_string();
    let email = email.to_string();
    block_on(async {
        let resp = http_client()
            .get(format!("{}/get_emails", BASE_URL))
            .header("User-Agent", get_current_ua())
            .header("Cookie", &token)
            .header("x-requested-with", "XMLHttpRequest")
            .header("dnt", "1")
            .header("origin", "https://awamail.com")
            .header("referer", "https://awamail.com/?lang=zh")
            .send()
            .await
            .map_err(|e| format!("awamail request failed: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("awamail get emails failed: {}", resp.status()));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("parse failed: {}", e))?;
        if !data["success"].as_bool().unwrap_or(false) {
            return Err("Failed to get emails".into());
        }

        Ok(data["data"]["emails"]
            .as_array()
            .map(|arr| arr.iter().map(|raw| normalize_email(raw, &email)).collect())
            .unwrap_or_default())
    })
}
