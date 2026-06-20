/*!
 * tempmail.lol 渠道实现
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::Value;

const BASE_URL: &str = "https://api.tempmail.lol/v2";

pub fn generate_email(domain: Option<&str>) -> Result<EmailInfo, String> {
    let domain = domain.map(|s| s.to_string());
    block_on(async {
        let resp = http_client()
            .post(format!("{}/inbox/create", BASE_URL))
            .header("Content-Type", "application/json")
            .header("User-Agent", get_current_ua())
            .header("Origin", "https://tempmail.lol")
            .header("DNT", "1")
            .json(&serde_json::json!({"domain": domain, "captcha": null}))
            .send()
            .await
            .map_err(|e| format!("tempmail-lol request failed: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("tempmail-lol generate failed: {}", resp.status()));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("parse failed: {}", e))?;
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
    })
}

pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let token = token.to_string();
    let email = email.to_string();
    block_on(async {
        let resp = http_client()
            .get(format!(
                "{}/inbox?token={}",
                BASE_URL,
                urlencoding::encode(&token)
            ))
            .header("User-Agent", get_current_ua())
            .header("Origin", "https://tempmail.lol")
            .header("DNT", "1")
            .send()
            .await
            .map_err(|e| format!("tempmail-lol request failed: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("tempmail-lol get emails failed: {}", resp.status()));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("parse failed: {}", e))?;
        Ok(data["emails"]
            .as_array()
            .map(|arr| arr.iter().map(|raw| normalize_email(raw, &email)).collect())
            .unwrap_or_default())
    })
}
