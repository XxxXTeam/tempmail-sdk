/*!
 * smail.pw：POST _root.data intent=generate + __session；GET 拉取 RSC 邮件
 */

use std::sync::OnceLock;

use regex::Regex;
use serde_json::{json, Value};
use crate::types::{Channel, EmailInfo, Email};
use crate::normalize::normalize_email;
use crate::config::{http_client, block_on, get_current_ua};

const ROOT_DATA_URL: &str = "https://smail.pw/_root.data";

fn re_quoted_inbox() -> &'static Regex {
    static R: OnceLock<Regex> = OnceLock::new();
    R.get_or_init(|| Regex::new(r#"(?i)"([a-z0-9][a-z0-9.-]*@smail\.pw)""#).unwrap())
}

fn re_plain_inbox() -> &'static Regex {
    static R: OnceLock<Regex> = OnceLock::new();
    R.get_or_init(|| Regex::new(r"(?i)\b([a-z0-9][a-z0-9.-]*@smail\.pw)\b").unwrap())
}

fn re_mail_block() -> &'static Regex {
    static R: OnceLock<Regex> = OnceLock::new();
    R.get_or_init(|| {
        Regex::new(
            r#""id","([^"]+)","to_address","([^"]*)","from_name","([^"]*)","from_address","([^"]*)","subject","([^"]*)","time",(\d+)"#,
        )
        .unwrap()
    })
}

fn re_mail_block_no_subj() -> &'static Regex {
    static R: OnceLock<Regex> = OnceLock::new();
    R.get_or_init(|| {
        Regex::new(
            r#""id","([^"]+)","to_address","([^"]*)","from_name","([^"]*)","from_address","([^"]*)","subject","time",(\d+)"#,
        )
        .unwrap()
    })
}

fn re_mail_block2() -> &'static Regex {
    static R: OnceLock<Regex> = OnceLock::new();
    R.get_or_init(|| {
        Regex::new(
            r#""id","([^"]+)","from_name","([^"]*)","from_address","([^"]*)","subject","([^"]*)","time",(\d+)"#,
        )
        .unwrap()
    })
}

