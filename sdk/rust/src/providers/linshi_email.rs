/*!
 * linshi-email.com 渠道实现
 */

use serde_json::Value;
use crate::types::{Channel, EmailInfo, Email};
use crate::normalize::normalize_email;
use crate::config::{http_client, block_on, get_current_ua};
use super::linshi_token::random_synthetic_linshi_api_path_key;

const BASE_URL: &str = "https://www.linshi-email.com/api/v1";

fn parse_email_from_data(data: &Value) -> String {
    let d = data.as_object();
    let raw = d
        .and_then(|o| o.get("email").and_then(|v| v.as_str()))
        .or_else(|| d.and_then(|o| o.get("mail").and_then(|v| v.as_str())))
        .or_else(|| d.and_then(|o| o.get("address").and_then(|v| v.as_str())))
        .unwrap_or("");
    let s = raw.trim();
    if s.contains('@') {
        s.to_string()
    } else {
        String::new()
    }
}

pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let api_key = random_synthetic_linshi_api_path_key();
        let resp = http_client()
            .post(format!("{}/email/{}", BASE_URL, api_key))
            .header("Content-Type", "application/json")
            .header("User-Agent", get_current_ua())
            .header("Origin", "https://www.linshi-email.com")
            .header("Referer", "https://www.linshi-email.com/")
            .header("DNT", "1")
            .json(&serde_json::json!({}))
            .send().await.map_err(|e| format!("linshi-email request failed: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("linshi-email generate failed: {}", resp.status()));
        }

        let data: Value = resp.json().await.map_err(|e| format!("parse failed: {}", e))?;
        if data["status"].as_str() != Some("ok") {
            return Err("Failed to generate email".into());
        }

        let email = parse_email_from_data(&data["data"]);
        if email.is_empty() {
            return Err(
                "linshi-email: API 未返回有效邮箱（可能触发频率限制）".into(),
            );
        }

        Ok(EmailInfo {
            channel: Channel::LinshiEmail,
            email,
            token: Some(api_key),
            expires_at: data["data"]["expired"].as_i64(),
            created_at: None,
        })
    })
}

pub fn get_emails(api_path_key: &str, email: &str) -> Result<Vec<Email>, String> {
    let email = email.to_string();
    let key = api_path_key.to_string();
    block_on(async {
        if key.is_empty() {
            return Err("api path key missing for linshi-email".into());
        }
        let ts = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH).unwrap().as_millis();
        let resp = http_client()
            .get(format!("{}/refreshmessage/{}/{}?t={}", BASE_URL, key, urlencoding::encode(&email), ts))
            .header("User-Agent", get_current_ua())
            .header("Origin", "https://www.linshi-email.com")
            .header("DNT", "1")
            .send().await.map_err(|e| format!("linshi-email request failed: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("linshi-email get emails failed: {}", resp.status()));
        }

        let data: Value = resp.json().await.map_err(|e| format!("parse failed: {}", e))?;
        if data["status"].as_str() != Some("ok") {
            return Err("Failed to get emails".into());
        }

        Ok(data["list"].as_array()
            .map(|arr| arr.iter().map(|raw| normalize_email(raw, &email)).collect())
            .unwrap_or_default())
    })
}
