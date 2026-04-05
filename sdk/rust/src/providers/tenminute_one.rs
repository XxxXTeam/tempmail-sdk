/*!
 * 10minutemail.one：SSR __NUXT_DATA__ 中的 mailServiceToken + 页面 emailDomains；
 * 收信 GET web.10minutemail.one/api/v1/mailbox/{email}
 */

use std::collections::HashMap;

use base64::{engine::general_purpose::URL_SAFE_NO_PAD, Engine as _};
use chrono::Utc;
use hex::encode as hex_encode;
use rand::{Rng, RngCore};
use regex::Regex;
use serde_json::Value;
use std::sync::LazyLock;

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};

const SITE_ORIGIN: &str = "https://10minutemail.one";
const API_BASE: &str = "https://web.10minutemail.one/api/v1";

static NUXT_DATA_RE: LazyLock<Regex> = LazyLock::new(|| {
    Regex::new(r#"(?is)<script[^>]*\bid="__NUXT_DATA__"[^>]*>([\s\S]*?)</script>"#)
        .expect("nuxt re")
});

static JWT_RE: LazyLock<Regex> = LazyLock::new(|| {
    Regex::new(r"^[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+$").expect("jwt re")
});

fn enc_mailbox_email(email: &str) -> String {
    if let Some((l, d)) = email.split_once('@') {
        format!(
            "{}%40{}",
            urlencoding::encode(l),
            urlencoding::encode(d)
        )
    } else {
        urlencoding::encode(email).into_owned()
    }
}

fn parse_nuxt_array(html: &str) -> Result<Vec<Value>, String> {
    let cap = NUXT_DATA_RE
        .captures(html)
        .ok_or_else(|| "10minute-one: __NUXT_DATA__ not found".to_string())?;
    let raw = cap.get(1).map(|m| m.as_str().trim()).unwrap_or("");
    serde_json::from_str(raw).map_err(|e| format!("10minute-one: nuxt json: {e}"))
}

fn resolve_ref(arr: &[Value], v: &Value, depth: usize) -> Option<Value> {
    if depth > 64 {
        return None;
    }
    let idx = if let Some(n) = v.as_u64() {
        n as usize
    } else if let Some(n) = v.as_i64() {
        if n < 0 {
            return Some(v.clone());
        }
        n as usize
    } else {
        return Some(v.clone());
    };
    let next = arr.get(idx)?;
    resolve_ref(arr, next, depth + 1).or_else(|| Some(next.clone()))
}

fn parse_mail_service_token(arr: &[Value]) -> Result<String, String> {
    let n = arr.len().min(48);
    for i in 0..n {
        if let Some(obj) = arr[i].as_object() {
            if let Some(refv) = obj.get("mailServiceToken") {
                if let Some(t) = resolve_ref(arr, refv, 0) {
                    if let Some(s) = t.as_str() {
                        if JWT_RE.is_match(s) {
                            return Ok(s.to_string());
                        }
                    }
                }
            }
        }
    }
    for el in arr {
        if let Some(obj) = el.as_object() {
            if let Some(refv) = obj.get("mailServiceToken") {
                if let Some(t) = resolve_ref(arr, refv, 0) {
                    if let Some(s) = t.as_str() {
                        if JWT_RE.is_match(s) {
                            return Ok(s.to_string());
                        }
                    }
                }
            }
        }
    }
    for el in arr {
        if let Some(s) = el.as_str() {
            if JWT_RE.is_match(s) {
                return Ok(s.to_string());
            }
        }
    }
    Err("10minute-one: mailServiceToken not found".into())
}

fn parse_quoted_json_array(html: &str, field: &str) -> Vec<String> {
    let key = format!("{field}:\"");
    let Some(start) = html.find(&key) else {
        return vec![];
    };
    let slice_start = start + key.len();
    let b = html.as_bytes();
    if b.get(slice_start) != Some(&b'[') {
        return vec![];
    }
    let mut depth = 0u32;
    let mut j = slice_start;
    while j < b.len() {
        match b[j] {
            b'[' => depth += 1,
            b']' => {
                depth -= 1;
                if depth == 0 {
                    j += 1;
                    break;
                }
            }
            _ => {}
        }
        j += 1;
    }
    let Some(raw) = html.get(slice_start..j) else {
        return vec![];
    };
    let unesc = raw.replace(r#"\""#, "\"");
    serde_json::from_str::<Vec<String>>(&unesc).unwrap_or_default()
}

fn pick_locale(domain: Option<&str>) -> String {
    let s = domain.unwrap_or("").trim();
    if s.is_empty() {
        return "zh".into();
    }
    if s.contains('.') && !s.contains('/') {
        "zh".into()
    } else {
        s.to_string()
    }
}

fn pick_mailbox_domain(hint: Option<&str>, available: &[String]) -> Result<String, String> {
    if available.is_empty() {
        return Err("10minute-one: no email domains".into());
    }
    if let Some(h) = hint {
        let h = h.trim().to_lowercase();
        if h.contains('.') {
            for d in available {
                if d.to_lowercase() == h {
                    return Ok(d.clone());
                }
            }
        }
    }
    let i = rand::thread_rng().gen_range(0..available.len());
    Ok(available[i].clone())
}

fn random_local(len: usize) -> String {
    const CHARS: &[u8] = b"abcdefghijklmnopqrstuvwxyz0123456789";
    let mut rng = rand::thread_rng();
    (0..len)
        .map(|_| CHARS[rng.gen_range(0..CHARS.len())] as char)
        .collect()
}

fn jwt_exp_unix(token: &str) -> Option<i64> {
    let mid = token.split('.').nth(1)?;
    let bytes = URL_SAFE_NO_PAD.decode(mid).ok()?;
    let v: Value = serde_json::from_slice(&bytes).ok()?;
    v.get("exp")?.as_i64().or_else(|| v.get("exp")?.as_f64().map(|x| x as i64))
}

fn api_headers(token: &str) -> Vec<(&'static str, String)> {
    let mut rid = [0u8; 16];
    rand::thread_rng().fill_bytes(&mut rid);
    let ts = Utc::now().timestamp().to_string();
    vec![
        ("Accept", "*/*".into()),
        ("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8".into()),
        ("Authorization", format!("Bearer {token}")),
        ("Cache-Control", "no-cache".into()),
        ("Content-Type", "application/json".into()),
        ("DNT", "1".into()),
        ("Origin", SITE_ORIGIN.into()),
        ("Pragma", "no-cache".into()),
        ("Referer", format!("{SITE_ORIGIN}/")),
        ("User-Agent", get_current_ua().to_string()),
        ("X-Request-ID", hex_encode(rid)),
        ("X-Timestamp", ts),
    ]
}

fn item_needs_detail(m: &serde_json::Map<String, Value>) -> bool {
    let id = m.get("id").map(|v| v.to_string()).unwrap_or_default();
    if id.trim().is_empty() || id == "null" {
        return false;
    }
    let subj = m
        .get("subject")
        .or_else(|| m.get("mail_title"))
        .and_then(|v| v.as_str())
        .unwrap_or("")
        .trim();
    let body = ["text", "body", "html", "mail_text"]
        .iter()
        .find_map(|k| m.get(*k).and_then(|v| v.as_str()))
        .unwrap_or("")
        .trim();
    subj.is_empty() && body.is_empty()
}

pub fn generate_email(domain: Option<&str>) -> Result<EmailInfo, String> {
    block_on(async {
        let loc = pick_locale(domain);
        let page_url = format!(
            "{}/{}",
            SITE_ORIGIN,
            urlencoding::encode(&loc)
        );
        let resp = http_client()
            .get(&page_url)
            .header("User-Agent", get_current_ua())
            .header(
                "Accept",
                "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
            )
            .header("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
            .header("Cache-Control", "no-cache")
            .header("Pragma", "no-cache")
            .header("DNT", "1")
            .header("Referer", &page_url)
            .send()
            .await
            .map_err(|e| format!("10minute-one page: {e}"))?;
        if !resp.status().is_success() {
            return Err(format!("10minute-one page: {}", resp.status()));
        }
        let html = resp.text().await.map_err(|e| e.to_string())?;

        let arr = parse_nuxt_array(&html)?;
        let token = parse_mail_service_token(&arr)?;

        let mut domains = parse_quoted_json_array(&html, "emailDomains");
        if domains.is_empty() {
            domains = vec![
                "xghff.com".into(),
                "oqqaj.com".into(),
                "psovv.com".into(),
            ];
        }

        let mut blocked: HashMap<String, ()> = HashMap::new();
        for u in parse_quoted_json_array(&html, "blockedUsernames") {
            blocked.insert(u.to_lowercase(), ());
        }

        let dom_hint = domain
            .map(str::trim)
            .filter(|s| s.contains('.'))
            .map(str::to_string);
        let mail_domain = pick_mailbox_domain(dom_hint.as_deref(), &domains)?;

        let mut local = String::new();
        for _ in 0..32 {
            let cand = random_local(10);
            if !blocked.contains_key(&cand.to_lowercase()) {
                local = cand;
                break;
            }
        }
        if local.is_empty() {
            return Err("10minute-one: could not pick username".into());
        }

        let email = format!("{local}@{mail_domain}");
        let exp_ms = jwt_exp_unix(&token).map(|x| x * 1000);
        Ok(EmailInfo {
            channel: Channel::TenminuteOne,
            email,
            token: Some(token),
            expires_at: exp_ms,
            created_at: None,
        })
    })
}

pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    block_on(async {
        let client = http_client();
        let url = format!("{API_BASE}/mailbox/{}", enc_mailbox_email(email));
        let mut req = client.get(&url);
        for (k, v) in api_headers(token) {
            req = req.header(k, v);
        }
        let resp = req
            .send()
            .await
            .map_err(|e| format!("10minute-one inbox: {e}"))?;
        if !resp.status().is_success() {
            return Err(format!("10minute-one inbox: {}", resp.status()));
        }
        let list: Vec<Value> = resp.json().await.map_err(|e| e.to_string())?;

        let mut out = Vec::with_capacity(list.len());
        for item in list {
            let Some(mut obj) = item.as_object().cloned() else {
                continue;
            };
            if item_needs_detail(&obj) {
                let id = obj.get("id").cloned().unwrap_or(Value::Null);
                let id_s = serde_json::to_string(&id).unwrap_or_default();
                let id_s = id_s.trim_matches('"');
                let mid = urlencoding::encode(id_s);
                let du = format!(
                    "{API_BASE}/mailbox/{}/{}",
                    enc_mailbox_email(email),
                    mid
                );
                let mut dreq = client.get(&du);
                for (k, v) in api_headers(token) {
                    dreq = dreq.header(k, v);
                }
                if let Ok(r2) = dreq.send().await {
                    if r2.status().is_success() {
                        if let Ok(detail) = r2.json::<Value>().await {
                            if let Some(dm) = detail.as_object() {
                                for (k, v) in dm {
                                    obj.entry(k.clone()).or_insert_with(|| v.clone());
                                }
                            }
                        }
                    }
                }
            }
            out.push(normalize_email(&Value::Object(obj), email));
        }
        Ok(out)
    })
}
