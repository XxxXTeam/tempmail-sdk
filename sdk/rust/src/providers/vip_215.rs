/*!
 * vip.215.im：POST /api/temp-inbox；收件通过 WebSocket `message.new`（独立线程 + tungstenite）
 */

use std::collections::HashMap;
use std::sync::{Arc, Mutex, OnceLock};
use std::thread;

use serde_json::Value;
use tungstenite::protocol::Message;

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};

const HTTP_BASE: &str = "https://vip.215.im";
const VIP215_SYNTHETIC_MARKER: &str = "【tempmail-sdk|synthetic|vip-215|v1】";

struct Vip215Box {
    emails: Vec<Email>,
    seen: HashMap<String, ()>,
    started: bool,
}

impl Default for Vip215Box {
    fn default() -> Self {
        Self {
            emails: Vec::new(),
            seen: HashMap::new(),
            started: false,
        }
    }
}

fn registry() -> &'static Mutex<HashMap<String, Arc<Mutex<Vip215Box>>>> {
    static REG: OnceLock<Mutex<HashMap<String, Arc<Mutex<Vip215Box>>>>> = OnceLock::new();
    REG.get_or_init(|| Mutex::new(HashMap::new()))
}

fn get_box(token: &str) -> Arc<Mutex<Vip215Box>> {
    let mut g = registry().lock().unwrap();
    g.entry(token.to_string())
        .or_insert_with(|| Arc::new(Mutex::new(Vip215Box::default())))
        .clone()
}

fn vip215_sanitize_slice(s: &str) -> String {
    s.replace("\r\n", " ")
        .replace('\r', " ")
        .replace('\n', " ")
        .trim()
        .to_string()
}

fn vip215_sanitize_val(v: Option<&Value>) -> String {
    let Some(v) = v else {
        return String::new();
    };
    let s = match v {
        Value::String(x) => x.clone(),
        Value::Number(n) => n.to_string(),
        Value::Bool(b) => b.to_string(),
        Value::Null => return String::new(),
        _ => v.to_string(),
    };
    vip215_sanitize_slice(&s)
}

fn vip215_optional_size(data: &Value) -> Option<i64> {
    let x = data.get("size")?;
    if let Some(i) = x.as_i64() {
        return (i >= 0).then_some(i);
    }
    if let Some(f) = x.as_f64() {
        if f >= 0.0 && f.is_finite() {
            return Some(f as i64);
        }
    }
    None
}

fn html_escape_attr(s: &str) -> String {
    let mut out = String::with_capacity(s.len());
    for c in s.chars() {
        match c {
            '&' => out.push_str("&amp;"),
            '<' => out.push_str("&lt;"),
            '>' => out.push_str("&gt;"),
            '"' => out.push_str("&quot;"),
            _ => out.push(c),
        }
    }
    out
}

/// 与 Node/Python/Go 一致的 synthetic-v1：可解析的纯文本 + dl/dt/dd 摘要 HTML
fn vip215_synthetic_bodies(recipient: &str, data: &Value) -> (String, String) {
    let eid = vip215_sanitize_val(data.get("id"));
    let subj = vip215_sanitize_val(data.get("subject"));
    let from = vip215_sanitize_val(data.get("from"));
    let to = vip215_sanitize_slice(recipient);
    let date = vip215_sanitize_val(data.get("date"));

    let mut lines = vec![
        VIP215_SYNTHETIC_MARKER.to_string(),
        format!("id: {eid}"),
        format!("subject: {subj}"),
        format!("from: {from}"),
        format!("to: {to}"),
        format!("date: {date}"),
    ];
    if let Some(sz) = vip215_optional_size(data) {
        lines.push(format!("size: {sz}"));
    }
    let text = lines.join("\n");

    let mut inner = String::new();
    for (k, v) in [
        ("id", eid.as_str()),
        ("subject", subj.as_str()),
        ("from", from.as_str()),
        ("to", to.as_str()),
        ("date", date.as_str()),
    ] {
        inner.push_str("<dt>");
        inner.push_str(&html_escape_attr(k));
        inner.push_str("</dt><dd>");
        inner.push_str(&html_escape_attr(v));
        inner.push_str("</dd>");
    }
    if let Some(sz) = vip215_optional_size(data) {
        inner.push_str("<dt>size</dt><dd>");
        inner.push_str(&html_escape_attr(&sz.to_string()));
        inner.push_str("</dd>");
    }
    let html = format!(
        r#"<div class="tempmail-sdk-synthetic" data-tempmail-sdk-format="synthetic-v1" data-channel="vip-215"><dl class="tempmail-sdk-meta">{inner}</dl></div>"#
    );
    (text, html)
}

