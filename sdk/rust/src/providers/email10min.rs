/*!
 * email10min — https://email10min.com
 * GET /zh 拿 CSRF + Cookie，POST /messages 获取邮件。
 */

use std::collections::BTreeMap;

use regex_lite::Regex;
use serde_json::Value;
use wreq::header::HeaderMap;

use crate::config::{block_on, http_client_no_cookie_jar};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};

const BASE: &str = "https://email10min.com";
const TOK_PREFIX: &str = "e10m:";

fn cookie_map(hdr: &str) -> BTreeMap<String, String> {
    let mut m = BTreeMap::new();
    for part in hdr.split(';') {
        let p = part.trim();
        if let Some(i) = p.find('=') {
            let k = p[..i].trim().to_string();
            let v = p[i + 1..].trim().to_string();
            if !k.is_empty() {
                m.insert(k, v);
            }
        }
    }
    m
}

fn cookie_hdr_from_map(m: &BTreeMap<String, String>) -> String {
    m.iter()
        .map(|(k, v)| format!("{}={}", k, v))
        .collect::<Vec<_>>()
        .join("; ")
}

fn merge_set_cookies(prev: &str, headers: &HeaderMap) -> String {
    let mut m = cookie_map(prev);
    for v in headers.get_all("set-cookie") {
        let s = v.to_str().unwrap_or("");
        let nv = s.split(';').next().unwrap_or("").trim();
        if let Some(i) = nv.find('=') {
            let k = nv[..i].trim().to_string();
            let val = nv[i + 1..].trim().to_string();
            if !k.is_empty() {
                m.insert(k, val);
            }
        }
    }
    cookie_hdr_from_map(&m)
}

fn encode_token(cookie: &str, csrf: &str) -> String {
    let raw = serde_json::json!({"c": cookie, "t": csrf}).to_string();
    format!(
        "{}{}",
        TOK_PREFIX,
        base64::Engine::encode(&base64::engine::general_purpose::STANDARD, raw.as_bytes())
    )
}

fn decode_token(token: &str) -> Result<(String, String), String> {
    if !token.starts_with(TOK_PREFIX) {
        return Err("email10min: invalid session token".into());
    }
    let decoded = base64::Engine::decode(
        &base64::engine::general_purpose::STANDARD,
        &token[TOK_PREFIX.len()..],
    )
    .map_err(|_| "email10min: invalid session token".to_string())?;
    let raw =
        String::from_utf8(decoded).map_err(|_| "email10min: invalid session token".to_string())?;
    let data: Value =
        serde_json::from_str(&raw).map_err(|_| "email10min: invalid session token".to_string())?;
    let cookie = data["c"].as_str().unwrap_or("").trim().to_string();
    let csrf = data["t"].as_str().unwrap_or("").trim().to_string();
    if cookie.is_empty() || csrf.is_empty() {
        return Err("email10min: invalid session token (empty fields)".into());
    }
    Ok((cookie, csrf))
}

