/*!
 * ta-easy.com 临时邮箱
 * API: https://api-endpoint.ta-easy.com/temp-email/
 */

use serde_json::Value;
use crate::types::{Channel, EmailInfo, Email};
use crate::normalize::normalize_email;
use crate::config::{http_client, block_on, get_current_ua};

const API_BASE: &str = "https://api-endpoint.ta-easy.com";
const ORIGIN: &str = "https://www.ta-easy.com";

pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let resp = http_client()
            .post(format!("{API_BASE}/temp-email/address/new"))
            .header("Accept", "application/json")
            .header("User-Agent", get_current_ua())
            .header("origin", ORIGIN)
            .header("referer", format!("{ORIGIN}/"))
            .header("Content-Length", "0")
            .send()
            .await
            .map_err(|e| format!("ta-easy request failed: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("ta-easy generate failed: {}", resp.status()));
        }

        let data: Value = resp.json().await.map_err(|e| format!("parse failed: {}", e))?;
        if data["status"].as_str() != Some("success") {
            let msg = data["message"].as_str().unwrap_or("create failed");
            return Err(format!("ta-easy: {}", msg));
        }
        let addr = data["address"].as_str().unwrap_or("");
        let token = data["token"].as_str().unwrap_or("");
        if addr.is_empty() || token.is_empty() {
            return Err("ta-easy: create failed".into());
        }
        let expires_at = data["expiresAt"]
            .as_i64()
            .or_else(|| data["expiresAt"].as_f64().map(|f| f as i64))
            .filter(|&v| v > 0);

        Ok(EmailInfo {
            channel: Channel::TaEasy,
            email: addr.to_string(),
            token: Some(token.to_string()),
            expires_at,
            created_at: None,
        })
    })
}

pub fn get_emails(email: &str, token: &str) -> Result<Vec<Email>, String> {
    let email = email.to_string();
    block_on(async {
        let resp = http_client()
            .post(format!("{API_BASE}/temp-email/inbox/list"))
            .header("Accept", "application/json")
            .header("User-Agent", get_current_ua())
            .header("origin", ORIGIN)
            .header("referer", format!("{ORIGIN}/"))
            .header("Content-Type", "application/json")
            .json(&serde_json::json!({ "token": token, "email": email }))
            .send()
            .await
            .map_err(|e| format!("ta-easy inbox request failed: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("ta-easy inbox failed: {}", resp.status()));
        }

        let data: Value = resp.json().await.map_err(|e| format!("parse failed: {}", e))?;
        if data["status"].as_str() != Some("success") {
            let msg = data["message"].as_str().unwrap_or("inbox failed");
            return Err(format!("ta-easy: {}", msg));
        }
        let arr = data["data"].as_array().cloned().unwrap_or_default();
        Ok(arr.iter().map(|raw| normalize_email(raw, &email)).collect())
    })
}
