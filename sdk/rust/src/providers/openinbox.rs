/*!
 * OpenInbox -- https://openinbox.io
 */

use crate::config::{block_on, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::{Map, Value};

const API_BASE: &str = "https://api.openinbox.io/api";

fn fetch_json(method: &str, url: String) -> Result<Value, String> {
    block_on(async {
        let builder = match method {
            "POST" => http_client()
                .post(url)
                .header("Content-Type", "application/json"),
            _ => http_client().get(url),
        };
        let resp = builder
            .header("Accept", "application/json, text/plain, */*")
            .header("Origin", "https://openinbox.io")
            .header("Referer", "https://openinbox.io/")
            .header("User-Agent", "Mozilla/5.0")
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("openinbox http {}", resp.status()));
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
        raw.get("from")
            .cloned()
            .unwrap_or(Value::String(String::new())),
    );
    out.insert("to".into(), Value::String(recipient.to_string()));
    out.insert(
        "subject".into(),
        raw.get("subject")
            .cloned()
            .unwrap_or(Value::String(String::new())),
    );
    out.insert(
        "text".into(),
        raw.get("textBody")
            .cloned()
            .unwrap_or(Value::String(String::new())),
    );
    out.insert(
        "html".into(),
        raw.get("htmlBody")
            .cloned()
            .unwrap_or(Value::String(String::new())),
    );
    if let Some(received_at) = raw.get("receivedAt") {
        out.insert("receivedAt".into(), received_at.clone());
    }
    if let Some(is_read) = raw.get("isRead") {
        out.insert("isRead".into(), is_read.clone());
    }
    out.insert("attachments".into(), Value::Array(vec![]));
    Value::Object(out)
}

pub fn generate_email() -> Result<EmailInfo, String> {
    let data = fetch_json("POST", format!("{}/inbox", API_BASE))?;
    let inbox_id = data
        .get("id")
        .and_then(|x| x.as_str())
        .unwrap_or("")
        .trim()
        .to_string();
    let email = data
        .get("email")
        .and_then(|x| x.as_str())
        .unwrap_or("")
        .trim()
        .to_string();
    if inbox_id.is_empty() || email.is_empty() || !email.contains('@') {
        return Err("openinbox: invalid mailbox response".into());
    }
    Ok(EmailInfo {
        channel: Channel::Openinbox,
        email,
        token: Some(inbox_id),
        expires_at: None,
        created_at: data
            .get("createdAt")
            .and_then(|x| x.as_str())
            .map(|x| x.to_string()),
    })
}

fn fetch_detail(message_id: &str) -> Result<Value, String> {
    fetch_json(
        "GET",
        format!("{}/emails/{}", API_BASE, urlencoding::encode(message_id)),
    )
}

pub fn get_emails(inbox_id: &str, email: &str) -> Result<Vec<Email>, String> {
    let box_id = inbox_id.trim();
    let address = email.trim();
    if box_id.is_empty() {
        return Err("openinbox: empty inbox id".into());
    }
    if address.is_empty() {
        return Err("openinbox: empty email".into());
    }
    let data = fetch_json(
        "GET",
        format!(
            "{}/emails/inbox/{}?page=1&limit=50",
            API_BASE,
            urlencoding::encode(box_id)
        ),
    )?;
    let rows = data
        .get("emails")
        .and_then(|x| x.as_array())
        .cloned()
        .unwrap_or_default();
    let mut out = Vec::new();
    for row in rows {
        let raw = row
            .get("id")
            .and_then(|x| x.as_str())
            .and_then(|id| fetch_detail(id).ok())
            .unwrap_or(row);
        out.push(normalize_email(&flatten(&raw, address), address));
    }
    Ok(out)
}
