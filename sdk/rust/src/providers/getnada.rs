/*!
 * GetNada -- https://getnada.net
 */

use crate::config::{block_on, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use rand::Rng;
use serde_json::{json, Map, Value};

const API_BASE: &str = "https://getnada.net/api";

fn random_local() -> String {
    const CHARS: &[u8] = b"abcdefghijklmnopqrstuvwxyz0123456789";
    let mut rng = rand::thread_rng();
    let mut out = String::from("sdk");
    for _ in 0..16 {
        out.push(CHARS[rng.gen_range(0..CHARS.len())] as char);
    }
    out
}

fn pick_domain(preferred: Option<&str>) -> Result<String, String> {
    block_on(async {
        let resp = http_client()
            .get(format!("{}/public/domains", API_BASE))
            .header("Accept", "application/json")
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("getnada domains {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let domains: Vec<String> = data
            .get("domains")
            .and_then(|x| x.as_array())
            .cloned()
            .unwrap_or_default()
            .into_iter()
            .filter_map(|domain| domain.as_str().map(|x| x.trim().to_lowercase()))
            .filter(|domain| !domain.is_empty())
            .collect();
        if let Some(preferred) = preferred {
            let wanted = preferred.trim().trim_start_matches('@').to_lowercase();
            if let Some(found) = domains.iter().find(|domain| **domain == wanted) {
                return Ok(found.clone());
            }
            return Err(format!("getnada: domain not available: {}", wanted));
        }
        for domain in &domains {
            if domain == "getnada.net" {
                return Ok("getnada.net".to_string());
            }
        }
        if let Some(first) = domains.first() {
            return Ok(first.clone());
        }
        Err("getnada: no domain available".into())
    })
}

fn flatten_message(raw: &Value, recipient: &str) -> Value {
    let mut out: Map<String, Value> = raw.as_object().cloned().unwrap_or_default();
    out.insert(
        "id".to_string(),
        raw.get("id")
            .cloned()
            .unwrap_or(Value::String(String::new())),
    );
    if let Some(v) = raw.get("from_addr").or_else(|| raw.get("from")) {
        out.insert("from".to_string(), v.clone());
    }
    if raw
        .get("to")
        .and_then(|x| x.as_str())
        .unwrap_or("")
        .is_empty()
    {
        out.insert("to".to_string(), Value::String(recipient.to_string()));
    }
    if let Some(v) = raw.get("text_plain").or_else(|| raw.get("text")) {
        out.insert("text".to_string(), v.clone());
    }
    if let Some(v) = raw.get("html_sanitized").or_else(|| raw.get("html")) {
        out.insert("html".to_string(), v.clone());
    }
    if let Some(v) = raw.get("read_at") {
        let read = match v {
            Value::String(s) => !s.trim().is_empty(),
            Value::Null => false,
            _ => true,
        };
        out.insert("read".to_string(), Value::Bool(read));
    }
    Value::Object(out)
}

pub fn generate_email(domain: Option<&str>) -> Result<EmailInfo, String> {
    let selected_domain = pick_domain(domain)?;
    let requested = format!("{}@{}", random_local(), selected_domain);
    block_on(async {
        let resp = http_client()
            .post(format!("{}/inbox/open", API_BASE))
            .header("Accept", "application/json")
            .header("Content-Type", "application/json")
            .json(&json!({ "email": requested }))
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("getnada open {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let token = data
            .get("token")
            .and_then(|x| x.as_str())
            .unwrap_or("")
            .trim()
            .to_string();
        let email = data
            .get("recipient")
            .and_then(|x| x.as_str())
            .unwrap_or(&requested)
            .trim()
            .to_string();
        if token.is_empty() || email.is_empty() || !email.contains('@') {
            return Err("getnada: invalid open response".into());
        }
        Ok(EmailInfo {
            channel: Channel::Getnada,
            email,
            token: Some(token),
            expires_at: None,
            created_at: None,
        })
    })
}

fn fetch_detail(token: &str, message_id: &str) -> Result<Value, String> {
    block_on(async {
        let url = format!(
            "{}/inbox/message?id={}&token={}",
            API_BASE,
            urlencoding::encode(message_id),
            urlencoding::encode(token)
        );
        let resp = http_client()
            .get(url)
            .header("Accept", "application/json")
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("getnada message {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        Ok(data.get("message").cloned().unwrap_or(Value::Null))
    })
}

pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let auth = token.trim();
    let address = email.trim();
    if auth.is_empty() {
        return Err("getnada: empty token".into());
    }
    if address.is_empty() {
        return Err("getnada: empty email".into());
    }
    block_on(async {
        let url = format!(
            "{}/inbox/messages?token={}",
            API_BASE,
            urlencoding::encode(auth)
        );
        let resp = http_client()
            .get(url)
            .header("Accept", "application/json")
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("getnada messages {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let items = data
            .get("messages")
            .and_then(|x| x.as_array())
            .cloned()
            .unwrap_or_default();
        let mut out = Vec::new();
        for item in items {
            let raw = item
                .get("id")
                .and_then(|x| x.as_str())
                .and_then(|id| fetch_detail(auth, id).ok())
                .filter(|x| !x.is_null())
                .unwrap_or(item);
            out.push(normalize_email(&flatten_message(&raw, address), address));
        }
        Ok(out)
    })
}
