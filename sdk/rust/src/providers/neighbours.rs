/*!
 * Neighbours -- https://neighbours.sh
 */

use crate::config::{block_on, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use rand::Rng;
use serde_json::Value;

const API_BASE: &str = "https://neighbours.sh/api/v1";

fn request_json(path: &str, allow_not_found: bool) -> Result<Option<Value>, String> {
    block_on(async {
        let resp = http_client()
            .get(format!("{}{}", API_BASE, path))
            .header("Accept", "application/json")
            .header(
                "User-Agent",
                "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
            )
            .send()
            .await
            .map_err(|e| format!("neighbours request failed: {}", e))?;

        if allow_not_found && resp.status().as_u16() == 404 {
            return Ok(None);
        }
        if !resp.status().is_success() {
            return Err(format!("neighbours http {}", resp.status()));
        }
        let data = resp.json::<Value>().await.map_err(|e| e.to_string())?;
        Ok(Some(data))
    })
}

fn random_int(limit: usize) -> usize {
    if limit <= 1 {
        return 0;
    }
    rand::thread_rng().gen_range(0..limit)
}

fn random_local() -> String {
    const CHARS: &[u8] = b"abcdefghijklmnopqrstuvwxyz0123456789";
    let mut out = String::from("sdk");
    for _ in 0..16 {
        out.push(CHARS[random_int(CHARS.len())] as char);
    }
    out
}

fn any_string(value: &Value) -> String {
    match value {
        Value::Null => String::new(),
        Value::String(s) => s.trim().to_string(),
        Value::Number(n) => n.to_string(),
        Value::Bool(b) => b.to_string(),
        Value::Array(items) => items
            .iter()
            .map(any_string)
            .find(|s| !s.is_empty())
            .unwrap_or_default(),
        Value::Object(map) => {
            if let Some(s) = map.get("address").and_then(Value::as_str) {
                let s = s.trim();
                if !s.is_empty() {
                    return s.to_string();
                }
            }
            if let Some(s) = map.get("text").and_then(Value::as_str) {
                let s = s.trim();
                if s.contains('@') {
                    return s.to_string();
                }
            }
            if let Some(inner) = map.get("value") {
                return any_string(inner);
            }
            String::new()
        }
    }
}

fn list_domains() -> Result<Vec<String>, String> {
    let data = request_json("/config/domains", false)?.ok_or_else(|| "neighbours: missing domains response".to_string())?;
    let domains = data["data"]["domains"]
        .as_array()
        .cloned()
        .unwrap_or_default()
        .iter()
        .filter_map(Value::as_str)
        .map(str::trim)
        .map(|s| s.to_lowercase())
        .filter(|s| !s.is_empty())
        .collect::<Vec<_>>();
    if domains.is_empty() {
        return Err("neighbours: domain list is empty".into());
    }
    Ok(domains)
}

fn pick_domain(domains: &[String], preferred: Option<&str>) -> Result<String, String> {
    if let Some(wanted) = preferred {
        let wanted = wanted.trim().to_lowercase();
        if !wanted.is_empty() {
            if let Some(hit) = domains.iter().find(|item| item.as_str() == wanted) {
                return Ok(hit.clone());
            }
            return Err(format!("neighbours: unsupported domain {}", wanted));
        }
    }
    Ok(domains[random_int(domains.len())].clone())
}

fn flatten_message(raw: &Value, recipient: &str) -> Value {
    let text = any_string(raw.get("text").unwrap_or(&Value::Null));
    let text = if text.is_empty() {
        any_string(raw.get("snippet").unwrap_or(&Value::Null))
    } else {
        text
    };
    let date = any_string(raw.get("date").unwrap_or(&Value::Null));
    let date = if date.is_empty() {
        any_string(raw.get("received_at").unwrap_or(&Value::Null))
    } else {
        date
    };
    let to = any_string(raw.get("to").unwrap_or(&Value::Null));
    serde_json::json!({
        "id": any_string(raw.get("uid").unwrap_or(&Value::Null)),
        "from": any_string(raw.get("from").unwrap_or(&Value::Null)),
        "to": if to.is_empty() { recipient } else { to.as_str() },
        "subject": any_string(raw.get("subject").unwrap_or(&Value::Null)),
        "text": text,
        "html": any_string(raw.get("html").unwrap_or(&Value::Null)),
        "date": date,
        "attachments": raw.get("attachments").cloned().unwrap_or(Value::Array(vec![])),
    })
}

fn fetch_detail(address: &str, uid: &str) -> Result<Option<Value>, String> {
    request_json(
        &format!(
            "/inbox/{}/{}",
            urlencoding::encode(address),
            urlencoding::encode(uid)
        ),
        true,
    )
    .map(|opt| opt.and_then(|data| data.get("data").cloned()))
}

pub fn generate_email(domain: Option<&str>) -> Result<EmailInfo, String> {
    let domains = list_domains()?;
    let selected = pick_domain(&domains, domain)?;
    let email = format!("{}@{}", random_local(), selected);
    Ok(EmailInfo {
        channel: Channel::Neighbours,
        email: email.clone(),
        token: Some(email),
        expires_at: None,
        created_at: None,
    })
}

pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let address = email.trim();
    if address.is_empty() {
        return Err("neighbours: empty email".into());
    }
    let data = request_json(&format!("/inbox/{}", urlencoding::encode(address)), true)?;
    let Some(data) = data else {
        return Ok(vec![]);
    };
    let rows = data["data"].as_array().cloned().unwrap_or_default();
    if rows.is_empty() {
        return Ok(vec![]);
    }

    let mut out = Vec::with_capacity(rows.len());
    for row in rows {
        let raw = if let Some(uid) = row.get("uid").and_then(Value::as_str) {
            fetch_detail(address, uid)?.unwrap_or(row)
        } else {
            row
        };
        out.push(normalize_email(&flatten_message(&raw, address), address));
    }
    Ok(out)
}
