/*!
 * Mail10s -- https://mail10s.com
 */

use crate::config::{block_on, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use rand::Rng;
use serde_json::{json, Value};

const BASE_URL: &str = "https://mail10s.com";
const DOMAIN: &str = "mail10s.com";

fn random_local() -> String {
    const CHARS: &[u8] = b"abcdefghijklmnopqrstuvwxyz0123456789";
    let mut rng = rand::thread_rng();
    let mut out = String::from("sdk");
    for _ in 0..16 {
        out.push(CHARS[rng.gen_range(0..CHARS.len())] as char);
    }
    out
}

fn flatten(raw: &Value, recipient: &str) -> Value {
    json!({
        "id": raw.get("id").cloned().unwrap_or(Value::String(String::new())),
        "from": raw.get("sender").cloned().unwrap_or(Value::String(String::new())),
        "to": recipient,
        "subject": raw.get("subject").cloned().unwrap_or(Value::String(String::new())),
        "text": raw.get("body_text").cloned().unwrap_or(Value::String(String::new())),
        "html": raw.get("body_html").cloned().unwrap_or(Value::String(String::new())),
        "date": raw.get("received_at").cloned().unwrap_or(Value::String(String::new())),
        "attachments": raw.get("attachments").cloned().unwrap_or(Value::Array(vec![])),
    })
}

pub fn generate_email() -> Result<EmailInfo, String> {
    let email = format!("{}@{}", random_local(), DOMAIN);
    Ok(EmailInfo {
        channel: Channel::Mail10s,
        email: email.clone(),
        token: Some(email),
        expires_at: None,
        created_at: None,
    })
}

pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let address = email.trim();
    if address.is_empty() {
        return Err("mail10s: empty email".into());
    }
    block_on(async {
        let url = format!(
            "{}/api/emails/{}/inbox",
            BASE_URL,
            urlencoding::encode(address)
        );
        let resp = http_client()
            .get(url)
            .header("Accept", "application/json")
            .header("User-Agent", "Mozilla/5.0")
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("mail10s http {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let rows = data
            .get("data")
            .and_then(|x| x.get("messages"))
            .and_then(|x| x.as_array())
            .cloned()
            .unwrap_or_default();
        Ok(rows
            .iter()
            .map(|row| normalize_email(&flatten(row, address), address))
            .collect())
    })
}
