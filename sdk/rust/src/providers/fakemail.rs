/*!
 * FakeMail -- https://www.fakemail.net
 */

use crate::config::{block_on, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use regex::Regex;
use serde::Deserialize;
use serde_json::{Map, Value};
use std::sync::OnceLock;
use wreq::header::HeaderMap;

const BASE_URL: &str = "https://www.fakemail.net";

#[derive(Debug, Deserialize)]
struct GenerateResponse {
    email: Option<String>,
}

fn csrf_re() -> &'static Regex {
    static RE: OnceLock<Regex> = OnceLock::new();
    RE.get_or_init(|| Regex::new(r#"const\s+CSRF\s*=\s*"([^"]+)""#).unwrap())
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

fn clean_json(text: &str) -> &str {
    text.strip_prefix('\u{feff}').unwrap_or(text)
}

fn open_session() -> Result<(String, String), String> {
    block_on(async {
        let resp = http_client()
            .get(format!("{}/", BASE_URL))
            .header("Accept", "text/html,application/xhtml+xml")
            .header("User-Agent", "Mozilla/5.0")
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("fakemail home: {}", resp.status()));
        }
        let cookie = merge_cookie("", resp.headers());
        let html = resp.text().await.map_err(|e| e.to_string())?;
        let csrf = csrf_re()
            .captures(&html)
            .and_then(|caps| caps.get(1).map(|m| m.as_str().to_string()))
            .unwrap_or_default();
        if csrf.is_empty() {
            return Err("fakemail: csrf token not found".into());
        }
        Ok((csrf, cookie))
    })
}

fn create_address(csrf: &str, cookie: &str) -> Result<(GenerateResponse, String), String> {
    block_on(async {
        let resp = http_client()
            .get(format!(
                "{}/index/index?csrf_token={}",
                BASE_URL,
                urlencoding::encode(csrf)
            ))
            .header("Accept", "application/json, text/javascript, */*; q=0.01")
            .header("X-Requested-With", "XMLHttpRequest")
            .header("Referer", format!("{}/", BASE_URL))
            .header("Cookie", cookie)
            .header("User-Agent", "Mozilla/5.0")
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("fakemail generate: {}", resp.status()));
        }
        let next_cookie = merge_cookie(cookie, resp.headers());
        let text = resp.text().await.map_err(|e| e.to_string())?;
        let data = serde_json::from_str::<GenerateResponse>(clean_json(&text))
            .map_err(|e| e.to_string())?;
        Ok((data, next_cookie))
    })
}

fn fetch_rows(cookie: &str) -> Result<Vec<Value>, String> {
    block_on(async {
        let resp = http_client()
            .get(format!("{}/index/refresh", BASE_URL))
            .header("Accept", "application/json, text/javascript, */*; q=0.01")
            .header("X-Requested-With", "XMLHttpRequest")
            .header("Referer", format!("{}/", BASE_URL))
            .header("Cookie", cookie)
            .header("User-Agent", "Mozilla/5.0")
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("fakemail refresh: {}", resp.status()));
        }
        let text = resp.text().await.map_err(|e| e.to_string())?;
        serde_json::from_str::<Vec<Value>>(clean_json(&text)).map_err(|e| e.to_string())
    })
}

fn fetch_detail(cookie: &str, id: &str) -> Result<Value, String> {
    block_on(async {
        let resp = http_client()
            .post(format!("{}/index/email", BASE_URL))
            .header("Accept", "application/json, text/javascript, */*; q=0.01")
            .header("X-Requested-With", "XMLHttpRequest")
            .header("Referer", format!("{}/", BASE_URL))
            .header("Cookie", cookie)
            .header("Content-Type", "application/x-www-form-urlencoded")
            .header("User-Agent", "Mozilla/5.0")
            .form(&[("id", id)])
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("fakemail detail: {}", resp.status()));
        }
        let text = resp.text().await.map_err(|e| e.to_string())?;
        serde_json::from_str::<Value>(clean_json(&text)).map_err(|e| e.to_string())
    })
}

fn flatten(row: &Value, detail: Option<&Value>, recipient: &str) -> Value {
    let detail_obj = detail.and_then(|x| x.as_object());
    let mut out = Map::new();
    out.insert(
        "id".into(),
        detail_obj
            .and_then(|m| m.get("id").cloned())
            .or_else(|| row.get("id").cloned())
            .unwrap_or(Value::String(String::new())),
    );
    out.insert(
        "from".into(),
        detail_obj
            .and_then(|m| m.get("od").cloned())
            .or_else(|| row.get("od").cloned())
            .unwrap_or(Value::String(String::new())),
    );
    out.insert("to".into(), Value::String(recipient.to_string()));
    out.insert(
        "subject".into(),
        detail_obj
            .and_then(|m| m.get("predmet").cloned())
            .or_else(|| row.get("predmet").cloned())
            .unwrap_or(Value::String(String::new())),
    );
    out.insert("text".into(), Value::String(String::new()));
    out.insert(
        "html".into(),
        detail_obj
            .and_then(|m| m.get("telo").cloned())
            .unwrap_or(Value::String(String::new())),
    );
    out.insert(
        "date".into(),
        row.get("kdy")
            .cloned()
            .unwrap_or(Value::String(String::new())),
    );
    out.insert(
        "isRead".into(),
        Value::Bool(row.get("precteno").and_then(|x| x.as_str()) == Some("precteno")),
    );
    Value::Object(out)
}

pub fn generate_email() -> Result<EmailInfo, String> {
    let (csrf, cookie) = open_session()?;
    let (data, cookie) = create_address(&csrf, &cookie)?;
    let email = data.email.unwrap_or_default().trim().to_string();
    if email.is_empty() || !email.contains('@') {
        return Err("fakemail: invalid mailbox response".into());
    }
    Ok(EmailInfo {
        channel: Channel::Fakemail,
        email,
        token: Some(cookie),
        expires_at: None,
        created_at: None,
    })
}

pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let cookie = token.trim();
    let address = email.trim();
    if cookie.is_empty() {
        return Err("fakemail: empty session token".into());
    }
    if address.is_empty() {
        return Err("fakemail: empty email".into());
    }
    let rows = fetch_rows(cookie)?;
    let mut out = Vec::new();
    for row in rows {
        let id = row.get("id").map(|x| x.to_string()).unwrap_or_default();
        let detail = if id.trim_matches('"').is_empty() {
            None
        } else {
            fetch_detail(cookie, id.trim_matches('"')).ok()
        };
        out.push(normalize_email(
            &flatten(&row, detail.as_ref(), address),
            address,
        ));
    }
    Ok(out)
}
