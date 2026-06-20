/*!
 * Inboxes -- https://inboxes.com
 */

use crate::config::{block_on, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use rand::Rng;
use serde_json::{Map, Value};

const API_BASE: &str = "https://inboxes.com/api/v2";
const DEFAULT_DOMAIN: &str = "blondmail.com";

fn fetch_json(url: String) -> Result<Value, String> {
    block_on(async {
        let resp = http_client()
            .get(url)
            .header("Accept", "application/json")
            .header("Origin", "https://inboxes.com")
            .header("Referer", "https://inboxes.com/")
            .header("User-Agent", "Mozilla/5.0")
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("inboxes http {}", resp.status()));
        }
        resp.json::<Value>().await.map_err(|e| e.to_string())
    })
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

fn get_domains() -> Result<Vec<String>, String> {
    let data = fetch_json(format!("{}/domain", API_BASE))?;
    let domains: Vec<String> = data
        .get("domains")
        .and_then(|x| x.as_array())
        .cloned()
        .unwrap_or_default()
        .iter()
        .filter_map(|item| item.get("qdn").and_then(|x| x.as_str()))
        .map(str::trim)
        .filter(|s| !s.is_empty())
        .map(ToString::to_string)
        .collect();
    if domains.is_empty() {
        return Err("inboxes: no domains".into());
    }
    Ok(domains)
}

fn pick_domain(domains: &[String], preferred: Option<&str>) -> String {
    let wanted = preferred
        .unwrap_or("")
        .trim()
        .trim_start_matches('@')
        .to_lowercase();
    if !wanted.is_empty() {
        if let Some(hit) = domains.iter().find(|d| d.to_lowercase() == wanted) {
            return hit.clone();
        }
    }
    domains
        .iter()
        .find(|d| d.as_str() == DEFAULT_DOMAIN)
        .cloned()
        .unwrap_or_else(|| domains[0].clone())
}

pub fn generate_email(domain: Option<&str>) -> Result<EmailInfo, String> {
    let domains = get_domains()?;
    let selected = pick_domain(&domains, domain);
    Ok(EmailInfo {
        channel: Channel::Inboxes,
        email: format!("{}@{}", random_local(), selected),
        token: None,
        expires_at: None,
        created_at: None,
    })
}

fn fetch_detail(uid: &str) -> Result<Value, String> {
    fetch_json(format!("{}/message/{}", API_BASE, urlencoding::encode(uid)))
}

fn flatten(raw: &Value, recipient: &str) -> Value {
    let mut out = Map::new();
    out.insert(
        "id".into(),
        raw.get("uid")
            .cloned()
            .unwrap_or(Value::String(String::new())),
    );
    out.insert(
        "from".into(),
        raw.get("sf")
            .or_else(|| raw.get("f"))
            .cloned()
            .unwrap_or(Value::String(String::new())),
    );
    out.insert(
        "to".into(),
        raw.get("ib")
            .cloned()
            .unwrap_or_else(|| Value::String(recipient.to_string())),
    );
    out.insert(
        "subject".into(),
        raw.get("s")
            .cloned()
            .unwrap_or(Value::String(String::new())),
    );
    out.insert(
        "text".into(),
        raw.get("text")
            .cloned()
            .unwrap_or(Value::String(String::new())),
    );
    out.insert(
        "html".into(),
        raw.get("html")
            .cloned()
            .unwrap_or(Value::String(String::new())),
    );
    if let Some(ph) = raw.get("ph") {
        out.insert("preview_text".into(), ph.clone());
    }
    if let Some(cr) = raw.get("cr") {
        out.insert("date".into(), cr.clone());
    }
    if let Some(ru) = raw.get("ru") {
        out.insert("isRead".into(), ru.clone());
    }
    out.insert(
        "attachments".into(),
        raw.get("at").cloned().unwrap_or(Value::Array(vec![])),
    );
    Value::Object(out)
}

pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let address = email.trim();
    if address.is_empty() {
        return Err("inboxes: empty email".into());
    }
    let data = fetch_json(format!(
        "{}/inbox/{}",
        API_BASE,
        urlencoding::encode(address)
    ))?;
    let rows = data
        .get("msgs")
        .and_then(|x| x.as_array())
        .cloned()
        .unwrap_or_default();
    let mut out = Vec::new();
    for row in rows {
        let raw = row
            .get("uid")
            .and_then(|x| x.as_str())
            .and_then(|uid| fetch_detail(uid).ok())
            .unwrap_or(row);
        out.push(normalize_email(&flatten(&raw, address), address));
    }
    Ok(out)
}
