/*!
 * fmail 渠道实现
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::Value;

const BASE_URL: &str = "https://fmail.men";

fn any_string(v: &Value) -> String {
    match v {
        Value::String(s) => s.trim().to_string(),
        Value::Number(n) => n.to_string(),
        Value::Bool(b) => b.to_string(),
        _ => String::new(),
    }
}

fn normalize_domain(domain: Option<&str>) -> Option<String> {
    let value = domain.unwrap_or("").trim().trim_start_matches('@').to_string();
    if value.is_empty() {
        None
    } else {
        Some(value)
    }
}

async fn request_json(path: &str) -> Result<Value, String> {
    let resp = http_client()
        .get(format!("{}{}", BASE_URL, path))
        .header("Accept", "application/json")
        .header("User-Agent", get_current_ua())
        .send()
        .await
        .map_err(|e| format!("fmail request failed: {}", e))?;
    if !resp.status().is_success() {
        return Err(format!("fmail http {}", resp.status()));
    }
    resp.json::<Value>()
        .await
        .map_err(|e| format!("fmail parse response: {}", e))
}

fn flatten_message(raw: &Value, recipient: &str) -> Value {
    serde_json::json!({
        "id": any_string(raw.get("id").unwrap_or(&Value::Null)),
        "from": raw.get("sender").and_then(Value::as_str).or_else(|| raw.get("from").and_then(Value::as_str)).unwrap_or(""),
        "to": raw.get("recipient").and_then(Value::as_str).or_else(|| raw.get("to").and_then(Value::as_str)).unwrap_or(recipient),
        "subject": raw.get("subject").and_then(Value::as_str).unwrap_or(""),
        "text": raw.get("body_text").and_then(Value::as_str).or_else(|| raw.get("text").and_then(Value::as_str)).or_else(|| raw.get("snippet").and_then(Value::as_str)).unwrap_or(""),
        "html": raw.get("body_html").and_then(Value::as_str).or_else(|| raw.get("html").and_then(Value::as_str)).unwrap_or(""),
        "date": raw.get("received_at").and_then(Value::as_str).or_else(|| raw.get("timestamp").and_then(Value::as_str)).or_else(|| raw.get("date").and_then(Value::as_str)).unwrap_or(""),
        "is_read": raw.get("is_read").and_then(Value::as_bool).unwrap_or(false),
        "attachments": raw.get("attachments").cloned().unwrap_or(Value::Array(vec![])),
    })
}

pub fn generate_email(domain: Option<&str>) -> Result<EmailInfo, String> {
    block_on(async {
        let mut path = "/v1/random".to_string();
        if let Some(selected) = normalize_domain(domain) {
            path.push_str("?domain=");
            path.push_str(&urlencoding::encode(&selected));
        }
        let data = request_json(&path).await?;
        let mut email = data.get("address").and_then(Value::as_str).unwrap_or("").trim().to_string();
        if email.is_empty() {
            let username = data.get("username").and_then(Value::as_str).unwrap_or("").trim();
            let dom = data.get("domain").and_then(Value::as_str).unwrap_or("").trim();
            if !username.is_empty() && !dom.is_empty() {
                email = format!("{}@{}", username, dom);
            }
        }
        if email.is_empty() || !email.contains('@') {
            return Err("fmail: invalid random response".into());
        }
        Ok(EmailInfo {
            channel: Channel::Fmail,
            email,
            token: None,
            expires_at: None,
            created_at: None,
        })
    })
}

pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let address = email.trim();
    let (local, domain) = address
        .split_once('@')
        .ok_or_else(|| "fmail: invalid email".to_string())?;
    if local.trim().is_empty() || domain.trim().is_empty() {
        return Err("fmail: invalid email".into());
    }

    block_on(async {
        let data = request_json(&format!(
            "/v1/inbox/{}?domain={}&limit=50",
            urlencoding::encode(local),
            urlencoding::encode(domain)
        ))
        .await?;
        let rows = data
            .get("emails")
            .and_then(|v| v.as_array())
            .cloned()
            .unwrap_or_default();
        let mut out = Vec::with_capacity(rows.len());
        for row in &rows {
            let token = row
                .get("token")
                .and_then(Value::as_str)
                .or_else(|| row.get("id").and_then(Value::as_str))
                .unwrap_or("")
                .trim()
                .to_string();
            if token.is_empty() {
                out.push(normalize_email(&flatten_message(row, address), address));
                continue;
            }
            let detail = request_json(&format!("/v1/email/{}", urlencoding::encode(&token)))
                .await
                .unwrap_or_else(|_| row.clone());
            let nested = detail.get("email").and_then(|v| v.as_object()).map(|m| Value::Object(m.clone()));
            let picked = nested.as_ref().unwrap_or(&detail);
            out.push(normalize_email(&flatten_message(picked, address), address));
        }
        Ok(out)
    })
}
