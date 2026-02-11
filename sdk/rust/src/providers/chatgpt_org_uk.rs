/*!
 * mail.chatgpt.org.uk 渠道实现
 */

use serde_json::Value;
use crate::types::{Channel, EmailInfo, Email};
use crate::normalize::normalize_email;
use crate::config::http_client;

const BASE_URL: &str = "https://mail.chatgpt.org.uk/api";

pub fn generate_email() -> Result<EmailInfo, String> {
    let resp = http_client()
        .get(format!("{}/generate-email", BASE_URL))
        .header("Referer", "https://mail.chatgpt.org.uk/")
        .header("sec-ch-ua", "\"Microsoft Edge\";v=\"143\", \"Chromium\";v=\"143\", \"Not A(Brand\";v=\"24\"")
        .header("sec-ch-ua-mobile", "?0")
        .header("sec-ch-ua-platform", "\"Windows\"")
        .header("DNT", "1")
        .send().map_err(|e| format!("chatgpt-org-uk request failed: {}", e))?;

    if !resp.status().is_success() {
        return Err(format!("chatgpt-org-uk generate failed: {}", resp.status()));
    }

    let data: Value = resp.json().map_err(|e| format!("parse failed: {}", e))?;
    if !data["success"].as_bool().unwrap_or(false) {
        return Err("Failed to generate email".into());
    }

    Ok(EmailInfo {
        channel: Channel::ChatgptOrgUk,
        email: data["data"]["email"].as_str().unwrap_or("").to_string(),
        token: None, expires_at: None, created_at: None,
    })
}

pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let resp = http_client()
        .get(format!("{}/emails?email={}", BASE_URL, urlencoding::encode(email)))
        .header("Referer", "https://mail.chatgpt.org.uk/")
        .header("DNT", "1")
        .send().map_err(|e| format!("chatgpt-org-uk request failed: {}", e))?;

    if !resp.status().is_success() {
        return Err(format!("chatgpt-org-uk get emails failed: {}", resp.status()));
    }

    let data: Value = resp.json().map_err(|e| format!("parse failed: {}", e))?;
    if !data["success"].as_bool().unwrap_or(false) {
        return Err("Failed to get emails".into());
    }

    Ok(data["data"]["emails"].as_array()
        .map(|arr| arr.iter().map(|raw| normalize_email(raw, email)).collect())
        .unwrap_or_default())
}
