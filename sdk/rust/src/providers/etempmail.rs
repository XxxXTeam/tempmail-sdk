/*!
 * eTempMail — https://etempmail.com
 * GET /zh 建立 ci_session；POST /getEmailAddress、/getInbox。
 */

use std::collections::BTreeMap;

use chrono::{TimeZone, Utc};
use serde_json::{json, Value};
use wreq::header::HeaderMap;

use crate::config::{block_on, http_client_no_cookie_jar};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};

const BASE: &str = "https://etempmail.com";

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

fn common_headers() -> Vec<(&'static str, String)> {
    vec![
        ("Accept", "*/*".into()),
        (
            "Accept-Language",
            "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6".into(),
        ),
        ("Cache-Control", "no-cache".into()),
        ("DNT", "1".into()),
        ("Origin", BASE.into()),
        ("Pragma", "no-cache".into()),
        ("Referer", format!("{}/zh", BASE)),
        (
            "sec-ch-ua",
            r#""Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146""#.into(),
        ),
        ("sec-ch-ua-mobile", "?0".into()),
        ("sec-ch-ua-platform", r#""Windows""#.into()),
        ("sec-fetch-dest", "empty".into()),
        ("sec-fetch-mode", "cors".into()),
        ("sec-fetch-site", "same-origin".into()),
        (
            "User-Agent",
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0".into(),
        ),
        ("X-Requested-With", "XMLHttpRequest".into()),
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
    let hdrs = common_headers();
    let (addr, created_at, cookie) = block_on(async {
        let mut req = client.get(format!("{}/zh", BASE));
        req = apply_headers(req, &hdrs);
        let resp = req.send().await.map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("etempmail session {}", resp.status()));
        }
        let headers = resp.headers().clone();
        resp.bytes().await.map_err(|e| e.to_string())?;
        let mut cookie_hdr = merge_set_cookies("", &headers);
        if !cookie_hdr.contains("ci_session=") {
            return Err("etempmail: no ci_session cookie".into());
        }

        let mut req2 = client.post(format!("{}/getEmailAddress", BASE));
        req2 = apply_headers(req2, &hdrs);
        req2 = req2.header("Cookie", &cookie_hdr);
        let resp2 = req2.send().await.map_err(|e| e.to_string())?;
        if !resp2.status().is_success() {
            return Err(format!("etempmail getEmailAddress {}", resp2.status()));
        }
        let headers2 = resp2.headers().clone();
        cookie_hdr = merge_set_cookies(&cookie_hdr, &headers2);
        let data: Value = resp2.json().await.map_err(|e| e.to_string())?;
        let address = data
            .get("address")
            .and_then(|x| x.as_str())
            .unwrap_or("")
            .to_string();
        if address.is_empty() {
            return Err("etempmail: no address".into());
        }
        let created_iso = data
            .get("creation_time")
            .and_then(|x| x.as_str())
            .and_then(|s| s.parse::<i64>().ok())
            .filter(|&x| x > 0)
            .and_then(|sec| Utc.timestamp_opt(sec, 0).single())
            .map(|t| t.to_rfc3339());
        Ok((address, created_iso, cookie_hdr))
    })?;
    Ok(EmailInfo {
        channel: Channel::Etempmail,
        email: addr,
        token: Some(cookie),
        expires_at: None,
        created_at,
    })
}

pub fn get_emails(cookie: &str, email: &str) -> Result<Vec<Email>, String> {
    if cookie.is_empty() {
        return Err("token is required for etempmail".into());
    }
    let client = http_client_no_cookie_jar();
    let hdrs = common_headers();
    block_on(async {
        let mut req = client.post(format!("{}/getInbox", BASE));
        req = apply_headers(req, &hdrs);
        req = req.header("Cookie", cookie);
        let resp = req.send().await.map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("etempmail getInbox {}", resp.status()));
        }
        let rows: Vec<Value> = resp.json().await.map_err(|e| e.to_string())?;
        let mut out = Vec::new();
        for (i, raw) in rows.into_iter().enumerate() {
            let from = raw["from"].as_str().unwrap_or("").to_string();
            let subj = raw["subject"].as_str().unwrap_or("").to_string();
            let dt = raw["date"].as_str().unwrap_or("").to_string();
            let body = raw["body"].as_str().map(|s| s.to_string()).unwrap_or_default();
            let flat = json!({
                "id": format!("{}\n{}\n{}\n{}\n{}", from, subj, dt, i, email),
                "from": from,
                "subject": subj,
                "body": body,
                "date": dt,
                "isRead": false,
            });
            out.push(normalize_email(&flat, email));
        }
        Ok(out)
    })
}
