/*!
 * TempMail Pro - https://tempmailpro.us
 */

use crate::config::{block_on, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::{json, Value};

const API_BASE: &str = "https://tempmailpro.us/api/v1";

fn flatten(raw: &Value, recipient: &str) -> Value {
    json!({
        "id": raw.get("id").cloned().or_else(|| raw.get("message_id").cloned()).unwrap_or(Value::String(String::new())),
        "from": raw.get("from_address").cloned().or_else(|| raw.get("from_name").cloned()).unwrap_or(Value::String(String::new())),
        "to": recipient,
        "subject": raw.get("subject").cloned().unwrap_or(Value::String(String::new())),
        "text": raw.get("body_text").cloned().unwrap_or(Value::String(String::new())),
        "html": raw.get("body_html").cloned().unwrap_or(Value::String(String::new())),
        "date": raw.get("received_at").cloned().unwrap_or(Value::Null),
        "attachments": raw.get("attachments").cloned().unwrap_or(Value::Array(Vec::new())),
    })
}

pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let resp = http_client()
            .post(format!("{}/mailbox/create", API_BASE))
            .header("Accept", "application/json")
            .header("Content-Type", "application/json")
            .header("User-Agent", "Mozilla/5.0")
            .json(&json!({}))
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("tempmailpro http {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let box_data = data.get("data").unwrap_or(&Value::Null);
        let email = box_data
            .get("address")
            .and_then(|x| x.as_str())
            .unwrap_or("")
            .trim()
            .to_string();
        let token = box_data
            .get("token")
            .and_then(|x| x.as_str())
            .unwrap_or("")
            .trim()
            .to_string();
        if email.is_empty() || !email.contains('@') || token.is_empty() {
            return Err("tempmailpro: invalid mailbox response".into());
        }
        Ok(EmailInfo {
            channel: Channel::Tempmailpro,
            email,
            token: Some(token),
            expires_at: box_data.get("expires_at").and_then(|x| x.as_i64()),
            created_at: box_data.get("created_at").map(|x| {
                x.as_str()
                    .map(|s| s.to_string())
                    .unwrap_or_else(|| x.to_string())
            }),
        })
    })
}

fn fetch_detail(token: &str, message_id: &str) -> Result<Value, String> {
    block_on(async {
        let resp = http_client()
            .get(format!(
                "{}/mailbox/{}/emails/{}",
                API_BASE,
                urlencoding::encode(token),
                urlencoding::encode(message_id)
            ))
            .header("Accept", "application/json")
            .header("User-Agent", "Mozilla/5.0")
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("tempmailpro message {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        Ok(data.get("data").cloned().unwrap_or(Value::Null))
    })
}

pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let mailbox_token = token.trim();
    let address = email.trim();
    if mailbox_token.is_empty() {
        return Err("tempmailpro: empty token".into());
    }
    if address.is_empty() {
        return Err("tempmailpro: empty email".into());
    }

    block_on(async {
        let resp = http_client()
            .get(format!(
                "{}/mailbox/{}/emails",
                API_BASE,
                urlencoding::encode(mailbox_token)
            ))
            .header("Accept", "application/json")
            .header("User-Agent", "Mozilla/5.0")
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("tempmailpro messages {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let rows = data
            .get("data")
            .and_then(|x| x.as_array())
            .cloned()
            .unwrap_or_default();
        let mut out = Vec::new();
        for row in rows {
            let raw = row
                .get("id")
                .map(|x| x.to_string())
                .map(|id| id.trim_matches('"').to_string())
                .filter(|id| !id.is_empty())
                .and_then(|id| fetch_detail(mailbox_token, &id).ok())
                .filter(|x| !x.is_null())
                .unwrap_or(row);
            out.push(normalize_email(&flatten(&raw, address), address));
        }
        Ok(out)
    })
}
