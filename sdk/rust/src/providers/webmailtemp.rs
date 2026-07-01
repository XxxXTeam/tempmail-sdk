/*!
 * WebMailTemp -- https://webmailtemp.com
 */

use crate::config::{block_on, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use base64::Engine;
use serde_json::{json, Value};

const BASE_URL: &str = "https://webmailtemp.com";

fn encode_token(username: &str, cookie: &str) -> Result<String, String> {
    let raw =
        serde_json::to_vec(&json!({ "u": username, "c": cookie })).map_err(|e| e.to_string())?;
    Ok(format!(
        "wmt1:{}",
        base64::engine::general_purpose::STANDARD.encode(raw)
    ))
}

fn decode_token(token: &str) -> Result<(String, String), String> {
    let payload = token
        .strip_prefix("wmt1:")
        .ok_or_else(|| "webmailtemp: invalid token".to_string())?;
    let raw = base64::engine::general_purpose::STANDARD
        .decode(payload)
        .map_err(|_| "webmailtemp: invalid token".to_string())?;
    let data: Value =
        serde_json::from_slice(&raw).map_err(|_| "webmailtemp: invalid token data".to_string())?;
    let username = data
        .get("u")
        .and_then(|x| x.as_str())
        .unwrap_or("")
        .trim()
        .to_string();
    let cookie = data
        .get("c")
        .and_then(|x| x.as_str())
        .unwrap_or("")
        .trim()
        .to_string();
    if username.is_empty() || cookie.is_empty() {
        return Err("webmailtemp: invalid token data".into());
    }
    Ok((username, cookie))
}

fn flatten(raw: &Value, recipient: &str) -> Value {
    json!({
        "id": raw.get("id").cloned().unwrap_or(Value::String(String::new())),
        "from": raw.get("from").cloned().unwrap_or(Value::String(String::new())),
        "to": recipient,
        "subject": raw.get("subject").cloned().unwrap_or(Value::String(String::new())),
        "text": raw.get("body").cloned().unwrap_or(Value::String(String::new())),
        "html": raw.get("html").cloned().unwrap_or(Value::String(String::new())),
        "date": raw.get("received_at").or_else(|| raw.get("timestamp")).cloned().unwrap_or(Value::String(String::new())),
        "attachments": raw.get("attachments").cloned().unwrap_or(Value::Array(vec![])),
    })
}

pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let resp = http_client()
            .get(format!("{}/api/create", BASE_URL))
            .header("Accept", "application/json")
            .header("User-Agent", "Mozilla/5.0")
            .send()
            .await
            .map_err(|e| e.to_string())?;
        let cookie = resp
            .headers()
            .get_all("set-cookie")
            .iter()
            .filter_map(|v| v.to_str().ok())
            .filter_map(|v| v.split(';').next())
            .map(str::trim)
            .find(|v| !v.is_empty())
            .unwrap_or("")
            .to_string();
        if !resp.status().is_success() {
            return Err(format!("webmailtemp create http {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let email = data
            .get("email")
            .and_then(|x| x.as_str())
            .unwrap_or("")
            .trim()
            .to_string();
        let username = data
            .get("username")
            .and_then(|x| x.as_str())
            .unwrap_or("")
            .trim()
            .to_string();
        let success = data
            .get("success")
            .and_then(|x| x.as_bool())
            .unwrap_or(false);
        if !success
            || email.is_empty()
            || !email.contains('@')
            || username.is_empty()
            || cookie.is_empty()
        {
            return Err("webmailtemp: invalid create response".into());
        }
        let expires_at = data
            .get("ttl")
            .and_then(|x| x.as_i64())
            .filter(|x| *x > 0)
            .map(|ttl| chrono::Utc::now().timestamp_millis() + ttl * 1000);
        Ok(EmailInfo {
            channel: Channel::Webmailtemp,
            email,
            token: Some(encode_token(&username, &cookie)?),
            expires_at,
            created_at: None,
        })
    })
}

pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let address = email.trim();
    if address.is_empty() {
        return Err("webmailtemp: empty email".into());
    }
    let (username, cookie) = decode_token(token)?;
    block_on(async {
        let resp = http_client()
            .get(format!(
                "{}/api/check/{}",
                BASE_URL,
                urlencoding::encode(&username)
            ))
            .header("Accept", "application/json")
            .header("Cookie", cookie)
            .header("User-Agent", "Mozilla/5.0")
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("webmailtemp check http {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let rows = data
            .get("emails")
            .and_then(|x| x.as_array())
            .cloned()
            .unwrap_or_default();
        Ok(rows
            .iter()
            .map(|row| normalize_email(&flatten(row, address), address))
            .collect())
    })
}
