/*!
 * tempmail.lol V2 渠道实现
 * API: https://api.tempmail.lol
 */

use serde_json::Value;
use crate::types::{Channel, EmailInfo, Email};
use crate::normalize::normalize_email;
use crate::config::{http_client, block_on, get_current_ua};

const API_BASE: &str = "https://api.tempmail.lol";

pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let resp = http_client()
            .get(format!("{}/generate", API_BASE))
            .header("User-Agent", get_current_ua())
            .header("Accept", "application/json")
            .send().await.map_err(|e| format!("tempmail-lol-v2 request failed: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("tempmail-lol-v2 generate failed: {}", resp.status()));
        }

        let data: Value = resp.json().await.map_err(|e| format!("parse failed: {}", e))?;
        let address = data["address"].as_str().unwrap_or("");
        let token = data["token"].as_str().unwrap_or("");

        if address.is_empty() || token.is_empty() {
            return Err("tempmail-lol-v2: missing address or token".into());
        }

        Ok(EmailInfo {
            channel: Channel::TempmailLolV2,
            email: address.to_string(),
            token: Some(token.to_string()),
            expires_at: None,
            created_at: None,
        })
    })
}

pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let token = token.to_string();
    let email = email.to_string();
    block_on(async {
        let resp = http_client()
            .get(format!("{}/auth/{}", API_BASE, urlencoding::encode(&token)))
            .header("User-Agent", get_current_ua())
            .header("Accept", "application/json")
            .send().await.map_err(|e| format!("tempmail-lol-v2 request failed: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("tempmail-lol-v2 get emails failed: {}", resp.status()));
        }

        let data: Value = resp.json().await.map_err(|e| format!("parse failed: {}", e))?;
        let list = data["email"].as_array().cloned().unwrap_or_default();

        let mut out = Vec::with_capacity(list.len());
        for raw in &list {
            let from = raw["from"].as_str()
                .or_else(|| raw["sender"].as_str())
                .unwrap_or("");
            let flat = serde_json::json!({
                "id": raw["id"].as_str().or_else(|| raw["_id"].as_str()).unwrap_or(""),
                "from": from,
                "to": &email,
                "subject": raw["subject"].as_str().unwrap_or(""),
                "text": raw["body"].as_str().or_else(|| raw["text"].as_str()).unwrap_or(""),
                "html": raw["html"].as_str().or_else(|| raw["body"].as_str()).unwrap_or(""),
                "date": raw["date"].as_str().or_else(|| raw["receivedAt"].as_str()).unwrap_or(""),
                "isRead": false,
            });
            out.push(normalize_email(&flat, &email));
        }
        Ok(out)
    })
}
