/*!
 * ThrowawayMail — https://throwawaymail.app
 */

use crate::config::{block_on, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::{json, Value};

const API_BASE: &str = "https://throwawaymail.app/api";

fn headers(b: wreq::RequestBuilder) -> wreq::RequestBuilder {
    b.header("Accept", "application/json")
}

fn message_raw(raw: &Value, recipient: &str) -> Value {
    let to = raw
        .get("to_addresses")
        .and_then(|x| x.as_array())
        .and_then(|xs| xs.first())
        .and_then(|x| x.as_str())
        .unwrap_or(recipient);
    json!({
        "id": raw.get("message_id").cloned().unwrap_or(Value::String(String::new())),
        "messageId": raw.get("message_id").cloned().unwrap_or(Value::String(String::new())),
        "from_address": raw.get("from_address").cloned().unwrap_or(Value::String(String::new())),
        "fromName": raw.get("from_name").cloned().unwrap_or(Value::Null),
        "to": to,
        "subject": raw.get("subject").cloned().unwrap_or(Value::String(String::new())),
        "received_at": raw.get("received_at").cloned().unwrap_or(Value::Null),
        "read": raw.get("read").cloned().unwrap_or(Value::Bool(false)),
        "text": raw.get("text").cloned().unwrap_or(Value::String(String::new())),
        "html": raw.get("html").cloned().unwrap_or(Value::String(String::new())),
        "size": raw.get("size").cloned().unwrap_or(Value::Null),
    })
}

fn fetch_message(mailbox_id: &str, message_id: &str) -> Result<Value, String> {
    block_on(async {
        let url = format!(
            "{}/mailboxes/{}/messages/{}",
            API_BASE,
            urlencoding::encode(mailbox_id),
            urlencoding::encode(message_id)
        );
        let resp = headers(http_client().get(url))
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("throwawaymail message {}", resp.status()));
        }
        resp.json::<Value>().await.map_err(|e| e.to_string())
    })
}

pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let resp = headers(http_client().post(format!("{}/mailboxes", API_BASE)))
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("throwawaymail mailbox {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let mailbox_id = data
            .get("mailbox_id")
            .and_then(|x| x.as_str())
            .unwrap_or("")
            .trim()
            .to_string();
        let email = data
            .get("address")
            .and_then(|x| x.as_str())
            .unwrap_or("")
            .trim()
            .to_string();
        if mailbox_id.is_empty() || email.is_empty() || !email.contains('@') {
            return Err("throwawaymail: invalid mailbox response".into());
        }
        Ok(EmailInfo {
            channel: Channel::Throwawaymail,
            email,
            token: Some(mailbox_id),
            expires_at: None,
            created_at: data
                .get("created_at")
                .and_then(|x| x.as_str())
                .map(|x| x.to_string()),
        })
    })
}

pub fn get_emails(mailbox_id: &str, email: &str) -> Result<Vec<Email>, String> {
    let mid = mailbox_id.trim();
    let em = email.trim();
    if mid.is_empty() {
        return Err("throwawaymail: empty mailbox id".into());
    }
    if em.is_empty() {
        return Err("throwawaymail: empty email".into());
    }
    block_on(async {
        let url = format!(
            "{}/mailboxes/{}/messages",
            API_BASE,
            urlencoding::encode(mid)
        );
        let resp = headers(http_client().get(url))
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("throwawaymail messages {}", resp.status()));
        }
        let rows: Value = resp.json().await.map_err(|e| e.to_string())?;
        let items = rows.as_array().cloned().unwrap_or_default();
        let mut out = Vec::new();
        for item in items {
            let message_id = item
                .get("message_id")
                .and_then(|x| x.as_str())
                .unwrap_or("")
                .trim();
            let raw = if !message_id.is_empty() {
                fetch_message(mid, message_id).unwrap_or(item)
            } else {
                item
            };
            out.push(normalize_email(&message_raw(&raw, em), em));
        }
        Ok(out)
    })
}
