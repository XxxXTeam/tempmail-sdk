/*!
 * Mail123 -- https://mail123.fr
 */

use crate::config::{block_on, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::{Map, Value};

const API_BASE: &str = "https://mail123.fr/api/v1";

fn fetch_json(url: String) -> Result<Value, String> {
    block_on(async {
        let resp = http_client()
            .get(url)
            .header("Accept", "application/json")
            .header("User-Agent", "Mozilla/5.0")
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("mail123 http {}", resp.status()));
        }
        resp.json::<Value>().await.map_err(|e| e.to_string())
    })
}

fn flatten(raw: &Value, recipient: &str) -> Value {
    let mut out: Map<String, Value> = raw.as_object().cloned().unwrap_or_default();
    out.insert(
        "id".to_string(),
        raw.get("id")
            .cloned()
            .unwrap_or(Value::String(String::new())),
    );
    if raw
        .get("to")
        .and_then(|x| x.as_str())
        .unwrap_or("")
        .is_empty()
    {
        out.insert("to".to_string(), Value::String(recipient.to_string()));
    }
    if !out.contains_key("text") {
        if let Some(preview) = raw.get("preview") {
            out.insert("text".to_string(), preview.clone());
        }
    }
    if let Some(unread) = raw.get("is_unread").and_then(|x| x.as_bool()) {
        out.insert("isRead".to_string(), Value::Bool(!unread));
    }
    Value::Object(out)
}

pub fn generate_email() -> Result<EmailInfo, String> {
    let data = fetch_json(format!("{}/mailbox/new", API_BASE))?;
    let email = data
        .get("address")
        .and_then(|x| x.as_str())
        .unwrap_or("")
        .trim()
        .to_string();
    if email.is_empty() || !email.contains('@') {
        return Err("mail123: invalid mailbox response".into());
    }
    let expires_at = data
        .get("expires_in_days")
        .and_then(|x| x.as_i64())
        .filter(|x| *x > 0)
        .map(|days| chrono::Utc::now().timestamp_millis() + days * 86_400_000);
    Ok(EmailInfo {
        channel: Channel::Mail123,
        email: email.clone(),
        token: Some(email),
        expires_at,
        created_at: None,
    })
}

fn fetch_detail(address: &str, message_id: &str) -> Result<Value, String> {
    let data = fetch_json(format!(
        "{}/mailbox/{}/messages/{}",
        API_BASE,
        urlencoding::encode(address),
        urlencoding::encode(message_id)
    ))?;
    Ok(data.get("message").cloned().unwrap_or(Value::Null))
}

pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let address = email.trim();
    if address.is_empty() {
        return Err("mail123: empty email".into());
    }
    let data = fetch_json(format!(
        "{}/mailbox/{}/messages?limit=50",
        API_BASE,
        urlencoding::encode(address)
    ))?;
    let rows = data
        .get("messages")
        .and_then(|x| x.as_array())
        .cloned()
        .unwrap_or_default();
    let mut out = Vec::new();
    for row in rows {
        let raw = row
            .get("id")
            .and_then(|x| x.as_str())
            .and_then(|id| fetch_detail(address, id).ok())
            .filter(|x| !x.is_null())
            .unwrap_or(row);
        out.push(normalize_email(&flatten(&raw, address), address));
    }
    Ok(out)
}
