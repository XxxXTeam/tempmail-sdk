/*!
 * Tempy Email 渠道实现
 * API: https://tempy.email/api/v1
 */

use crate::config::{block_on, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::Value;

const API_BASE: &str = "https://tempy.email/api/v1";

fn browser_ua() -> &'static str {
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36"
}

async fn request_json(method: &str, url: String, body: Option<Value>) -> Result<Value, String> {
    let mut req = match method {
        "POST" => http_client().post(url),
        _ => http_client().get(url),
    }
    .header("Accept", "application/json")
    .header("User-Agent", browser_ua());

    if let Some(b) = body {
        req = req.header("Content-Type", "application/json").json(&b);
    }

    let resp = req
        .send()
        .await
        .map_err(|e| format!("tempy-email request failed: {}", e))?;
    if !resp.status().is_success() {
        return Err(format!("tempy-email http {}", resp.status()));
    }
    resp.json::<Value>()
        .await
        .map_err(|e| format!("tempy-email parse response: {}", e))
}

fn flatten_message(raw: &Value, recipient: &str) -> Value {
    let to = raw
        .get("to")
        .and_then(|v| v.as_str())
        .unwrap_or(recipient)
        .trim()
        .to_string();

    serde_json::json!({
        "id": raw.get("id")
            .or_else(|| raw.get("messageId"))
            .or_else(|| raw.get("message_id"))
            .or_else(|| raw.get("mail_id"))
            .cloned()
            .unwrap_or(Value::String(String::new())),
        "from": raw.get("from")
            .or_else(|| raw.get("sender"))
            .or_else(|| raw.get("from_addr"))
            .or_else(|| raw.get("from_address"))
            .cloned()
            .unwrap_or(Value::String(String::new())),
        "to": to,
        "subject": raw.get("subject")
            .or_else(|| raw.get("mail_title"))
            .cloned()
            .unwrap_or(Value::String(String::new())),
        "text": raw.get("text")
            .or_else(|| raw.get("body_text"))
            .or_else(|| raw.get("text_body"))
            .or_else(|| raw.get("body"))
            .cloned()
            .unwrap_or(Value::String(String::new())),
        "html": raw.get("html")
            .or_else(|| raw.get("body_html"))
            .or_else(|| raw.get("html_body"))
            .cloned()
            .unwrap_or(Value::String(String::new())),
        "date": raw.get("date")
            .or_else(|| raw.get("received_at"))
            .or_else(|| raw.get("created_at"))
            .cloned()
            .unwrap_or(Value::String(String::new())),
        "is_read": raw.get("is_read")
            .or_else(|| raw.get("isRead"))
            .or_else(|| raw.get("seen"))
            .cloned()
            .unwrap_or(Value::Bool(false)),
        "attachments": raw.get("attachments").cloned().unwrap_or(Value::Array(vec![])),
    })
}

pub fn generate_email(domain: Option<&str>) -> Result<EmailInfo, String> {
    block_on(async {
        let mut body = serde_json::Map::new();
        if let Some(wanted) = domain.map(str::trim).filter(|s| !s.is_empty()) {
            body.insert("domain".into(), Value::String(wanted.to_string()));
        }

        let data = request_json(
            "POST",
            format!("{}/mailbox", API_BASE),
            Some(Value::Object(body)),
        )
        .await?;
        let email = data
            .get("email")
            .and_then(|v| v.as_str())
            .unwrap_or("")
            .trim()
            .to_string();
        if email.is_empty() {
            return Err("tempy-email: invalid create response".into());
        }

        Ok(EmailInfo {
            channel: Channel::TempyEmail,
            email,
            token: None,
            expires_at: None,
            created_at: None,
        })
    })
}

pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let address = email.trim();
    if address.is_empty() {
        return Err("tempy-email: empty email".into());
    }

    block_on(async {
        let data = request_json(
            "GET",
            format!("{}/mailbox/{}/messages", API_BASE, urlencoding::encode(address)),
            None,
        )
        .await?;
        let rows = if let Some(messages) = data.get("messages").and_then(|v| v.as_array()) {
            messages.clone()
        } else if let Some(arr) = data.as_array() {
            arr.clone()
        } else {
            Vec::new()
        };

        let mut out = Vec::with_capacity(rows.len());
        for row in &rows {
            out.push(normalize_email(&flatten_message(row, address), address));
        }
        Ok(out)
    })
}
