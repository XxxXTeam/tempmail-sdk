/*!
 * Catchmail — https://catchmail.io
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use rand::Rng;
use regex::Regex;
use serde_json::{json, Value};

const API_BASE: &str = "https://api.catchmail.io/api/v1";
const DEFAULT_DOMAIN: &str = "catchmail.io";
const DOMAINS: &[&str] = &["catchmail.io", "mailistry.com", "zeppost.com"];

fn headers(b: wreq::RequestBuilder) -> wreq::RequestBuilder {
    b.header("Accept", "application/json")
        .header("Referer", "https://catchmail.io/")
        .header("Origin", "https://catchmail.io")
        .header("User-Agent", get_current_ua())
}

fn random_local() -> String {
    const CHARS: &[u8] = b"abcdefghijklmnopqrstuvwxyz0123456789";
    let mut rng = rand::thread_rng();
    let mut out = String::from("sdk");
    for _ in 0..16 {
        out.push(CHARS[rng.gen_range(0..CHARS.len())] as char);
    }
    out
}

fn pick_domain(preferred: Option<&str>) -> &'static str {
    if let Some(p) = preferred {
        let wanted = p.trim().trim_start_matches('@').to_lowercase();
        if let Some(hit) = DOMAINS.iter().find(|d| d.to_lowercase() == wanted) {
            return hit;
        }
    }
    DEFAULT_DOMAIN
}

fn clean_address(value: Option<&Value>) -> String {
    let Some(v) = value else {
        return String::new();
    };
    if let Some(arr) = v.as_array() {
        return clean_address(arr.first());
    }
    let text = v
        .as_str()
        .map(str::to_string)
        .unwrap_or_else(|| v.to_string());
    let text = text.trim().to_string();
    if let Ok(re) = Regex::new(r"<([^>]+)>") {
        if let Some(caps) = re.captures(&text) {
            if let Some(m) = caps.get(1) {
                return m.as_str().trim().to_string();
            }
        }
    }
    text
}

fn flatten_detail(raw: &Value, recipient: &str) -> Value {
    let body = raw.get("body").unwrap_or(&Value::Null);
    let to = {
        let value = clean_address(raw.get("to"));
        if value.is_empty() {
            recipient.to_string()
        } else {
            value
        }
    };
    json!({
        "id": raw.get("id").cloned().unwrap_or(Value::String(String::new())),
        "from": clean_address(raw.get("from")),
        "to": to,
        "subject": raw.get("subject").cloned().unwrap_or(Value::String(String::new())),
        "text": body.get("text").cloned().unwrap_or(Value::String(String::new())),
        "html": body.get("html").cloned().unwrap_or(Value::String(String::new())),
        "date": raw.get("date").cloned().unwrap_or(Value::String(String::new())),
        "attachments": raw.get("attachments").cloned().unwrap_or(Value::Array(vec![])),
    })
}

fn fetch_message(message_id: &str, email: &str) -> Result<Value, String> {
    block_on(async {
        let url = format!(
            "{}/message/{}?mailbox={}",
            API_BASE,
            urlencoding::encode(message_id),
            urlencoding::encode(email)
        );
        let resp = headers(http_client().get(url))
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("catchmail message {}", resp.status()));
        }
        resp.json::<Value>().await.map_err(|e| e.to_string())
    })
}

pub fn generate_email(domain: Option<&str>) -> Result<EmailInfo, String> {
    let email = format!("{}@{}", random_local(), pick_domain(domain));
    block_on(async {
        let url = format!(
            "{}/mailbox?address={}",
            API_BASE,
            urlencoding::encode(&email)
        );
        let resp = headers(http_client().get(url))
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("catchmail create mailbox {}", resp.status()));
        }
        Ok(EmailInfo {
            channel: Channel::Catchmail,
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
        return Err("catchmail: empty email".into());
    }
    block_on(async {
        let url = format!("{}/mailbox?address={}", API_BASE, urlencoding::encode(em));
        let resp = headers(http_client().get(url))
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("catchmail mailbox {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let rows = data
            .get("messages")
            .and_then(|x| x.as_array())
            .cloned()
            .unwrap_or_default();
        let mut out = Vec::new();
        for item in rows {
            let id = item.get("id").and_then(|x| x.as_str()).unwrap_or("").trim();
            if id.is_empty() {
                continue;
            }
            if let Ok(detail) = fetch_message(id, em) {
                out.push(normalize_email(&flatten_detail(&detail, em), em));
                continue;
            }
            let flat = json!({
                "id": id,
                "from": clean_address(item.get("from")),
                "to": item.get("mailbox").cloned().unwrap_or(Value::String(em.to_string())),
                "subject": item.get("subject").cloned().unwrap_or(Value::String(String::new())),
                "date": item.get("date").cloned().unwrap_or(Value::String(String::new())),
            });
            out.push(normalize_email(&flat, em));
        }
        Ok(out)
    })
}
