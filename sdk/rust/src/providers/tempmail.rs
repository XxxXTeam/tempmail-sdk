/*!
 * tempmail.ing 渠道实现
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::Value;

const BASE_URL: &str = "https://api.tempmail.ing/api";

pub fn generate_email(duration: u32) -> Result<EmailInfo, String> {
    block_on(async {
        let resp = http_client()
            .post(format!("{}/generate", BASE_URL))
            .header("Content-Type", "application/json")
            .header("User-Agent", get_current_ua())
            .header("Referer", "https://tempmail.ing/")
            .header("DNT", "1")
            .json(&serde_json::json!({"duration": duration}))
            .send()
            .await
            .map_err(|e| format!("tempmail request failed: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("tempmail generate failed: {}", resp.status()));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("parse failed: {}", e))?;

        if !data["success"].as_bool().unwrap_or(false) {
            return Err("Failed to generate email".into());
        }

        Ok(EmailInfo {
            channel: Channel::Tempmail,
            email: data["email"]["address"].as_str().unwrap_or("").to_string(),
            token: None,
            expires_at: data["email"]["expiresAt"].as_i64(),
            created_at: data["email"]["createdAt"].as_str().map(|s| s.to_string()),
        })
    })
}

pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let email = email.to_string();
    block_on(async {
        let resp = http_client()
            .get(format!(
                "{}/emails/{}",
                BASE_URL,
                urlencoding::encode(&email)
            ))
            .header("User-Agent", get_current_ua())
            .header("Referer", "https://tempmail.ing/")
            .header("DNT", "1")
            .send()
            .await
            .map_err(|e| format!("tempmail request failed: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("tempmail get emails failed: {}", resp.status()));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("parse failed: {}", e))?;

        if !data["success"].as_bool().unwrap_or(false) {
            return Err("Failed to get emails".into());
        }

        let emails = data["emails"]
            .as_array()
            .map(|arr| {
                arr.iter()
                    .map(|raw| {
                        let flat = flatten_message(raw, &email);
                        normalize_email(&flat, &email)
                    })
                    .collect()
            })
            .unwrap_or_default();

        Ok(emails)
    })
}

/// 将 tempmail.ing 的原始格式扁平化
/// content → html, from_address → from, received_at → date
fn flatten_message(raw: &Value, recipient: &str) -> Value {
    serde_json::json!({
        "id": raw["id"],
        "from": raw["from_address"].as_str().or_else(|| raw["from"].as_str()).unwrap_or(""),
        "to": recipient,
        "subject": raw["subject"].as_str().unwrap_or(""),
        "text": raw["text"].as_str().unwrap_or(""),
        "html": raw["content"].as_str().or_else(|| raw["html"].as_str()).unwrap_or(""),
        "date": raw["received_at"].as_str().or_else(|| raw["date"].as_str()).unwrap_or(""),
        "is_read": raw["is_read"].as_i64().unwrap_or(0) == 1,
        "attachments": raw.get("attachments").cloned().unwrap_or(Value::Array(vec![])),
    })
}
