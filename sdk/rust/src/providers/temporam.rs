/*!
 * Temporam -- https://temporam.com
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use rand::Rng;
use serde_json::{Map, Value};

const ORIGIN: &str = "https://temporam.com";

fn fetch_json(path: &str) -> Result<Value, String> {
    block_on(async {
        let resp = http_client()
            .get(format!("{}{}", ORIGIN, path))
            .header("Accept", "application/json")
            .header("Accept-Encoding", "gzip, deflate, br")
            .header("User-Agent", get_current_ua())
            .send()
            .await
            .map_err(|e| format!("temporam request failed: {}", e))?;
        if !resp.status().is_success() {
            return Err(format!("temporam http {}", resp.status()));
        }
        resp.json::<Value>()
            .await
            .map_err(|e| format!("temporam parse response: {}", e))
    })
}

fn random_local() -> String {
    const CHARS: &[u8] = b"abcdefghijklmnopqrstuvwxyz0123456789";
    let mut rng = rand::thread_rng();
    let mut out = String::from("sdk");
    for _ in 0..18 {
        out.push(CHARS[rng.gen_range(0..CHARS.len())] as char);
    }
    out
}

fn domains() -> Result<Vec<String>, String> {
    let data = fetch_json("/api/domains")?;
    let domains: Vec<String> = data["data"]
        .as_array()
        .cloned()
        .unwrap_or_default()
        .iter()
        .filter_map(|item| item["domain"].as_str())
        .map(str::trim)
        .filter(|s| !s.is_empty())
        .map(ToString::to_string)
        .collect();
    if domains.is_empty() {
        return Err("temporam: domain list is empty".into());
    }
    Ok(domains)
}

fn pick_domain(domains: &[String], preferred: Option<&str>) -> Result<String, String> {
    if let Some(wanted) = preferred {
        if !wanted.trim().is_empty() {
            if let Some(hit) = domains.iter().find(|item| item.as_str() == wanted) {
                return Ok(hit.clone());
            }
            return Err(format!("temporam: unsupported domain {}", wanted));
        }
    }
    let mut rng = rand::thread_rng();
    Ok(domains[rng.gen_range(0..domains.len())].clone())
}

fn flatten(raw: &Value, recipient: &str) -> Value {
    let mut out = Map::new();
    out.insert(
        "id".into(),
        raw.get("id")
            .or_else(|| raw.get("uuid"))
            .cloned()
            .unwrap_or(Value::String(String::new())),
    );
    out.insert(
        "from".into(),
        raw.get("fromEmail")
            .or_else(|| raw.get("from"))
            .cloned()
            .unwrap_or(Value::String(String::new())),
    );
    out.insert(
        "to".into(),
        raw.get("toEmail")
            .or_else(|| raw.get("to"))
            .cloned()
            .unwrap_or_else(|| Value::String(recipient.to_string())),
    );
    out.insert(
        "subject".into(),
        raw.get("subject")
            .cloned()
            .unwrap_or(Value::String(String::new())),
    );
    out.insert(
        "content".into(),
        raw.get("content")
            .or_else(|| raw.get("summary"))
            .cloned()
            .unwrap_or(Value::String(String::new())),
    );
    out.insert(
        "date".into(),
        raw.get("createdAt")
            .or_else(|| raw.get("created_at"))
            .or_else(|| raw.get("date"))
            .cloned()
            .unwrap_or(Value::String(String::new())),
    );
    Value::Object(out)
}

pub fn generate_email(domain: Option<&str>) -> Result<EmailInfo, String> {
    let domain = pick_domain(&domains()?, domain)?;
    let email = format!("{}@{}", random_local(), domain);
    Ok(EmailInfo {
        channel: Channel::Temporam,
        email: email.clone(),
        token: Some(email),
        expires_at: None,
        created_at: None,
    })
}

pub fn get_emails(token: Option<&str>, email: &str) -> Result<Vec<Email>, String> {
    let recipient = token.unwrap_or(email);
    if recipient.is_empty() {
        return Err("temporam: missing email".into());
    }
    let list = fetch_json(&format!(
        "/api/emails?email={}&limit=50",
        urlencoding::encode(recipient)
    ))?;
    let rows = list["data"].as_array().cloned().unwrap_or_default();
    let mut out = Vec::with_capacity(rows.len());
    for row in rows {
        let id = row["id"]
            .as_str()
            .or_else(|| row["uuid"].as_str())
            .unwrap_or("");
        let raw = if id.is_empty() {
            row
        } else {
            fetch_json(&format!("/api/emails/{}", urlencoding::encode(id)))
                .ok()
                .and_then(|detail| detail.get("data").cloned())
                .unwrap_or(row)
        };
        out.push(normalize_email(&flatten(&raw, recipient), recipient));
    }
    Ok(out)
}
