/*!
 * UnCorreoTemporal -- https://uncorreotemporal.com
 */

use crate::config::{block_on, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::{Map, Value};

const API_BASE: &str = "https://uncorreotemporal.com/api/v1";

fn fetch_json(method: &str, url: String, token: Option<&str>) -> Result<Value, String> {
    block_on(async {
        let builder = match method {
            "POST" => http_client()
                .post(url)
                .header("Content-Type", "application/json"),
            _ => http_client().get(url),
        };
        let mut builder = builder
            .header("Accept", "application/json")
            .header("Origin", "https://uncorreotemporal.com")
            .header("Referer", "https://uncorreotemporal.com/")
            .header("User-Agent", "Mozilla/5.0");
        if let Some(session_token) = token {
            builder = builder.header("X-Session-Token", session_token);
        }
        let resp = builder.send().await.map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("uncorreotemporal http {}", resp.status()));
        }
        resp.json::<Value>().await.map_err(|e| e.to_string())
    })
}

fn flatten(raw: &Value, recipient: &str) -> Value {
    let mut out = Map::new();
    out.insert(
        "id".into(),
        raw.get("id")
            .cloned()
            .unwrap_or(Value::String(String::new())),
    );
    out.insert(
        "from".into(),
        raw.get("from_address")
            .cloned()
            .unwrap_or(Value::String(String::new())),
    );
    out.insert(
        "to".into(),
        raw.get("to_address")
            .cloned()
            .unwrap_or_else(|| Value::String(recipient.to_string())),
    );
    out.insert(
        "subject".into(),
        raw.get("subject")
            .cloned()
            .unwrap_or(Value::String(String::new())),
    );
    out.insert(
        "text".into(),
        raw.get("body_text")
            .cloned()
            .unwrap_or(Value::String(String::new())),
    );
    out.insert(
        "html".into(),
        raw.get("body_html")
            .cloned()
            .unwrap_or(Value::String(String::new())),
    );
    if let Some(received_at) = raw.get("received_at") {
        out.insert("date".into(), received_at.clone());
    }
    if let Some(is_read) = raw.get("is_read") {
        out.insert("isRead".into(), is_read.clone());
    }
    out.insert(
        "attachments".into(),
        raw.get("attachments")
            .cloned()
            .unwrap_or(Value::Array(vec![])),
    );
    Value::Object(out)
}

pub fn generate_email() -> Result<EmailInfo, String> {
    let data = fetch_json("POST", format!("{}/mailboxes", API_BASE), None)?;
    let email = data
        .get("address")
        .and_then(|x| x.as_str())
        .unwrap_or("")
        .trim()
        .to_string();
    let token = data
        .get("session_token")
        .and_then(|x| x.as_str())
        .unwrap_or("")
        .trim()
        .to_string();
    if email.is_empty() || !email.contains('@') || token.is_empty() {
        return Err("uncorreotemporal: invalid mailbox response".into());
    }
    Ok(EmailInfo {
        channel: Channel::Uncorreotemporal,
        email,
        token: Some(token),
        expires_at: None,
        created_at: None,
    })
}

fn fetch_detail(email: &str, token: &str, message_id: &str) -> Result<Value, String> {
    fetch_json(
        "GET",
        format!(
            "{}/mailboxes/{}/messages/{}",
            API_BASE,
            urlencoding::encode(email),
            urlencoding::encode(message_id)
        ),
        Some(token),
    )
}

pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let session_token = token.trim();
    let address = email.trim();
    if session_token.is_empty() {
        return Err("uncorreotemporal: empty session token".into());
    }
    if address.is_empty() {
        return Err("uncorreotemporal: empty email".into());
    }
    let data = fetch_json(
        "GET",
        format!(
            "{}/mailboxes/{}/messages?limit=50",
            API_BASE,
            urlencoding::encode(address)
        ),
        Some(session_token),
    )?;
    let rows = data.as_array().cloned().unwrap_or_default();
    let mut out = Vec::new();
    for row in rows {
        let raw = row
            .get("id")
            .and_then(|x| x.as_str())
            .and_then(|id| fetch_detail(address, session_token, id).ok())
            .unwrap_or(row);
        out.push(normalize_email(&flatten(&raw, address), address));
    }
    Ok(out)
}
