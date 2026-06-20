/*!
 * Tempmailo — https://tempmailo.com
 */

use std::collections::BTreeMap;
use std::sync::LazyLock;

use regex::Regex;
use serde::{Deserialize, Serialize};
use serde_json::Value;
use wreq::header::HeaderMap;

use crate::config::{block_on, get_current_ua, http_client_no_cookie_jar};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};

const BASE_URL: &str = "https://tempmailo.com";

static TOKEN_RE: LazyLock<Regex> = LazyLock::new(|| {
    Regex::new(r#"(?i)name="__RequestVerificationToken"[^>]*value="([^"]+)""#).expect("re")
});
static TOKEN_RE_REV: LazyLock<Regex> = LazyLock::new(|| {
    Regex::new(r#"(?i)value="([^"]+)"[^>]*name="__RequestVerificationToken""#).expect("re")
});

#[derive(Serialize, Deserialize)]
struct TempmailoSession {
    #[serde(rename = "verificationToken")]
    verification_token: String,
    cookie: String,
}

fn parse_cookie_header(hdr: &str) -> BTreeMap<String, String> {
    let mut m = BTreeMap::new();
    for part in hdr.split(';') {
        let part = part.trim();
        if let Some(i) = part.find('=') {
            let k = part[..i].trim();
            let v = part[i + 1..].trim();
            if !k.is_empty() {
                m.insert(k.to_string(), v.to_string());
            }
        }
    }
    m
}

fn merge_set_cookies(prev: &str, headers: &HeaderMap) -> String {
    let mut m = parse_cookie_header(prev);
    for val in headers.get_all("set-cookie") {
        let Ok(s) = val.to_str() else {
            continue;
        };
        let first = s.split(';').next().unwrap_or("");
        if let Some(i) = first.find('=') {
            let k = first[..i].trim();
            let v = first[i + 1..].trim();
            if !k.is_empty() {
                m.insert(k.to_string(), v.to_string());
            }
        }
    }
    m.iter()
        .map(|(k, v)| format!("{}={}", k, v))
        .collect::<Vec<_>>()
        .join("; ")
}

fn page_headers(req: wreq::RequestBuilder, cookie: &str) -> wreq::RequestBuilder {
    let req = req
        .header("User-Agent", get_current_ua())
        .header("Accept", "text/html,application/json,*/*;q=0.8")
        .header("Referer", format!("{}/", BASE_URL));
    if cookie.is_empty() {
        req
    } else {
        req.header("Cookie", cookie)
    }
}

fn parse_verification_token(html: &str) -> Result<String, String> {
    TOKEN_RE
        .captures(html)
        .or_else(|| TOKEN_RE_REV.captures(html))
        .and_then(|cap| cap.get(1).map(|m| m.as_str().to_string()))
        .ok_or_else(|| "tempmailo: verification token not found".to_string())
}

fn encode_session(s: &TempmailoSession) -> Result<String, String> {
    serde_json::to_string(s).map_err(|e| e.to_string())
}

fn decode_session(token: &str) -> Result<TempmailoSession, String> {
    let s: TempmailoSession =
        serde_json::from_str(token).map_err(|_| "tempmailo: invalid session token".to_string())?;
    if s.verification_token.trim().is_empty() || s.cookie.trim().is_empty() {
        return Err("tempmailo: invalid session token".into());
    }
    Ok(s)
}

pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let client = http_client_no_cookie_jar();
        let home = page_headers(client.get(BASE_URL), "")
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !home.status().is_success() {
            return Err(format!("tempmailo home: {}", home.status()));
        }
        let mut cookie = merge_set_cookies("", home.headers());
        let html = home.text().await.map_err(|e| e.to_string())?;
        let verification_token = parse_verification_token(&html)?;

        let changed = page_headers(client.get(format!("{}/changemail?_r=1", BASE_URL)), &cookie)
            .header("Accept", "text/plain,*/*;q=0.8")
            .header("RequestVerificationToken", &verification_token)
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !changed.status().is_success() {
            return Err(format!("tempmailo changemail: {}", changed.status()));
        }
        cookie = merge_set_cookies(&cookie, changed.headers());
        let email = changed
            .text()
            .await
            .map_err(|e| e.to_string())?
            .trim()
            .to_string();
        if !email.contains('@') {
            return Err("tempmailo: invalid email response".into());
        }
        let token = encode_session(&TempmailoSession {
            verification_token,
            cookie,
        })?;
        Ok(EmailInfo {
            channel: Channel::Tempmailo,
            email,
            token: Some(token),
            expires_at: None,
            created_at: None,
        })
    })
}

pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let session = decode_session(token)?;
    let address = email.trim();
    if address.is_empty() {
        return Err("tempmailo: empty email".into());
    }
    block_on(async {
        let resp = page_headers(http_client_no_cookie_jar().post(BASE_URL), &session.cookie)
            .header("Accept", "application/json,*/*;q=0.8")
            .header("Content-Type", "application/json")
            .header("RequestVerificationToken", &session.verification_token)
            .json(&serde_json::json!({ "mail": address }))
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("tempmailo messages: {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let rows = data.as_array().cloned().unwrap_or_default();
        Ok(rows
            .iter()
            .map(|raw| normalize_email(raw, address))
            .collect())
    })
}
