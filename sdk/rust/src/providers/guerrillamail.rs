/*!
 * guerrillamail.com 渠道实现
 */

use serde_json::Value;
use crate::types::{Channel, EmailInfo, Email};
use crate::normalize::normalize_email;
use crate::config::http_client;

const BASE_URL: &str = "https://api.guerrillamail.com/ajax.php";

pub fn generate_email() -> Result<EmailInfo, String> {
    let resp = http_client()
        .get(format!("{}?f=get_email_address&lang=en", BASE_URL))
        .send().map_err(|e| format!("guerrillamail request failed: {}", e))?;

    if !resp.status().is_success() {
        return Err(format!("guerrillamail generate failed: {}", resp.status()));
    }

    let data: Value = resp.json().map_err(|e| format!("parse failed: {}", e))?;
    let email_addr = data["email_addr"].as_str().unwrap_or("");
    let sid_token = data["sid_token"].as_str().unwrap_or("");

    if email_addr.is_empty() || sid_token.is_empty() {
        return Err("Failed to generate email: missing email_addr or sid_token".into());
    }

    Ok(EmailInfo {
        channel: Channel::GuerrillaMail,
        email: email_addr.to_string(),
        token: Some(sid_token.to_string()),
        expires_at: data["email_timestamp"].as_i64().map(|ts| (ts + 3600) * 1000),
        created_at: None,
    })
}

pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let resp = http_client()
        .get(format!("{}?f=check_email&seq=0&sid_token={}", BASE_URL, urlencoding::encode(token)))
        .send().map_err(|e| format!("guerrillamail request failed: {}", e))?;

    if !resp.status().is_success() {
        return Err(format!("guerrillamail get emails failed: {}", resp.status()));
    }

    let data: Value = resp.json().map_err(|e| format!("parse failed: {}", e))?;
    let list = data["list"].as_array().cloned().unwrap_or_default();

    Ok(list.iter().map(|item| {
        let flat = serde_json::json!({
            "id": item["mail_id"],
            "from": item["mail_from"],
            "to": email,
            "subject": item["mail_subject"],
            "text": item.get("mail_body").or(item.get("mail_excerpt")).unwrap_or(&Value::String(String::new())),
            "html": item["mail_body"].as_str().unwrap_or(""),
            "date": item["mail_date"].as_str().unwrap_or(""),
            "isRead": item["mail_read"].as_i64() == Some(1),
        });
        normalize_email(&flat, email)
    }).collect())
}
