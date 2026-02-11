/*!
 * linshi-email.com 渠道实现
 */

use reqwest::blocking::Client;
use serde_json::Value;
use crate::types::{Channel, EmailInfo, Email};
use crate::normalize::normalize_email;

const BASE_URL: &str = "https://www.linshi-email.com/api/v1";
const API_KEY: &str = "552562b8524879814776e52bc8de5c9f";

fn client() -> Client {
    Client::builder()
        .user_agent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36")
        .timeout(std::time::Duration::from_secs(15))
        .build().unwrap()
}

pub fn generate_email() -> Result<EmailInfo, String> {
    let resp = client()
        .post(format!("{}/email/{}", BASE_URL, API_KEY))
        .header("Content-Type", "application/json")
        .header("Origin", "https://www.linshi-email.com")
        .header("Referer", "https://www.linshi-email.com/")
        .header("sec-ch-ua", "\"Microsoft Edge\";v=\"143\", \"Chromium\";v=\"143\", \"Not A(Brand\";v=\"24\"")
        .header("sec-ch-ua-mobile", "?0")
        .header("sec-ch-ua-platform", "\"Windows\"")
        .header("DNT", "1")
        .json(&serde_json::json!({}))
        .send().map_err(|e| format!("linshi-email request failed: {}", e))?;

    if !resp.status().is_success() {
        return Err(format!("linshi-email generate failed: {}", resp.status()));
    }

    let data: Value = resp.json().map_err(|e| format!("parse failed: {}", e))?;
    if data["status"].as_str() != Some("ok") {
        return Err("Failed to generate email".into());
    }

    Ok(EmailInfo {
        channel: Channel::LinshiEmail,
        email: data["data"]["email"].as_str().unwrap_or("").to_string(),
        token: None,
        expires_at: data["data"]["expired"].as_i64(),
        created_at: None,
    })
}

pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let ts = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH).unwrap().as_millis();
    let resp = client()
        .get(format!("{}/refreshmessage/{}/{}?t={}", BASE_URL, API_KEY, urlencoding::encode(email), ts))
        .header("Origin", "https://www.linshi-email.com")
        .header("DNT", "1")
        .send().map_err(|e| format!("linshi-email request failed: {}", e))?;

    if !resp.status().is_success() {
        return Err(format!("linshi-email get emails failed: {}", resp.status()));
    }

    let data: Value = resp.json().map_err(|e| format!("parse failed: {}", e))?;
    if data["status"].as_str() != Some("ok") {
        return Err("Failed to get emails".into());
    }

    Ok(data["list"].as_array()
        .map(|arr| arr.iter().map(|raw| normalize_email(raw, email)).collect())
        .unwrap_or_default())
}
