/*!
 * moakt.com：tm_session Cookie + HTML 收件箱；详情页 /{locale}/email/{uuid}/html
 */

use std::collections::BTreeMap;
use std::sync::LazyLock;

use base64::Engine;
use regex::Regex;
use serde::{Deserialize, Serialize};
use serde_json::{json, Value};
use wreq::header::HeaderMap;

use crate::config::{block_on, get_current_ua, http_client_no_cookie_jar};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};

const ORIGIN: &str = "https://www.moakt.com";
const TOK_PREFIX: &str = "mok1:";

#[derive(Serialize, Deserialize)]
struct MoaktSess {
    l: String,
    c: String,
}

static EMAIL_DIV_RE: LazyLock<Regex> = LazyLock::new(|| {
    Regex::new(r#"(?is)<div\s+id="email-address"\s*>([^<]+)</div>"#).expect("re")
});
static HREF_EMAIL_RE: LazyLock<Regex> = LazyLock::new(|| {
    Regex::new(
        r#"href="(/[^"]+/email/[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})""#,
    )
    .expect("re")
});
static TITLE_RE: LazyLock<Regex> = LazyLock::new(|| {
    Regex::new(r#"(?is)<li\s+class="title"\s*>([^<]*)</li>"#).expect("re")
});
static DATE_RE: LazyLock<Regex> = LazyLock::new(|| {
    Regex::new(r#"(?is)<li\s+class="date"[^>]*>[\s\S]*?<span[^>]*>([^<]+)</span>"#).expect("re")
});
static SENDER_RE: LazyLock<Regex> = LazyLock::new(|| {
    Regex::new(r#"(?is)<li\s+class="sender"[^>]*>[\s\S]*?<span[^>]*>([\s\S]*?)</span>\s*</li>"#).expect("re")
});
static BODY_RE: LazyLock<Regex> = LazyLock::new(|| {
    Regex::new(r#"(?is)<div\s+class="email-body"\s*>([\s\S]*?)</div>"#).expect("re")
});
static FROM_ADDR_RE: LazyLock<Regex> = LazyLock::new(|| {
    Regex::new(r#"<([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})>"#).expect("re")
});
static TAG_RE: LazyLock<Regex> = LazyLock::new(|| Regex::new(r#"<[^>]+>"#).expect("re"));

fn locale_from_domain(domain: Option<&str>) -> String {
    let s = domain.unwrap_or("").trim();
    if s.is_empty() {
        return "zh".to_string();
    }
    if s.contains('/') || s.contains('?') || s.contains('#') || s.contains('\\') {
        return "zh".to_string();
    }
    s.to_string()
}

fn light_unescape(s: &str) -> String {
    let t = s.replace("&amp;", "\u{fffd}");
    t.replace("&quot;", "\"")
        .replace("&#34;", "\"")
        .replace("&#39;", "'")
        .replace("&lt;", "<")
        .replace("&gt;", ">")
        .replace('\u{fffd}', "&")
}

fn parse_cookie_header(hdr: &str) -> BTreeMap<String, String> {
    let mut m = BTreeMap::new();
    for part in hdr.split(';') {
        let part = part.trim();
        if part.is_empty() {
            continue;
        }
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

fn cookie_header_from_map(m: &BTreeMap<String, String>) -> String {
    m.iter()
        .map(|(k, v)| format!("{}={}", k, v))
        .collect::<Vec<_>>()
        .join("; ")
}

fn merge_set_cookies(hdr: &str, headers: &HeaderMap) -> String {
    let mut m = parse_cookie_header(hdr);
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
    cookie_header_from_map(&m)
}

fn encode_sess(s: &MoaktSess) -> Result<String, String> {
    let b = serde_json::to_vec(s).map_err(|e| e.to_string())?;
    Ok(format!(
        "{}{}",
        TOK_PREFIX,
        base64::engine::general_purpose::STANDARD.encode(b)
    ))
}

fn decode_sess(tok: &str) -> Result<MoaktSess, String> {
    if !tok.starts_with(TOK_PREFIX) {
        return Err("moakt: invalid session token".into());
    }
    let raw = base64::engine::general_purpose::STANDARD
        .decode(tok.trim_start_matches(TOK_PREFIX))
        .map_err(|_| "moakt: invalid session token".to_string())?;
    let s: MoaktSess =
        serde_json::from_slice(&raw).map_err(|_| "moakt: invalid session token".to_string())?;
    if s.c.is_empty() || s.l.is_empty() {
        return Err("moakt: invalid session token".into());
    }
    Ok(s)
}

fn parse_inbox_address(html: &str) -> Result<String, String> {
    let cap = EMAIL_DIV_RE
        .captures(html)
        .ok_or_else(|| "moakt: email-address not found".to_string())?;
    let addr = light_unescape(cap.get(1).unwrap().as_str().trim());
    if addr.is_empty() {
        return Err("moakt: empty email-address".into());
    }
    Ok(addr)
}

fn list_mail_ids(html: &str) -> Vec<String> {
    let mut seen = std::collections::HashSet::new();
    let mut out = Vec::new();
    for cap in HREF_EMAIL_RE.captures_iter(html) {
        let path = cap.get(1).unwrap().as_str();
        if path.contains("/delete") {
            continue;
        }
        let id = path.rsplit('/').next().unwrap_or("");
        if id.len() == 36 && seen.insert(id.to_string()) {
            out.push(id.to_string());
        }
    }
    out
}

fn strip_tags(s: &str) -> String {
    TAG_RE.replace_all(s, " ").to_string()
}

fn parse_detail_page(page: &str, id: &str, recipient: &str) -> Value {
    let mut from_s = String::new();
    if let Some(cap) = SENDER_RE.captures(page) {
        let inner = light_unescape(cap.get(1).unwrap().as_str());
        from_s = strip_tags(inner.trim());
        if let Some(em) = FROM_ADDR_RE.captures(&inner) {
            from_s = em.get(1).unwrap().as_str().trim().to_string();
        }
    }
    let subject = TITLE_RE
        .captures(page)
        .and_then(|c| c.get(1))
        .map(|m| light_unescape(m.as_str().trim()))
        .unwrap_or_default();
    let date = DATE_RE
        .captures(page)
        .and_then(|c| c.get(1))
        .map(|m| light_unescape(m.as_str().trim()))
        .unwrap_or_default();
    let html_body = BODY_RE
        .captures(page)
        .and_then(|c| c.get(1))
        .map(|m| m.as_str().trim().to_string())
        .unwrap_or_default();

    json!({
        "id": id,
        "to": recipient,
        "from": from_s,
        "subject": subject,
        "date": date,
        "html": html_body,
    })
}

fn page_headers(referer: &str) -> Vec<(&'static str, String)> {
    vec![
        ("User-Agent", get_current_ua().to_string()),
        (
            "Accept",
            "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8"
                .to_string(),
        ),
        (
            "Accept-Language",
            "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6".to_string(),
        ),
        ("Cache-Control", "no-cache".to_string()),
        ("DNT", "1".to_string()),
        ("Pragma", "no-cache".to_string()),
        ("Referer", referer.to_string()),
        ("Upgrade-Insecure-Requests", "1".to_string()),
    ]
}

pub fn generate_email(domain: Option<&str>) -> Result<EmailInfo, String> {
    let loc = locale_from_domain(domain);
    let base = format!("{ORIGIN}/{loc}");
    let inbox = format!("{base}/inbox");
    block_on(async {
        let client = http_client_no_cookie_jar();
        let mut req = client.get(&base);
        for (k, v) in page_headers(&base) {
            req = req.header(k, v);
        }
        let resp = req.send().await.map_err(|e| format!("moakt home: {}", e))?;
        if !resp.status().is_success() {
            return Err(format!("moakt home: {}", resp.status()));
        }
        let headers = resp.headers().clone();
        let mut cookie_hdr = merge_set_cookies("", &headers);
        let _ = resp.text().await;

        let mut req2 = client.get(&inbox);
        for (k, v) in page_headers(&base) {
            req2 = req2.header(k, v);
        }
        req2 = req2.header("Cookie", &cookie_hdr);
        let resp2 = req2
            .send()
            .await
            .map_err(|e| format!("moakt inbox: {}", e))?;
        if !resp2.status().is_success() {
            return Err(format!("moakt inbox: {}", resp2.status()));
        }
        let headers2 = resp2.headers().clone();
        let html = resp2.text().await.map_err(|e| e.to_string())?;
        cookie_hdr = merge_set_cookies(&cookie_hdr, &headers2);

        let email = parse_inbox_address(&html)?;
        if !parse_cookie_header(&cookie_hdr).contains_key("tm_session") {
            return Err("moakt: missing tm_session cookie".into());
        }
        let tok = encode_sess(&MoaktSess {
            l: loc,
            c: cookie_hdr,
        })?;

        Ok(EmailInfo {
            channel: Channel::Moakt,
            email,
            token: Some(tok),
            expires_at: None,
            created_at: None,
        })
    })
}

pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let sess = decode_sess(token)?;
    let loc = &sess.l;
    let inbox = format!("{ORIGIN}/{loc}/inbox");
    let cookie_hdr = &sess.c;

    block_on(async {
        let client = http_client_no_cookie_jar();
        let base_ref = format!("{ORIGIN}/{loc}");
        let mut req = client.get(&inbox);
        for (k, v) in page_headers(&base_ref) {
            req = req.header(k, v);
        }
        req = req.header("Cookie", cookie_hdr);
        let resp = req
            .send()
            .await
            .map_err(|e| format!("moakt inbox: {}", e))?;
        if !resp.status().is_success() {
            return Err(format!("moakt inbox: {}", resp.status()));
        }
        let html = resp.text().await.map_err(|e| e.to_string())?;
        let ids = list_mail_ids(&html);
        let mut out = Vec::with_capacity(ids.len());
        for id in ids {
            let detail = format!("{ORIGIN}/{loc}/email/{id}/html");
            let refer = format!("{ORIGIN}/{loc}/email/{id}");
            let mut reqd = client.get(&detail);
            for (k, v) in page_headers(&refer) {
                reqd = reqd.header(k, v);
            }
            reqd = reqd.header("Cookie", cookie_hdr);
            let respd = match reqd.send().await {
                Ok(r) => r,
                Err(_) => continue,
            };
            if !respd.status().is_success() {
                continue;
            }
            let page = match respd.text().await {
                Ok(t) => t,
                Err(_) => continue,
            };
            let raw = parse_detail_page(&page, &id, email);
            out.push(normalize_email(&raw, email));
        }
        Ok(out)
    })
}