fn smail_headers() -> Vec<(&'static str, String)> {
    vec![
        ("Accept", "*/*".into()),
        ("accept-language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6".into()),
        ("cache-control", "no-cache".into()),
        ("dnt", "1".into()),
        ("origin", "https://smail.pw".into()),
        ("pragma", "no-cache".into()),
        ("referer", "https://smail.pw/".into()),
        ("sec-ch-ua", r#""Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146""#.into()),
        ("sec-ch-ua-mobile", "?0".into()),
        ("sec-ch-ua-platform", r#""Windows""#.into()),
        ("sec-fetch-dest", "empty".into()),
        ("sec-fetch-mode", "cors".into()),
        ("sec-fetch-site", "same-origin".into()),
        ("User-Agent", get_current_ua().to_string()),
    ]
}

fn apply_headers(
    b: wreq::RequestBuilder,
    extra: &[(&str, &str)],
) -> wreq::RequestBuilder {
    let mut r = b;
    for (k, v) in smail_headers() {
        r = r.header(k, v);
    }
    for (k, v) in extra {
        r = r.header(*k, *v);
    }
    r
}

fn extract_inbox(text: &str) -> Option<String> {
    re_quoted_inbox()
        .captures(text)
        .and_then(|c| c.get(1).map(|m| m.as_str().to_string()))
        .or_else(|| {
            re_plain_inbox()
                .captures(text)
                .and_then(|c| c.get(1).map(|m| m.as_str().to_string()))
        })
}

fn is_flight_deferred(obj: &serde_json::Map<String, Value>) -> bool {
    !obj.is_empty() && obj.keys().all(|k| k.starts_with('_'))
}

fn resolve_flight_deferred(root: &[Value], obj: &serde_json::Map<String, Value>) -> Option<serde_json::Map<String, Value>> {
    let mut fields = serde_json::Map::new();
    for (k, v) in obj {
        if !k.starts_with('_') {
            continue;
        }
        let key_idx: usize = k[1..].parse().ok()?;
        let val_idx = v.as_u64()? as usize;
        let key = root.get(key_idx)?.as_str()?;
        let val = root.get(val_idx)?;
        if key == "time" {
            if let Some(n) = val.as_i64().or_else(|| val.as_str().and_then(|s| s.parse().ok())) {
                fields.insert("time".into(), json!(n));
            }
        } else if let Some(s) = val.as_str() {
            fields.insert(key.to_string(), json!(s));
        }
    }
    if !fields.contains_key("id") && !fields.contains_key("time") {
        return None;
    }
    Some(fields)
}

fn row_to_mail(fields: &serde_json::Map<String, Value>, recipient: &str) -> Option<Value> {
    let time_ms = fields.get("time")?.as_i64()?;
    let to_addr = fields
        .get("to_address")
        .and_then(|v| v.as_str())
        .unwrap_or(recipient);
    let mut subject = fields
        .get("subject")
        .and_then(|v| v.as_str())
        .unwrap_or("")
        .to_string();
    if subject == to_addr || subject.to_lowercase().ends_with("@smail.pw") {
        subject.clear();
    }
    Some(json!({
        "id": fields.get("id").and_then(|v| v.as_str()).unwrap_or(""),
        "to_address": to_addr,
        "from_name": fields.get("from_name").and_then(|v| v.as_str()).unwrap_or(""),
        "from_address": fields.get("from_address").and_then(|v| v.as_str()).unwrap_or(""),
        "subject": subject,
        "date": time_ms,
        "text": "",
        "html": "",
        "attachments": Value::Array(vec![]),
    }))
}

fn parse_flight_root(root: &[Value], recipient: &str) -> Vec<Value> {
    let mut out = Vec::new();
    for i in 0..root.len().saturating_sub(1) {
        if root[i].as_str() != Some("emails") {
            continue;
        }
        let Some(refs) = root[i + 1].as_array() else { break };
        for r in refs {
            let Some(idx) = r.as_u64() else { continue };
            let Some(node) = root.get(idx as usize) else { continue };
            if let Value::Object(obj) = node {
                if is_flight_deferred(obj) {
                    if let Some(fields) = resolve_flight_deferred(root, obj) {
                        if let Some(mail) = row_to_mail(&fields, recipient) {
                            out.push(mail);
                        }
                    }
                }
            }
        }
        break;
    }
    out
}

fn parse_mail_values(text: &str, recipient: &str) -> Vec<Value> {
    let mut out = Vec::new();
    for cap in re_mail_block().captures_iter(text) {
        let ts: i64 = cap[6].parse().unwrap_or(0);
        out.push(json!({
            "id": &cap[1],
            "to_address": &cap[2],
            "from_name": &cap[3],
            "from_address": &cap[4],
            "subject": &cap[5],
            "date": ts,
            "text": "",
            "html": "",
            "attachments": Value::Array(vec![]),
        }));
    }
    for cap in re_mail_block_no_subj().captures_iter(text) {
        let ts: i64 = cap[5].parse().unwrap_or(0);
        out.push(json!({
            "id": &cap[1],
            "to_address": &cap[2],
            "from_name": &cap[3],
            "from_address": &cap[4],
            "subject": "",
            "date": ts,
            "text": "",
            "html": "",
            "attachments": Value::Array(vec![]),
        }));
    }
    if !out.is_empty() {
        return out;
    }
    if let Ok(Value::Array(root)) = serde_json::from_str::<Value>(text) {
        let flight = parse_flight_root(&root, recipient);
        if !flight.is_empty() {
            return flight;
        }
    }
    for cap in re_mail_block2().captures_iter(text) {
        let ts: i64 = cap[5].parse().unwrap_or(0);
        out.push(json!({
            "id": &cap[1],
            "to_address": recipient,
            "from_name": &cap[2],
            "from_address": &cap[3],
            "subject": &cap[4],
            "date": ts,
            "text": "",
            "html": "",
            "attachments": Value::Array(vec![]),
        }));
    }
    out
}

pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let req = apply_headers(
            http_client()
                .post(ROOT_DATA_URL)
                .header(
                    "Content-Type",
                    "application/x-www-form-urlencoded;charset=UTF-8",
                )
                .body("intent=generate"),
            &[],
        );

        let resp = req.send().await.map_err(|e| format!("smail.pw generate: {}", e))?;
        if !resp.status().is_success() {
            return Err(format!("smail.pw generate HTTP {}", resp.status()));
        }

        let mut cookie_header = String::new();
        for c in resp.cookies() {
            if c.name() == "__session" {
                cookie_header = format!("__session={}", c.value());
                break;
            }
        }
        if cookie_header.is_empty() {
            return Err("failed to extract __session cookie".to_string());
        }

        let text = resp.text().await.map_err(|e| e.to_string())?;
        let email = extract_inbox(&text).ok_or_else(|| "failed to parse inbox".to_string())?;

        Ok(EmailInfo {
            channel: Channel::SmailPw,
            email,
            token: Some(cookie_header),
            expires_at: None,
            created_at: None,
        })
    })
}

pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    block_on(async {
        let req = apply_headers(
            http_client().get(ROOT_DATA_URL).header("Cookie", token),
            &[],
        );
        let resp = req.send().await.map_err(|e| format!("smail.pw poll: {}", e))?;
        if !resp.status().is_success() {
            return Err(format!("smail.pw poll HTTP {}", resp.status()));
        }
        let text = resp.text().await.map_err(|e| e.to_string())?;
        let vals = parse_mail_values(&text, email);
        let mut emails: Vec<Email> = vals.iter().map(|v| normalize_email(v, email)).collect();
        for em in &mut emails {
            if em.id.is_empty() || em.id.starts_with("__smail_") {
                continue;
            }
            let url = format!("https://smail.pw/api/email/{}", urlencoding::encode(&em.id));
            let req = apply_headers(
                http_client().get(&url).header("Cookie", token),
                &[("Accept", "application/json")],
            );
            if let Ok(resp) = req.send().await {
                if resp.status().is_success() {
                    if let Ok(v) = resp.json::<Value>().await {
                        if let Some(body) = v.get("body").and_then(|b| b.as_str()) {
                            em.html = body.to_string();
                        }
                    }
                }
            }
        }
        Ok(emails)
    })
}
