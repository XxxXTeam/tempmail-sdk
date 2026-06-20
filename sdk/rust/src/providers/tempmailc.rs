/*!
 * TempMailC — https://tempmailc.com
 */

use crate::config::{block_on, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::{json, Value};

const API_BASE: &str = "https://tempmailc.com/api/v1";

fn headers(b: wreq::RequestBuilder) -> wreq::RequestBuilder {
    b.header("Accept", "application/json")
}

fn flatten_list_message(raw: &Value, recipient: &str) -> Value {
    json!({
        "id": raw.get("id").cloned().unwrap_or(Value::String(String::new())),
        "from": raw.get("from").cloned().unwrap_or(Value::String(String::new())),
        "to": recipient,
        "subject": raw.get("subject").cloned().unwrap_or(Value::String(String::new())),
        "timestamp": raw.get("ts").cloned().unwrap_or(Value::Null),
        "read": raw.get("read").cloned().unwrap_or(Value::Bool(false)),
    })
}

fn fetch_message(email: &str, message_id: &str) -> Result<Value, String> {
    block_on(async {
        let url = format!(
            "{}/message?email={}&msg_id={}",
            API_BASE,
            urlencoding::encode(email),
            urlencoding::encode(message_id)
        );
        let resp = headers(http_client().get(url))
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("tempmailc message {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        if data.get("ok").and_then(|x| x.as_bool()) == Some(false) {
            return Err("tempmailc: message response failed".into());
        }
        Ok(data)
    })
}

pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let resp = headers(http_client().get(format!("{}/new", API_BASE)))
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("tempmailc new {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let email = data
            .get("email")
            .and_then(|x| x.as_str())
            .unwrap_or("")
            .trim()
            .to_string();
        if data.get("ok").and_then(|x| x.as_bool()) != Some(true)
            || email.is_empty()
            || !email.contains('@')
        {
            return Err("tempmailc: invalid new email response".into());
        }
        Ok(EmailInfo {
            channel: Channel::Tempmailc,
            email,
            token: None,
            expires_at: None,
            created_at: None,
        })
    })
}

pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let em = email.trim();
    if em.is_empty() {
        return Err("tempmailc: empty email".into());
    }
    block_on(async {
        let url = format!("{}/inbox?email={}", API_BASE, urlencoding::encode(em));
        let resp = headers(http_client().get(url))
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("tempmailc inbox {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        if data.get("ok").and_then(|x| x.as_bool()) != Some(true) {
            return Err("tempmailc: inbox response failed".into());
        }
        let rows = data
            .get("messages")
            .and_then(|x| x.as_array())
            .cloned()
            .unwrap_or_default();
        let mut out = Vec::new();
        for item in rows {
            let id = item.get("id").and_then(|x| x.as_str()).unwrap_or("").trim();
            if !id.is_empty() {
                if let Ok(detail) = fetch_message(em, id) {
                    out.push(normalize_email(&detail, em));
                    continue;
                }
            }
            out.push(normalize_email(&flatten_list_message(&item, em), em));
        }
        Ok(out)
    })
}
