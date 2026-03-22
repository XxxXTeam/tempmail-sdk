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
    if !out.is_empty() {
        return out;
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
        Ok(vals.iter().map(|v| normalize_email(v, email)).collect())
    })
}