fn extract_csrf(html: &str) -> Result<String, String> {
    let re = Regex::new(r#"<meta\s+name="csrf-token"\s+content="([^"]+)""#).unwrap();
    if let Some(m) = re.captures(html) {
        return Ok(m.get(1).unwrap().as_str().to_string());
    }
    let re2 = Regex::new(r#"<input[^>]+name="_token"[^>]+value="([^"]+)""#).unwrap();
    if let Some(m) = re2.captures(html) {
        return Ok(m.get(1).unwrap().as_str().to_string());
    }
    Err("email10min: 未找到 CSRF token".into())
}

fn extract_email_from_html(html: &str) -> Result<String, String> {
    let re1 = Regex::new(r#"id="emailAddress"[^>]*>([^<]+)"#).unwrap();
    if let Some(m) = re1.captures(html) {
        let addr = m.get(1).unwrap().as_str().trim();
        if addr.contains('@') {
            return Ok(addr.to_string());
        }
    }
    let re2 = Regex::new(r#"class="[^"]*email[^"]*"[^>]*>([^<]*@[^<]+)"#).unwrap();
    if let Some(m) = re2.captures(html) {
        return Ok(m.get(1).unwrap().as_str().trim().to_string());
    }
    let re3 = Regex::new(r#"data-email="([^"]+@[^"]+)""#).unwrap();
    if let Some(m) = re3.captures(html) {
        return Ok(m.get(1).unwrap().as_str().trim().to_string());
    }
    let re4 = Regex::new(r#"value="([a-zA-Z0-9._%+\-]+@[a-zA-Z0-9.\-]+\.[a-zA-Z]{2,})""#).unwrap();
    if let Some(m) = re4.captures(html) {
        return Ok(m.get(1).unwrap().as_str().trim().to_string());
    }
    let re5 = Regex::new(r#""mailbox"\s*:\s*"([^"]+@[^"]+)""#).unwrap();
    if let Some(m) = re5.captures(html) {
        return Ok(m.get(1).unwrap().as_str().trim().to_string());
    }
    let re6 = Regex::new(r"([a-zA-Z0-9._%+\-]+@[a-zA-Z0-9.\-]+\.[a-zA-Z]{2,})").unwrap();
    if let Some(m) = re6.captures(html) {
        let addr = m.get(1).unwrap().as_str();
        if !addr.contains("email10min") && !addr.contains("example") && !addr.contains("google") {
            return Ok(addr.trim().to_string());
        }
    }
    Err("email10min: 未找到邮箱地址".into())
}

fn browser_headers() -> Vec<(&'static str, String)> {
    vec![
        ("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8".into()),
        ("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6".into()),
        ("Cache-Control", "no-cache".into()),
        ("DNT", "1".into()),
        ("Pragma", "no-cache".into()),
        ("Upgrade-Insecure-Requests", "1".into()),
        ("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0".into()),
        ("sec-ch-ua", r#""Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146""#.into()),
        ("sec-ch-ua-mobile", "?0".into()),
        ("sec-ch-ua-platform", r#""Windows""#.into()),
    ]
}

fn ajax_headers() -> Vec<(&'static str, String)> {
    vec![
        ("Accept", "application/json, text/plain, */*".into()),
        ("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6".into()),
        ("Content-Type", "application/x-www-form-urlencoded; charset=UTF-8".into()),
        ("Origin", BASE.into()),
        ("Referer", format!("{}/zh", BASE)),
        ("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0".into()),
        ("X-Requested-With", "XMLHttpRequest".into()),
        ("sec-ch-ua", r#""Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146""#.into()),
        ("sec-ch-ua-mobile", "?0".into()),
        ("sec-ch-ua-platform", r#""Windows""#.into()),
        ("sec-fetch-dest", "empty".into()),
        ("sec-fetch-mode", "cors".into()),
        ("sec-fetch-site", "same-origin".into()),
    ]
}

fn apply_headers(
    mut req: wreq::RequestBuilder,
    pairs: &[(&'static str, String)],
) -> wreq::RequestBuilder {
    for (k, v) in pairs {
        req = req.header(*k, v);
    }
    req
}

pub fn generate_email() -> Result<EmailInfo, String> {
    let client = http_client_no_cookie_jar();
    let hdrs = browser_headers();
    block_on(async {
        let mut req = client.get(format!("{}/zh", BASE));
        req = apply_headers(req, &hdrs);
        let resp = req.send().await.map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("email10min: 创建邮箱失败 HTTP {}", resp.status()));
        }
        let headers = resp.headers().clone();
        let cookie = merge_set_cookies("", &headers);
        let html = resp.text().await.map_err(|e| e.to_string())?;

        let csrf = extract_csrf(&html)?;
        let email_addr = extract_email_from_html(&html)?;
        let token = encode_token(&cookie, &csrf);

        Ok(EmailInfo {
            channel: Channel::Email10min,
            email: email_addr,
            token: Some(token),
            expires_at: None,
            created_at: None,
        })
    })
}

pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    if token.is_empty() {
        return Err("token is required for email10min".into());
    }
    let (cookie, csrf) = decode_token(token)?;
    let client = http_client_no_cookie_jar();
    let hdrs = ajax_headers();
    let ts = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap_or_default()
        .as_millis();

    block_on(async {
        let mut req = client.post(format!("{}/messages?{}", BASE, ts));
        req = apply_headers(req, &hdrs);
        req = req.header("Cookie", &cookie);
        req = req.body(format!("_token={}&captcha=", csrf));
        let resp = req.send().await.map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("email10min: 获取邮件失败 HTTP {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let messages = match data["messages"].as_array() {
            Some(arr) => arr,
            None => return Ok(vec![]),
        };

        let mut out = Vec::new();
        for (i, raw) in messages.iter().enumerate() {
            let msg_id = raw["id"]
                .as_str()
                .or_else(|| raw["message_id"].as_str())
                .map(|s| s.to_string())
                .unwrap_or_else(|| i.to_string());
            let from = raw["from"]
                .as_str()
                .or_else(|| raw["sender"].as_str())
                .unwrap_or("");
            let to = raw["to"].as_str().unwrap_or(email);
            let subject = raw["subject"].as_str().unwrap_or("");
            let text = raw["text"]
                .as_str()
                .or_else(|| raw["body"].as_str())
                .unwrap_or("");
            let html_content = raw["html"]
                .as_str()
                .or_else(|| raw["body_html"].as_str())
                .unwrap_or("");
            let date = raw["date"]
                .as_str()
                .or_else(|| raw["created_at"].as_str())
                .unwrap_or("");

            let flat = serde_json::json!({
                "id": msg_id,
                "from": from,
                "to": to,
                "subject": subject,
                "text": text,
                "html": html_content,
                "date": date,
                "isRead": false,
            });
            out.push(normalize_email(&flat, email));
        }
        Ok(out)
    })
}
