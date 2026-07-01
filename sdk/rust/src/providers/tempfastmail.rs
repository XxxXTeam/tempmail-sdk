/*!
 * TempFastMail -- https://tempfastmail.com
 */

use crate::config::{block_on, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::{json, Value};

const BASE_URL: &str = "https://tempfastmail.com";

fn flatten(raw: &Value, recipient: &str) -> Value {
    let to = raw
        .get("real_to")
        .and_then(|x| x.as_str())
        .filter(|x| !x.trim().is_empty())
        .unwrap_or(recipient);
    json!({
        "id": raw.get("uuid").cloned().unwrap_or(Value::String(String::new())),
        "from": raw.get("from").cloned().unwrap_or(Value::String(String::new())),
        "to": to,
        "subject": raw.get("subject").cloned().unwrap_or(Value::String(String::new())),
        "html": raw.get("html").cloned().unwrap_or(Value::String(String::new())),
        "date": raw.get("received_at").cloned().unwrap_or(Value::String(String::new())),
        "attachments": raw.get("attachments").cloned().unwrap_or(Value::Array(vec![])),
    })
}

pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let resp = http_client()
            .post(format!("{}/api/email-box", BASE_URL))
            .header("Accept", "application/json")
            .header("User-Agent", "Mozilla/5.0")
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("tempfastmail create http {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let email = data
            .get("email")
            .and_then(|x| x.as_str())
            .unwrap_or("")
            .trim()
            .to_string();
        let uuid = data
            .get("uuid")
            .and_then(|x| x.as_str())
            .unwrap_or("")
            .trim()
            .to_string();
        if email.is_empty() || !email.contains('@') || uuid.is_empty() {
            return Err("tempfastmail: invalid create response".into());
        }
        Ok(EmailInfo {
            channel: Channel::Tempfastmail,
            email,
            token: Some(uuid),
            expires_at: None,
            created_at: None,
        })
    })
}

async fn get_json(path: String) -> Result<Value, String> {
    let resp = http_client()
        .get(format!("{}{}", BASE_URL, path))
        .header("Accept", "application/json")
        .header("User-Agent", "Mozilla/5.0")
        .send()
        .await
        .map_err(|e| e.to_string())?;
    if !resp.status().is_success() {
        return Err(format!("tempfastmail http {}", resp.status()));
    }
    resp.json::<Value>().await.map_err(|e| e.to_string())
}

pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let uuid = token.trim();
    let address = email.trim();
    if uuid.is_empty() {
        return Err("tempfastmail: empty token".into());
    }
    if address.is_empty() {
        return Err("tempfastmail: empty email".into());
    }
    block_on(async {
        let list = get_json(format!(
            "/api/email-box/{}/emails",
            urlencoding::encode(uuid)
        ))
        .await?;
        let rows = list.as_array().cloned().unwrap_or_default();
        let mut out = Vec::new();
        for row in rows {
            let raw = match row.get("uuid").and_then(|x| x.as_str()) {
                Some(message_id) if !message_id.trim().is_empty() => get_json(format!(
                    "/api/email-box/{}/email/{}",
                    urlencoding::encode(uuid),
                    urlencoding::encode(message_id)
                ))
                .await
                .unwrap_or(row),
                _ => row,
            };
            out.push(normalize_email(&flatten(&raw, address), address));
        }
        Ok(out)
    })
}
