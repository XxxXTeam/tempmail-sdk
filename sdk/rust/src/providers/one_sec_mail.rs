/*!
 * 1SecMail -- https://1sec-mail.com
 */

use crate::config::{block_on, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use regex::Regex;
use serde::{Deserialize, Serialize};
use serde_json::{json, Map, Value};
use std::sync::OnceLock;
use wreq::header::HeaderMap;

const BASE_URL: &str = "https://1sec-mail.com/";

#[derive(Debug, Clone, Serialize, Deserialize)]
struct Session {
    csrf: String,
    cookie: String,
}

#[derive(Debug, Deserialize)]
struct MessagesResponse {
    mailbox: Option<String>,
    messages: Option<Vec<Value>>,
}

fn csrf_re() -> &'static Regex {
    static RE: OnceLock<Regex> = OnceLock::new();
    RE.get_or_init(|| Regex::new(r#"<meta name="csrf-token" content="([^"]+)""#).unwrap())
}

fn merge_cookie(prev: &str, headers: &HeaderMap) -> String {
    let mut jar: Vec<(String, String)> = Vec::new();
    for part in prev.split(';') {
        let p = part.trim();
        if let Some((k, v)) = p.split_once('=') {
            if !k.is_empty() {
                jar.push((k.to_string(), v.to_string()));
            }
        }
    }
    for value in headers.get_all("set-cookie").iter() {
        let Ok(line) = value.to_str() else { continue };
        let first = line.split(';').next().unwrap_or("").trim();
        let Some((k, v)) = first.split_once('=') else {
            continue;
        };
        if let Some(pos) = jar.iter().position(|(name, _)| name == k) {
            jar[pos].1 = v.to_string();
        } else {
            jar.push((k.to_string(), v.to_string()));
        }
    }
    jar.into_iter()
        .map(|(k, v)| format!("{}={}", k, v))
        .collect::<Vec<_>>()
        .join("; ")
}

fn decode_session(token: &str) -> Result<Session, String> {
    let mut session: Session = serde_json::from_str(token).map_err(|e| e.to_string())?;
    session.csrf = session.csrf.trim().to_string();
    session.cookie = session.cookie.trim().to_string();
    if session.csrf.is_empty() || session.cookie.is_empty() {
        return Err("1sec-mail: invalid session token".into());
    }
    Ok(session)
}

fn open_session() -> Result<Session, String> {
    block_on(async {
        let resp = http_client()
            .get(BASE_URL)
            .header("Accept", "text/html,application/xhtml+xml")
            .header("User-Agent", "Mozilla/5.0")
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("1sec-mail home: {}", resp.status()));
        }
        let cookie = merge_cookie("", resp.headers());
        let html = resp.text().await.map_err(|e| e.to_string())?;
        let csrf = csrf_re()
            .captures(&html)
            .and_then(|caps| caps.get(1).map(|m| m.as_str().to_string()))
            .unwrap_or_default();
        if csrf.is_empty() {
            return Err("1sec-mail: csrf token not found".into());
        }
        Ok(Session { csrf, cookie })
    })
}

fn fetch_messages(session: &Session) -> Result<(MessagesResponse, String), String> {
    block_on(async {
        let resp = http_client()
            .post(format!("{}get_messages", BASE_URL))
            .header("Accept", "application/json, text/plain, */*")
            .header("Content-Type", "application/json")
            .header("X-Requested-With", "XMLHttpRequest")
            .header("X-CSRF-TOKEN", &session.csrf)
            .header("Referer", BASE_URL)
            .header("Cookie", &session.cookie)
            .header("User-Agent", "Mozilla/5.0")
            .json(&json!({"_token": session.csrf, "captcha": ""}))
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("1sec-mail messages: {}", resp.status()));
        }
        let cookie = merge_cookie(&session.cookie, resp.headers());
        let data = resp
            .json::<MessagesResponse>()
            .await
            .map_err(|e| e.to_string())?;
        Ok((data, cookie))
    })
}

fn flatten(raw: &Value, recipient: &str) -> Value {
    let mut out: Map<String, Value> = raw.as_object().cloned().unwrap_or_default();
    let content = raw
        .get("content")
        .and_then(|x| x.as_str())
        .unwrap_or("")
        .to_string();
    let html = if raw.get("html").and_then(|x| x.as_bool()) == Some(true) {
        content.clone()
    } else {
        raw.get("html")
            .and_then(|x| x.as_str())
            .unwrap_or("")
            .to_string()
    };
    out.insert(
        "id".to_string(),
        raw.get("id")
            .cloned()
            .unwrap_or(Value::String(String::new())),
    );
    out.insert(
        "from".to_string(),
        raw.get("from_email")
            .or_else(|| raw.get("from"))
            .cloned()
            .unwrap_or(Value::String(String::new())),
    );
    if !out.contains_key("to") {
        out.insert("to".to_string(), Value::String(recipient.to_string()));
    }
    out.insert(
        "subject".to_string(),
        raw.get("subject")
            .cloned()
            .unwrap_or(Value::String(String::new())),
    );
    out.insert(
        "text".to_string(),
        if raw.get("html").and_then(|x| x.as_bool()) == Some(true) {
            Value::String(String::new())
        } else {
            Value::String(content)
        },
    );
    out.insert("html".to_string(), Value::String(html));
    if let Some(date) = raw.get("receivedAt") {
        out.insert("date".to_string(), date.clone());
    }
    if let Some(read) = raw.get("is_seen") {
        out.insert("isRead".to_string(), read.clone());
    }
    if let Some(attachments) = raw.get("attachments") {
        out.insert("attachments".to_string(), attachments.clone());
    }
    Value::Object(out)
}

pub fn generate_email() -> Result<EmailInfo, String> {
    let mut session = open_session()?;
    let (data, cookie) = fetch_messages(&session)?;
    let email = data.mailbox.unwrap_or_default().trim().to_string();
    if email.is_empty() || !email.contains('@') {
        return Err("1sec-mail: invalid mailbox response".into());
    }
    session.cookie = cookie;
    Ok(EmailInfo {
        channel: Channel::OneSecMail,
        email,
        token: Some(serde_json::to_string(&session).map_err(|e| e.to_string())?),
        expires_at: None,
        created_at: None,
    })
}

pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let session = decode_session(token)?;
    let address = email.trim();
    if address.is_empty() {
        return Err("1sec-mail: empty email".into());
    }
    let (data, _) = fetch_messages(&session)?;
    Ok(data
        .messages
        .unwrap_or_default()
        .iter()
        .map(|row| normalize_email(&flatten(row, address), address))
        .collect())
}
