/*!
 * tempmail.ing 渠道实现
 */

use serde_json::Value;
use crate::types::{Channel, EmailInfo, Email};
use crate::normalize::normalize_email;
use crate::config::http_client;

const BASE_URL: &str = "https://api.tempmail.ing/api";

pub fn generate_email(duration: u32) -> Result<EmailInfo, String> {
    let resp = http_client()
        .post(format!("{}/generate", BASE_URL))
        .header("Content-Type", "application/json")
        .header("Referer", "https://tempmail.ing/")
        .header("sec-ch-ua", "\"Microsoft Edge\";v=\"143\", \"Chromium\";v=\"143\", \"Not A(Brand\";v=\"24\"")
        .header("sec-ch-ua-mobile", "?0")
        .header("sec-ch-ua-platform", "\"Windows\"")
        .header("DNT", "1")
        .json(&serde_json::json!({"duration": duration}))
        .send()
        .map_err(|e| format!("tempmail request failed: {}", e))?;

    if !resp.status().is_success() {
        return Err(format!("tempmail generate failed: {}", resp.status()));
    }

    let data: Value = resp.json().map_err(|e| format!("parse failed: {}", e))?;

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
}

pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let resp = http_client()
        .get(format!("{}/emails/{}", BASE_URL, urlencoding::encode(email)))
        .header("Referer", "https://tempmail.ing/")
        .header("DNT", "1")
        .send()
        .map_err(|e| format!("tempmail request failed: {}", e))?;

    if !resp.status().is_success() {
        return Err(format!("tempmail get emails failed: {}", resp.status()));
    }

    let data: Value = resp.json().map_err(|e| format!("parse failed: {}", e))?;

    if !data["success"].as_bool().unwrap_or(false) {
        return Err("Failed to get emails".into());
    }

    let emails = data["emails"].as_array()
        .map(|arr| arr.iter().map(|raw| normalize_email(raw, email)).collect())
        .unwrap_or_default();

    Ok(emails)
}
