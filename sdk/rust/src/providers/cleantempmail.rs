/*!
 * CleanTempMail -- https://cleantempmail.com
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::Value;
use std::env;

const API_BASE: &str = "https://cleantempmail.com/api";
const DEFAULT_API_KEY: &str = "ct-test";

fn api_key() -> String {
    env::var("CLEANTEMPMAIL_API_KEY")
        .ok()
        .map(|s| s.trim().to_string())
        .filter(|s| !s.is_empty())
        .unwrap_or_else(|| DEFAULT_API_KEY.to_string())
}

fn fetch_json(url: String) -> Result<Value, String> {
    block_on(async {
        let resp = http_client()
            .get(url)
            .header("Accept", "application/json")
            .header("User-Agent", get_current_ua())
            .header("X-API-Key", api_key())
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("cleantempmail {}", resp.status()));
        }
        resp.json::<Value>().await.map_err(|e| e.to_string())
    })
}

pub fn generate_email() -> Result<EmailInfo, String> {
    let data = fetch_json(format!("{}/generate-email", API_BASE))?;
    let payload = data.get("data").cloned().unwrap_or(Value::Null);
    let mut email = payload
        .get("email")
        .and_then(|v| v.as_str())
        .unwrap_or("")
        .trim()
        .to_string();
    if email.is_empty() {
        email = payload
            .get("mailbox")
            .and_then(|v| v.as_str())
            .unwrap_or("")
            .trim()
            .to_string();
    }
    if email.is_empty() || !email.contains('@') {
        return Err("cleantempmail: invalid generate-email response".into());
    }
    Ok(EmailInfo {
        channel: Channel::CleanTempMail,
        email,
        token: None,
        expires_at: None,
        created_at: None,
    })
}

pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let address = email.trim();
    if address.is_empty() {
        return Err("cleantempmail: empty email".into());
    }
    let data = fetch_json(format!(
        "{}/emails?email={}",
        API_BASE,
        urlencoding::encode(address)
    ))?;
    let payload = data.get("data").cloned().unwrap_or(Value::Null);
    let rows = payload
        .get("emails")
        .and_then(|v| v.as_array())
        .cloned()
        .unwrap_or_default();
    let mut out = Vec::with_capacity(rows.len());
    for item in rows {
        out.push(normalize_email(&item, address));
    }
    Ok(out)
}
