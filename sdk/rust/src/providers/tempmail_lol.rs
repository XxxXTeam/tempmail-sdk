/*!
 * tempmail.lol 渠道实现
 */

use reqwest::blocking::Client;
use serde_json::Value;
use crate::types::{Channel, EmailInfo, Email};
use crate::normalize::normalize_email;

const BASE_URL: &str = "https://api.tempmail.lol/v2";

fn client() -> Client {
    Client::builder()
        .user_agent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36")
        .timeout(std::time::Duration::from_secs(15))
        .build().unwrap()
}

pub fn generate_email(domain: Option<&str>) -> Result<EmailInfo, String> {
    let resp = client()
        .post(format!("{}/inbox/create", BASE_URL))
        .header("Content-Type", "application/json")
        .header("Origin", "https://tempmail.lol")
        .header("sec-ch-ua", "\"Microsoft Edge\";v=\"143\", \"Chromium\";v=\"143\", \"Not A(Brand\";v=\"24\"")
        .header("sec-ch-ua-mobile", "?0")
        .header("sec-ch-ua-platform", "\"Windows\"")
        .header("DNT", "1")
        .json(&serde_json::json!({"domain": domain, "captcha": null}))
        .send().map_err(|e| format!("tempmail-lol request failed: {}", e))?;

    if !resp.status().is_success() {
        return Err(format!("tempmail-lol generate failed: {}", resp.status()));
    }

    let data: Value = resp.json().map_err(|e| format!("parse failed: {}", e))?;
    let address = data["address"].as_str().unwrap_or("");
    let token = data["token"].as_str().unwrap_or("");
    if address.is_empty() || token.is_empty() {
        return Err("Failed to generate email".into());
    }

    Ok(EmailInfo {
        channel: Channel::TempmailLol,
        email: address.to_string(),
        token: Some(token.to_string()),
        expires_at: None,
        created_at: None,
    })
}

pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let resp = client()
        .get(format!("{}/inbox?token={}", BASE_URL, urlencoding::encode(token)))
        .header("Origin", "https://tempmail.lol")
        .header("DNT", "1")
        .send().map_err(|e| format!("tempmail-lol request failed: {}", e))?;

    if !resp.status().is_success() {
        return Err(format!("tempmail-lol get emails failed: {}", resp.status()));
    }

    let data: Value = resp.json().map_err(|e| format!("parse failed: {}", e))?;
    Ok(data["emails"].as_array()
        .map(|arr| arr.iter().map(|raw| normalize_email(raw, email)).collect())
        .unwrap_or_default())
}
