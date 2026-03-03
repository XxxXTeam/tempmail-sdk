/*!
 * Emailnator 渠道实现
 * 网站: https://www.emailnator.com
 * 需要 XSRF-TOKEN Session 认证
 */

use serde_json::Value;
use crate::types::{Channel, EmailInfo, Email};
use crate::normalize::normalize_email;
use crate::config::http_client;

const BASE_URL: &str = "https://www.emailnator.com";

fn init_session() -> Result<(String, String), String> {
    let resp = http_client()
        .get(BASE_URL)
        .header("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36")
        .send().map_err(|e| format!("emailnator init session failed: {}", e))?;

    let mut xsrf_token = String::new();
    let mut cookie_parts: Vec<String> = Vec::new();

    /* 从 Set-Cookie 响应头中手动提取 cookie */
    for value in resp.headers().get_all("set-cookie") {
        let cookie_str = value.to_str().unwrap_or("");
        if let Some(name_value) = cookie_str.split(';').next() {
            cookie_parts.push(name_value.to_string());
            if name_value.starts_with("XSRF-TOKEN=") {
                let raw = &name_value["XSRF-TOKEN=".len()..];
                xsrf_token = urlencoding::decode(raw)
                    .unwrap_or_default().to_string();
            }
        }
    }

    if xsrf_token.is_empty() {
        return Err("Failed to extract XSRF-TOKEN".into());
    }

    Ok((xsrf_token, cookie_parts.join("; ")))
}

pub fn generate_email() -> Result<EmailInfo, String> {
    let (xsrf_token, cookies) = init_session()?;

    let resp = http_client()
        .post(format!("{}/generate-email", BASE_URL))
        .header("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36")
        .header("Content-Type", "application/json")
        .header("Accept", "application/json")
        .header("X-Requested-With", "XMLHttpRequest")
        .header("X-XSRF-TOKEN", &xsrf_token)
        .header("Cookie", &cookies)
        .json(&serde_json::json!({"email": ["domain"]}))
        .send().map_err(|e| format!("emailnator generate failed: {}", e))?;

    if !resp.status().is_success() {
        return Err(format!("emailnator generate failed: {}", resp.status()));
    }

    let data: Value = resp.json().map_err(|e| format!("parse failed: {}", e))?;
    let email_list = data["email"].as_array()
        .ok_or("emailnator: empty email response")?;

    if email_list.is_empty() {
        return Err("emailnator: empty email list".into());
    }

    let email = email_list[0].as_str().unwrap_or("").to_string();
    let token_payload = serde_json::json!({
        "xsrfToken": xsrf_token,
        "cookies": cookies,
    }).to_string();

    Ok(EmailInfo {
        channel: Channel::Emailnator,
        email,
        token: Some(token_payload),
        expires_at: None,
        created_at: None,
    })
}

pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let session: Value = serde_json::from_str(token)
        .map_err(|e| format!("parse session token failed: {}", e))?;

    let xsrf_token = session["xsrfToken"].as_str().unwrap_or("");
    let cookies = session["cookies"].as_str().unwrap_or("");

    let resp = http_client()
        .post(format!("{}/message-list", BASE_URL))
        .header("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36")
        .header("Content-Type", "application/json")
        .header("Accept", "application/json")
        .header("X-Requested-With", "XMLHttpRequest")
        .header("X-XSRF-TOKEN", xsrf_token)
        .header("Cookie", cookies)
        .json(&serde_json::json!({"email": email}))
        .send().map_err(|e| format!("emailnator get emails failed: {}", e))?;

    if !resp.status().is_success() {
        return Err(format!("emailnator get emails failed: {}", resp.status()));
    }

    let data: Value = resp.json().map_err(|e| format!("parse failed: {}", e))?;
    let message_data = data["messageData"].as_array().cloned().unwrap_or_default();

    let mut result = Vec::new();
    for msg in &message_data {
        let msg_id = msg["messageID"].as_str().unwrap_or("");
        if msg_id.starts_with("ADS") {
            continue;
        }
        let raw = serde_json::json!({
            "id": msg_id,
            "from": msg["from"].as_str().unwrap_or(""),
            "to": email,
            "subject": msg["subject"].as_str().unwrap_or(""),
            "text": "",
            "html": "",
            "date": msg["time"].as_str().unwrap_or(""),
            "isRead": false,
            "attachments": [],
        });
        result.push(normalize_email(&raw, email));
    }

    Ok(result)
}
