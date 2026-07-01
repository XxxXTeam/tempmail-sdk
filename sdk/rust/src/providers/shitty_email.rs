/*!
 * shitty.email — https://shitty.email
 */

use crate::config::{block_on, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::{json, Value};

const API_BASE: &str = "https://shitty.email/api";

fn headers(b: wreq::RequestBuilder, token: Option<&str>) -> wreq::RequestBuilder {
    let b = b
        .header("Accept", "application/json")
        .header("User-Agent", "Mozilla/5.0");
    if let Some(t) = token {
        b.header("X-Session-Token", t)
    } else {
        b
    }
}

fn message_raw(raw: &Value, recipient: &str) -> Value {
    json!({
        "id": raw.get("id").cloned().unwrap_or(Value::String(String::new())),
        "from": raw.get("from").cloned().unwrap_or(Value::String(String::new())),
        "to": raw.get("to").cloned().unwrap_or(Value::String(recipient.to_string())),
        "subject": raw.get("subject").cloned().unwrap_or(Value::String(String::new())),
        "text": raw.get("text").cloned().unwrap_or(Value::String(String::new())),
        "html": raw.get("html").cloned().unwrap_or(Value::String(String::new())),
        "date": raw.get("date").cloned().unwrap_or(Value::Null),
    })
}

fn fetch_message(token: &str, message_id: &str) -> Result<Value, String> {
    block_on(async {
        let url = format!("{}/email/{}", API_BASE, urlencoding::encode(message_id));
        let resp = headers(http_client().get(url), Some(token))
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("shitty-email message {}", resp.status()));
        }
        resp.json::<Value>().await.map_err(|e| e.to_string())
    })
}

pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let resp = headers(http_client().post(format!("{}/inbox", API_BASE)), None)
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("shitty-email inbox {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let email = data
            .get("email")
            .and_then(|x| x.as_str())
            .unwrap_or("")
            .trim()
            .to_string();
        let token = data
            .get("token")
            .and_then(|x| x.as_str())
            .unwrap_or("")
            .trim()
            .to_string();
        if email.is_empty() || !email.contains('@') || token.is_empty() {
            return Err("shitty-email: invalid inbox response".into());
        }
        Ok(EmailInfo {
            channel: Channel::ShittyEmail,
            email,
            token: Some(token),
            expires_at: None,
            created_at: None,
        })
    })
}

pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let session_token = token.trim();
    let address = email.trim();
    if session_token.is_empty() {
        return Err("shitty-email: empty token".into());
    }
    if address.is_empty() {
        return Err("shitty-email: empty email".into());
    }
    block_on(async {
        let resp = headers(
            http_client().get(format!("{}/inbox", API_BASE)),
            Some(session_token),
        )
        .send()
        .await
        .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("shitty-email messages {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let items = data
            .get("emails")
            .and_then(|x| x.as_array())
            .cloned()
            .unwrap_or_default();
        let mut out = Vec::new();
        for item in items {
            let message_id = item.get("id").and_then(|x| x.as_str()).unwrap_or("").trim();
            let raw = if !message_id.is_empty() {
                fetch_message(session_token, message_id).unwrap_or(item)
            } else {
                item
            };
            out.push(normalize_email(&message_raw(&raw, address), address));
        }
        Ok(out)
    })
}
