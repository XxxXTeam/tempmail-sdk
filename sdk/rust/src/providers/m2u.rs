/*!
 * MailToYou / m2u 渠道实现
 * API: https://api.m2u.io
 */

use crate::config::{block_on, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::Value;

const API_BASE: &str = "https://api.m2u.io";

fn browser_ua() -> &'static str {
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36"
}

fn any_string(value: &Value) -> String {
    match value {
        Value::String(s) => s.trim().to_string(),
        Value::Number(n) => n.to_string(),
        Value::Bool(b) => b.to_string(),
        _ => String::new(),
    }
}

fn pack_token(token: &str, view_token: &str) -> String {
    serde_json::json!({
        "token": token,
        "viewToken": view_token,
    })
    .to_string()
}

fn unpack_token(packed: &str) -> (String, String) {
    let value = packed.trim();
    if value.is_empty() {
        return (String::new(), String::new());
    }
    if value.starts_with('{') {
        if let Ok(data) = serde_json::from_str::<Value>(value) {
            return (any_string(&data["token"]), any_string(&data["viewToken"]));
        }
        return (String::new(), String::new());
    }
    (value.to_string(), String::new())
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
        .map_err(|e| format!("m2u request failed: {}", e))?;
    if !resp.status().is_success() {
        return Err(format!("m2u http {}", resp.status()));
    }
    resp.json::<Value>()
        .await
        .map_err(|e| format!("m2u parse response: {}", e))
}

fn flatten_message(raw: &Value, recipient: &str) -> Value {
    let id = any_string(
        raw.get("id")
            .or_else(|| raw.get("message_id"))
            .unwrap_or(&Value::Null),
    );
    let from = any_string(
        raw.get("from_addr")
            .or_else(|| raw.get("from_address"))
            .or_else(|| raw.get("from"))
            .unwrap_or(&Value::Null),
    );
    let text = any_string(
        raw.get("text_body")
            .or_else(|| raw.get("body_text"))
            .or_else(|| raw.get("text"))
            .unwrap_or(&Value::Null),
    );
    let html = any_string(
        raw.get("html_body")
            .or_else(|| raw.get("body_html"))
            .or_else(|| raw.get("html"))
            .unwrap_or(&Value::Null),
    );
    let date = any_string(
        raw.get("received_at")
            .or_else(|| raw.get("created_at"))
            .unwrap_or(&Value::Null),
    );

    serde_json::json!({
        "id": id,
        "from": from,
        "to": recipient,
        "subject": raw.get("subject").cloned().unwrap_or(Value::String(String::new())),
        "text": text,
        "html": html,
        "date": date,
        "is_read": raw.get("is_read").cloned().unwrap_or(Value::Bool(false)),
        "attachments": raw.get("attachments").cloned().unwrap_or(Value::Array(vec![])),
    })
}

pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let data = request_json(
            "POST",
            format!("{}/v1/mailboxes/auto", API_BASE),
            Some(serde_json::json!({})),
        )
        .await?;
        let mailbox = data.get("mailbox").cloned().unwrap_or(Value::Null);
        let local_part = any_string(mailbox.get("local_part").unwrap_or(&Value::Null));
        let domain = any_string(mailbox.get("domain").unwrap_or(&Value::Null));
        let token = any_string(mailbox.get("token").unwrap_or(&Value::Null));
        let view_token = any_string(mailbox.get("view_token").unwrap_or(&Value::Null));
        let email = if !local_part.is_empty() && !domain.is_empty() {
            format!("{}@{}", local_part, domain)
        } else {
            String::new()
        };

        if email.is_empty() || token.is_empty() || view_token.is_empty() {
            return Err("m2u: invalid mailbox response".into());
        }

        Ok(EmailInfo {
            channel: Channel::M2u,
            email,
            token: Some(pack_token(&token, &view_token)),
            expires_at: mailbox
                .get("expires_at")
                .and_then(|v| v.as_i64())
                .or_else(|| mailbox.get("expires_at").and_then(|v| v.as_f64()).map(|v| v as i64)),
            created_at: {
                let value = any_string(mailbox.get("created_at").unwrap_or(&Value::Null));
                if value.is_empty() {
                    None
                } else {
                    Some(value)
                }
            },
        })
    })
}

async fn fetch_detail(token: &str, view_token: &str, message_id: &str) -> Option<Value> {
    let url = format!(
        "{}/v1/mailboxes/{}/messages/{}?view={}",
        API_BASE,
        urlencoding::encode(token),
        urlencoding::encode(message_id),
        urlencoding::encode(view_token)
    );
    match request_json("GET", url, None).await {
        Ok(data) => data.get("message").cloned(),
        Err(_) => None,
    }
}

pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let (mailbox_token, view_token) = unpack_token(token);
    let address = email.trim();
    if mailbox_token.is_empty() {
        return Err("m2u: missing token".into());
    }
    if view_token.is_empty() {
        return Err("m2u: missing view token".into());
    }
    if address.is_empty() {
        return Err("m2u: empty email".into());
    }

    block_on(async {
        let data = request_json(
            "GET",
            format!(
                "{}/v1/mailboxes/{}/messages?view={}",
                API_BASE,
                urlencoding::encode(&mailbox_token),
                urlencoding::encode(&view_token)
            ),
            None,
        )
        .await?;
        let rows = data
            .get("messages")
            .and_then(|v| v.as_array())
            .cloned()
            .unwrap_or_default();
        let mut out = Vec::with_capacity(rows.len());
        for row in &rows {
            let message_id = row.get("id").and_then(|v| v.as_str()).unwrap_or("").trim();
            let raw = if !message_id.is_empty() {
                fetch_detail(&mailbox_token, &view_token, message_id)
                    .await
                    .unwrap_or_else(|| row.clone())
            } else {
                row.clone()
            };
            out.push(normalize_email(&flatten_message(&raw, address), address));
        }
        Ok(out)
    })
}
