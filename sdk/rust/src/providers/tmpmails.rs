/*!
 * tmpmails.com：GET 首页下发 user_sign；收信为 Next.js Server Action（text/x-component）
 */

use std::collections::HashMap;
use std::sync::LazyLock;

use regex::Regex;
use serde_json::Value;
use wreq::header::HeaderMap;

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};

const ORIGIN: &str = "https://tmpmails.com";
const TOKEN_SEP: char = '\t';

static EMAIL_RE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r"[a-zA-Z0-9._-]+@tmpmails\.com").expect("email re"));
static PAGE_CHUNK_RE: LazyLock<Regex> = LazyLock::new(|| {
    Regex::new(r#"/_next/static/chunks/app/%5Blocale%5D/page-[a-f0-9]+\.js"#).expect("chunk re")
});
static INBOX_ACTION_RE: LazyLock<Regex> = LazyLock::new(|| {
    Regex::new(r#""([0-9a-f]+)",[^,]+,void 0,[^,]+,"getInboxList""#).expect("action re")
});

fn locale_from_domain(domain: Option<&str>) -> String {
    let s = domain.unwrap_or("").trim();
    if s.is_empty() {
        "zh".to_string()
    } else {
        s.to_string()
    }
}

fn user_sign_from_headers(headers: &HeaderMap) -> Result<String, String> {
    for v in headers.get_all("set-cookie") {
        let s: &str = v.to_str().map_err(|_| "tmpmails: bad set-cookie")?;
        let lower = s.to_ascii_lowercase();
        let rest = if let Some(i) = lower.find("user_sign=") {
            &s[i + "user_sign=".len()..]
        } else {
            continue;
        };
        let val = rest.split(';').next().unwrap_or("").trim();
        if !val.is_empty() {
            return Ok(val.to_string());
        }
    }
    Err("tmpmails: missing user_sign cookie".into())
}

fn pick_email(html: &str) -> Result<String, String> {
    const SUPPORT: &str = "support@tmpmails.com";
    let mut counts: HashMap<String, usize> = HashMap::new();
    for cap in EMAIL_RE.captures_iter(html) {
        let addr = cap.get(0).unwrap().as_str();
        if addr.eq_ignore_ascii_case(SUPPORT) {
            continue;
        }
        *counts.entry(addr.to_string()).or_insert(0) += 1;
    }
    if counts.is_empty() {
        return Err("tmpmails: no inbox address in page".into());
    }
    counts
        .into_iter()
        .max_by_key(|(addr, c)| (std::cmp::Reverse(*c), std::cmp::Reverse(addr.clone())))
        .map(|(k, _)| k)
        .ok_or_else(|| "tmpmails: no inbox address in page".into())
}

async fn fetch_inbox_action_id(html: &str) -> Result<String, String> {
    let m = PAGE_CHUNK_RE
        .find(html)
        .ok_or_else(|| "tmpmails: page chunk script not found".to_string())?;
    let url = format!("{}{}", ORIGIN, m.as_str());
    let resp = http_client()
        .get(&url)
        .header("User-Agent", get_current_ua())
        .header("Accept", "*/*")
        .header("Referer", format!("{ORIGIN}/"))
        .send()
        .await
        .map_err(|e| format!("tmpmails chunk: {}", e))?;
    if !resp.status().is_success() {
        return Err(format!("tmpmails chunk: {}", resp.status()));
    }
    let js = resp.text().await.map_err(|e| format!("tmpmails chunk body: {}", e))?;
    let cap = INBOX_ACTION_RE
        .captures(&js)
        .ok_or_else(|| "tmpmails: getInboxList action not found".to_string())?;
    Ok(cap.get(1).unwrap().as_str().to_string())
}

pub fn generate_email(domain: Option<&str>) -> Result<EmailInfo, String> {
    let loc = locale_from_domain(domain);
    let page_url = format!("{ORIGIN}/{loc}");
    block_on(async {
        let resp = http_client()
            .get(&page_url)
            .header("User-Agent", get_current_ua())
            .header(
                "Accept",
                "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
            )
            .header(
                "Accept-Language",
                "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
            )
            .header("Cache-Control", "no-cache")
            .header("DNT", "1")
            .header("Pragma", "no-cache")
            .header("Referer", &page_url)
            .header("Upgrade-Insecure-Requests", "1")
            .send()
            .await
            .map_err(|e| format!("tmpmails generate: {}", e))?;
        if !resp.status().is_success() {
            return Err(format!("tmpmails generate: {}", resp.status()));
        }
        let headers = resp.headers().clone();
        let html = resp
            .text()
            .await
            .map_err(|e| format!("tmpmails generate body: {}", e))?;
        let user_sign = user_sign_from_headers(&headers)?;
        let email = pick_email(&html)?;
        let action_id = fetch_inbox_action_id(&html).await?;
        let token = format!("{loc}{TOKEN_SEP}{user_sign}{TOKEN_SEP}{action_id}");
        Ok(EmailInfo {
            channel: Channel::Tmpmails,
            email,
            token: Some(token),
            expires_at: None,
            created_at: None,
        })
    })
}

fn parse_inbox_response(body: &str, recipient: &str) -> Result<Vec<Email>, String> {
    let text = body.replace("\"$undefined\"", "null");
    let mut last_data_err: Option<String> = None;
    for line in text.lines() {
        let line = line.trim();
        if line.is_empty() {
            continue;
        }
        let Some(colon) = line.find(':') else {
            continue;
        };
        let json_part = line[colon + 1..].trim();
        if !json_part.starts_with('{') {
            continue;
        }
        let v: Value = match serde_json::from_str(json_part) {
            Ok(x) => x,
            Err(_) => continue,
        };
        if v.get("code").and_then(|c| c.as_u64()) != Some(200) {
            continue;
        }
        let data = match v.get("data") {
            Some(d) => d,
            None => continue,
        };
        let list = match data.get("list").and_then(|l| l.as_array()) {
            Some(l) => l,
            None => {
                last_data_err = Some("tmpmails: invalid list".into());
                continue;
            }
        };
        let mut out = Vec::with_capacity(list.len());
        for item in list {
            out.push(normalize_email(item, recipient));
        }
        return Ok(out);
    }
    if let Some(e) = last_data_err {
        return Err(e);
    }
    Err("tmpmails: inbox payload not found".into())
}

pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let mut parts = token.split(TOKEN_SEP);
    let loc = parts.next().ok_or("tmpmails: invalid token")?;
    let user_sign = parts.next().ok_or("tmpmails: invalid token")?;
    let action_id = parts.next().ok_or("tmpmails: invalid token")?;
    if parts.next().is_some() {
        return Err("tmpmails: invalid token".into());
    }
    let post_url = format!("{ORIGIN}/{loc}");
    let body = serde_json::to_string(&serde_json::json!([user_sign, email, 0]))
        .map_err(|e| e.to_string())?;
    block_on(async {
        let resp = http_client()
            .post(&post_url)
            .header("User-Agent", get_current_ua())
            .header("Accept", "text/x-component")
            .header("Content-Type", "text/plain;charset=UTF-8")
            .header("Next-Action", action_id)
            .header("Origin", ORIGIN)
            .header("Referer", &post_url)
            .header(
                "Cookie",
                format!("NEXT_LOCALE={loc}; user_sign={user_sign}"),
            )
            .body(body)
            .send()
            .await
            .map_err(|e| format!("tmpmails inbox: {}", e))?;
        if !resp.status().is_success() {
            return Err(format!("tmpmails inbox: {}", resp.status()));
        }
        let text = resp.text().await.map_err(|e| e.to_string())?;
        parse_inbox_response(&text, email)
    })
}
