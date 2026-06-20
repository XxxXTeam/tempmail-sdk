/*!
 * InboxKitten — https://inboxkitten.com
 */

use crate::config::{block_on, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use rand::Rng;
use serde_json::{json, Value};

const API_BASE: &str = "https://inboxkitten.com/api/v1/mail";
const DOMAIN: &str = "inboxkitten.com";

fn random_local() -> String {
    const CHARS: &[u8] = b"abcdefghijklmnopqrstuvwxyz0123456789";
    let mut rng = rand::thread_rng();
    let mut out = String::from("sdk");
    for _ in 0..16 {
        out.push(CHARS[rng.gen_range(0..CHARS.len())] as char);
    }
    out
}

fn html_to_text(html: &str) -> String {
    let mut out = String::with_capacity(html.len());
    let mut in_tag = false;
    let mut in_script = false;
    let bytes = html.as_bytes();
    let mut i = 0;
    while i < bytes.len() {
        let rest = &html[i..];
        if rest.to_ascii_lowercase().starts_with("<script") {
            in_script = true;
            in_tag = true;
        }
        if in_script && rest.to_ascii_lowercase().starts_with("</script") {
            in_script = false;
        }
        let ch = rest.chars().next().unwrap_or(' ');
        if ch == '<' {
            in_tag = true;
            out.push(' ');
        } else if ch == '>' {
            in_tag = false;
            out.push(' ');
        } else if !in_tag && !in_script {
            out.push(ch);
        }
        i += ch.len_utf8();
    }
    out.replace("&nbsp;", " ")
        .replace("&amp;", "&")
        .replace("&lt;", "<")
        .replace("&gt;", ">")
        .replace("&quot;", "\"")
        .replace("&#39;", "'")
        .split_whitespace()
        .collect::<Vec<_>>()
        .join(" ")
}

fn local_part(email: &str) -> String {
    email.trim().split('@').next().unwrap_or("").to_string()
}

fn fetch_json(url: String) -> Result<Value, String> {
    block_on(async {
        let resp = http_client()
            .get(url)
            .header("Accept", "application/json")
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("inboxkitten http {}", resp.status()));
        }
        resp.json::<Value>().await.map_err(|e| e.to_string())
    })
}

fn fetch_html(region: &str, key: &str) -> Result<String, String> {
    block_on(async {
        let url = format!(
            "{}/getHtml?region={}&key={}",
            API_BASE,
            urlencoding::encode(region),
            urlencoding::encode(key)
        );
        let resp = http_client()
            .get(url)
            .header("Accept", "text/html,*/*")
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("inboxkitten html {}", resp.status()));
        }
        resp.text().await.map_err(|e| e.to_string())
    })
}

fn detail_raw(row: &Value, recipient: &str) -> Value {
    let region = row
        .pointer("/storage/region")
        .and_then(|x| x.as_str())
        .unwrap_or("");
    let key = row
        .pointer("/storage/key")
        .and_then(|x| x.as_str())
        .unwrap_or("");
    let mut raw = json!({
        "id": key,
        "messageId": key,
        "from": row.pointer("/message/headers/from").and_then(|x| x.as_str())
            .or_else(|| row.pointer("/envelope/sender").and_then(|x| x.as_str()))
            .unwrap_or(""),
        "to": row.get("recipient").and_then(|x| x.as_str())
            .or_else(|| row.pointer("/envelope/targets").and_then(|x| x.as_str()))
            .unwrap_or(recipient),
        "subject": row.pointer("/message/headers/subject").and_then(|x| x.as_str()).unwrap_or(""),
        "timestamp": row.get("timestamp").cloned().unwrap_or(Value::Null),
    });
    if region.is_empty() || key.is_empty() {
        return raw;
    }

    let meta_url = format!(
        "{}/getKey?region={}&key={}",
        API_BASE,
        urlencoding::encode(region),
        urlencoding::encode(key)
    );
    if let Ok(meta) = fetch_json(meta_url) {
        if let Some(name) = meta.get("name").and_then(|x| x.as_str()) {
            raw["from"] = Value::String(name.to_string());
        }
        if let Some(recipients) = meta.get("recipients").and_then(|x| x.as_str()) {
            raw["to"] = Value::String(recipients.to_string());
        }
        if let Some(subject) = meta.get("subject").and_then(|x| x.as_str()) {
            raw["subject"] = Value::String(subject.to_string());
        }
    }
    if let Ok(html) = fetch_html(region, key) {
        raw["text"] = Value::String(html_to_text(&html));
        raw["html"] = Value::String(html);
    }
    raw
}

pub fn generate_email() -> Result<EmailInfo, String> {
    Ok(EmailInfo {
        channel: Channel::Inboxkitten,
        email: format!("{}@{}", random_local(), DOMAIN),
        token: None,
        expires_at: None,
        created_at: None,
    })
}

pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let local = local_part(email);
    if local.is_empty() {
        return Err("inboxkitten: empty email".into());
    }
    let url = format!(
        "{}/list?recipient={}",
        API_BASE,
        urlencoding::encode(&local)
    );
    let rows = fetch_json(url)?;
    let items = rows.as_array().cloned().unwrap_or_default();
    let mut out = Vec::new();
    for item in items {
        out.push(normalize_email(&detail_raw(&item, email), email));
    }
    Ok(out)
}