fn ws_loop(token: String, recipient: String, arc: Arc<Mutex<Vip215Box>>) {
    let enc = urlencoding::encode(&token);
    let ws_url = format!("wss://vip.215.im/v1/ws?token={}", enc);
    let ua = get_current_ua();
    let request = match tungstenite::http::Request::builder()
        .uri(&ws_url)
        .header("Host", "vip.215.im")
        .header("Origin", "https://vip.215.im")
        .header("User-Agent", ua)
        .body(())
    {
        Ok(r) => r,
        Err(e) => {
            log::warn!("vip-215 ws request build failed: {}", e);
            return;
        }
    };
    let mut socket = match tungstenite::connect(request) {
        Ok((s, _)) => s,
        Err(e) => {
            log::warn!("vip-215 ws connect failed: {}", e);
            return;
        }
    };

    loop {
        let msg = match socket.read() {
            Ok(m) => m,
            Err(e) => {
                log::debug!("vip-215 ws read end: {}", e);
                break;
            }
        };
        let text = match msg {
            Message::Text(t) => t.to_string(),
            Message::Binary(b) => match String::from_utf8(b.to_vec()) {
                Ok(s) => s,
                Err(_) => continue,
            },
            Message::Close(_) => break,
            Message::Ping(_) | Message::Pong(_) | Message::Frame(_) => continue,
        };
        let outer: Value = match serde_json::from_str(&text) {
            Ok(v) => v,
            Err(_) => continue,
        };
        if outer.get("type").and_then(|v| v.as_str()) != Some("message.new") {
            continue;
        }
        let Some(data) = outer.get("data") else { continue };
        let (syn_text, syn_html) = vip215_synthetic_bodies(&recipient, data);
        let raw = serde_json::json!({
            "id": data.get("id"),
            "from": data.get("from"),
            "subject": data.get("subject"),
            "date": data.get("date"),
            "to": &recipient,
            "text": syn_text,
            "html": syn_html,
        });
        let em = normalize_email(&raw, &recipient);
        if em.id.is_empty() {
            continue;
        }
        {
            let mut inner = match arc.lock() {
                Ok(g) => g,
                Err(_) => break,
            };
            if inner.seen.contains_key(&em.id) {
                continue;
            }
            inner.seen.insert(em.id.clone(), ());
            inner.emails.push(em);
        }
    }
}

fn ensure_ws(token: &str, recipient: &str) {
    let arc = get_box(token);
    let need_start = {
        let mut b = arc.lock().unwrap();
        if b.started {
            false
        } else {
            b.started = true;
            true
        }
    };
    if need_start {
        let t = token.to_string();
        let r = recipient.to_string();
        let a = arc.clone();
        thread::spawn(move || ws_loop(t, r, a));
        thread::sleep(std::time::Duration::from_millis(80));
    }
}

pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let ua = get_current_ua();
        let resp = http_client()
            .post(format!("{}/api/temp-inbox", HTTP_BASE))
            .header("Content-Type", "application/json")
            .header("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6")
            .header("Cache-Control", "no-cache")
            .header("DNT", "1")
            .header("Origin", "https://vip.215.im")
            .header("Pragma", "no-cache")
            .header("Referer", "https://vip.215.im/")
            .header("Sec-CH-UA", r#""Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146""#)
            .header("Sec-CH-UA-Mobile", "?0")
            .header("Sec-CH-UA-Platform", r#""Windows""#)
            .header("Sec-Fetch-Dest", "empty")
            .header("Sec-Fetch-Mode", "cors")
            .header("Sec-Fetch-Site", "same-origin")
            .header("X-Locale", "zh")
            .header("User-Agent", ua)
            .send()
            .await
            .map_err(|e| format!("vip-215 create inbox: {}", e))?;

        if !resp.status().is_success() {
            let status = resp.status();
            let t = resp.text().await.unwrap_or_default();
            return Err(format!("vip-215 create inbox HTTP {} {}", status, t));
        }

        let body: Value = resp.json().await.map_err(|e| e.to_string())?;
        if body.get("success").and_then(|v| v.as_bool()) != Some(true) {
            return Err("vip-215: success=false".into());
        }
        let data = body.get("data").ok_or("vip-215: missing data")?;
        let address = data
            .get("address")
            .and_then(|v| v.as_str())
            .ok_or("vip-215: missing address")?;
        let tok = data
            .get("token")
            .and_then(|v| v.as_str())
            .ok_or("vip-215: missing token")?;

        Ok(EmailInfo {
            channel: Channel::Vip215,
            email: address.to_string(),
            token: Some(tok.to_string()),
            expires_at: None,
            created_at: data
                .get("createdAt")
                .and_then(|v| v.as_str())
                .map(|s| s.to_string()),
        })
    })
}

pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    ensure_ws(token, email);
    let arc = get_box(token);
    let b = arc.lock().map_err(|e| e.to_string())?;
    Ok(b.emails.clone())
}
