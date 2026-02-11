/*!
 * temp-mail.io 渠道实现
 * 需要动态获取 X-CORS-Header
 */

use reqwest::blocking::Client;
use serde_json::Value;
use std::sync::Mutex;
use crate::types::{Channel, EmailInfo, Email};
use crate::normalize::normalize_email;

const BASE_URL: &str = "https://api.internal.temp-mail.io/api/v3";
const PAGE_URL: &str = "https://temp-mail.io/en";

static CACHED_CORS: Mutex<Option<String>> = Mutex::new(None);

fn fetch_cors_header() -> String {
    if let Ok(guard) = CACHED_CORS.lock() {
        if let Some(ref cached) = *guard {
            return cached.clone();
        }
    }

    let result = (|| -> Option<String> {
        let resp = Client::builder()
            .user_agent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36")
            .timeout(std::time::Duration::from_secs(10))
            .build().ok()?
            .get(PAGE_URL).send().ok()?;
        let html = resp.text().ok()?;
        let re = regex_lite::Regex::new(r#"mobileTestingHeader\s*:\s*"([^"]+)""#).ok()?;
        let caps = re.captures(&html)?;
        Some(caps.get(1)?.as_str().to_string())
    })().unwrap_or_else(|| "1".to_string());

    if let Ok(mut guard) = CACHED_CORS.lock() {
        *guard = Some(result.clone());
    }
    result
}

fn api_client() -> Client {
    Client::builder()
        .user_agent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36 Edg/144.0.0.0")
        .timeout(std::time::Duration::from_secs(15))
        .build().unwrap()
}

pub fn generate_email() -> Result<EmailInfo, String> {
    let cors = fetch_cors_header();
    let resp = api_client()
        .post(format!("{}/email/new", BASE_URL))
        .header("Content-Type", "application/json")
        .header("Application-Name", "web")
        .header("Application-Version", "4.0.0")
        .header("X-CORS-Header", &cors)
        .header("origin", "https://temp-mail.io")
        .json(&serde_json::json!({"min_name_length": 10, "max_name_length": 10}))
        .send().map_err(|e| format!("temp-mail-io request failed: {}", e))?;

    if !resp.status().is_success() {
        return Err(format!("temp-mail-io generate failed: {}", resp.status()));
    }

    let data: Value = resp.json().map_err(|e| format!("parse failed: {}", e))?;
    let email = data["email"].as_str().unwrap_or("");
    let token = data["token"].as_str().unwrap_or("");
    if email.is_empty() || token.is_empty() {
        return Err("Failed to generate email".into());
    }

    Ok(EmailInfo {
        channel: Channel::TempMailIO,
        email: email.to_string(),
        token: Some(token.to_string()),
        expires_at: None, created_at: None,
    })
}

pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let cors = fetch_cors_header();
    let resp = api_client()
        .get(format!("{}/email/{}/messages", BASE_URL, email))
        .header("Application-Name", "web")
        .header("Application-Version", "4.0.0")
        .header("X-CORS-Header", &cors)
        .header("origin", "https://temp-mail.io")
        .send().map_err(|e| format!("temp-mail-io request failed: {}", e))?;

    if !resp.status().is_success() {
        return Err(format!("temp-mail-io get emails failed: {}", resp.status()));
    }

    let data: Value = resp.json().map_err(|e| format!("parse failed: {}", e))?;
    let arr = data.as_array().cloned().unwrap_or_default();
    Ok(arr.iter().map(|raw| normalize_email(raw, email)).collect())
}
