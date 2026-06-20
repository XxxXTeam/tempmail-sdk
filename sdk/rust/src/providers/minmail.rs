/*!
 * MinMail — https://minmail.app/api
 * 建箱与列表均需客户端 visitor-id 请求头绑定会话。
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use rand::Rng;
use serde::{Deserialize, Serialize};
use serde_json::Value;
use std::time::{SystemTime, UNIX_EPOCH};

const BASE: &str = "https://minmail.app/api";

#[derive(Debug, Serialize, Deserialize, Default)]
struct MinmailToken {
    #[serde(default, rename = "visitorId")]
    visitor_id: String,
    #[serde(default)]
    ck: String,
    #[serde(default)]
    cookie: String,
}

fn random_seg(n: usize) -> String {
    let chars: Vec<char> = "abcdefghijklmnopqrstuvwxyz0123456789".chars().collect();
    let mut rng = rand::thread_rng();
    (0..n)
        .map(|_| chars[rng.gen_range(0..chars.len())])
        .collect()
}

fn visitor_id() -> String {
    format!(
        "{}-{}-{}-{}-{}",
        random_seg(8),
        random_seg(4),
        random_seg(4),
        random_seg(4),
        random_seg(12)
    )
}

fn merge_cookie_line(prev: &str, set_cookie: &str) -> String {
    let mut m: std::collections::BTreeMap<String, String> = std::collections::BTreeMap::new();
    for chunk in prev.split(';').chain(set_cookie.split(';')) {
        let t = chunk.trim();
        if t.is_empty() {
            continue;
        }
        if let Some((k, v)) = t.split_once('=') {
            m.insert(k.trim().to_string(), v.trim().to_string());
        }
    }
    m.iter()
        .map(|(k, v)| format!("{k}={v}"))
        .collect::<Vec<_>>()
        .join("; ")
}

fn cookie_get(line: &str, name: &str) -> String {
    for part in line.split(';') {
        let t = part.trim();
        if let Some((k, v)) = t.split_once('=') {
            if k.trim() == name {
                return v.trim().to_string();
            }
        }
    }
    String::new()
}

fn set_cookie_from_response(headers: &wreq::header::HeaderMap) -> String {
    let mut parts = Vec::new();
    for (name, value) in headers.iter() {
        if name.as_str().eq_ignore_ascii_case("set-cookie") {
            if let Ok(s) = value.to_str() {
                let nv = s.split(';').next().unwrap_or("").trim();
                if !nv.is_empty() {
                    parts.push(nv.to_string());
                }
            }
        }
    }
    parts.join("; ")
}

fn cookie_header() -> String {
    let now = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_millis() as i64)
        .unwrap_or(0);
    let rnd: i32 = rand::thread_rng().gen_range(0..1_000_000);
    let ga = format!("GA1.1.{}.{}", now, rnd);
    format!("_ga={ga}; _ga_DFGB8WF1WG=GS2.1.s{now}$o1$g0$t{now}$j60$l0$h0")
}

fn base_headers(b: wreq::RequestBuilder, cookie_line: &str) -> wreq::RequestBuilder {
    let cookie = if cookie_line.trim().is_empty() {
        cookie_header()
    } else {
        cookie_line.to_string()
    };
    b.header("Accept", "*/*")
        .header(
            "Accept-Language",
            "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
        )
        .header("Origin", "https://minmail.app")
        .header("Referer", "https://minmail.app/")
        .header("User-Agent", get_current_ua())
        .header("cache-control", "no-cache")
        .header("dnt", "1")
        .header("pragma", "no-cache")
        .header("priority", "u=1, i")
        .header(
            "sec-ch-ua",
            r#""Chromium";v="148", "Microsoft Edge";v="148", "Not/A)Brand";v="99""#,
        )
        .header("sec-ch-ua-mobile", "?0")
        .header("sec-ch-ua-platform", r#""Windows""#)
        .header("sec-fetch-dest", "empty")
        .header("sec-fetch-mode", "cors")
        .header("sec-fetch-site", "same-origin")
        .header("Cookie", cookie)
}

fn build_address_req(
    client: &wreq::Client,
    path: &str,
    vid: &str,
    cookie_line: &str,
) -> wreq::RequestBuilder {
    let url = format!("{}{}", BASE, path);
    base_headers(client.get(&url), cookie_line).header("visitor-id", vid)
}

fn build_list_req(
    client: &wreq::Client,
    path: &str,
    vid: &str,
    ck: &str,
    cookie_line: &str,
) -> wreq::RequestBuilder {
    let url = format!("{}{}", BASE, path);
    let mut b = base_headers(client.get(&url), cookie_line).header("visitor-id", vid);
    if !ck.is_empty() {
        b = b.header("ck", ck);
    }
    b
}

fn parse_minmail_token(s: &str) -> (String, String, String) {
    let t = s.trim();
    if t.starts_with('{') {
        if let Ok(tok) = serde_json::from_str::<MinmailToken>(t) {
            let mut vid = tok.visitor_id;
            if vid.is_empty() {
                vid = cookie_get(&tok.cookie, "visitorId");
            }
            let mut ck = tok.ck;
            if ck.is_empty() {
                ck = cookie_get(&tok.cookie, "ck");
            }
            return (vid, ck, tok.cookie);
        }
    }
    (t.to_string(), String::new(), String::new())
}

fn encode_minmail_token(
    visitor_id: &str,
    ck: &str,
    cookie_line: &str,
) -> Result<String, serde_json::Error> {
    serde_json::to_string(&MinmailToken {
        visitor_id: visitor_id.to_string(),
        ck: ck.to_string(),
        cookie: cookie_line.to_string(),
    })
}

fn minmail_to_matches(header: &str, want: &str) -> bool {
    let w = want.trim().to_lowercase();
    if w.is_empty() {
        return true;
    }
    let h = header.trim().to_lowercase();
    if h.is_empty() {
        return true;
    }
    if h == w {
        return true;
    }
    if let (Some(i), Some(j)) = (h.find('<'), h.find('>')) {
        if j > i {
            let inner = h[i + 1..j].trim();
            if inner == w {
                return true;
            }
        }
    }
    h.contains(&w)
}

pub fn generate_email() -> Result<EmailInfo, String> {
    let vid = visitor_id();
    let cook = cookie_header();
    block_on(async {
        let resp = build_address_req(
            &http_client(),
            "/mail/address?refresh=true&expire=1440&part=main",
            &vid,
            &cook,
        )
        .send()
        .await
        .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("minmail address {}", resp.status()));
        }
        let headers = resp.headers().clone();
        let set_ck = set_cookie_from_response(&headers);
        let mut cook_merged = merge_cookie_line(&cook, &set_ck);
        let ck_hdr = headers
            .get("ck")
            .and_then(|v| v.to_str().ok())
            .unwrap_or("")
            .to_string();
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let address = data["address"].as_str().unwrap_or("");
        if address.is_empty() {
            return Err("minmail: empty address".into());
        }
        let mut bound_vid = vid.clone();
        if let Some(s) = data["visitorId"].as_str().filter(|s| !s.is_empty()) {
            bound_vid = s.to_string();
        } else if let Some(s) = data["visitor_id"].as_str().filter(|s| !s.is_empty()) {
            bound_vid = s.to_string();
        } else {
            let from_cookie = cookie_get(&cook_merged, "visitorId");
            if !from_cookie.is_empty() {
                bound_vid = from_cookie;
            }
        }
        let ck_owned = data["ck"]
            .as_str()
            .filter(|s| !s.is_empty())
            .map(|s| s.to_string())
            .or_else(|| {
                if !ck_hdr.is_empty() {
                    Some(ck_hdr)
                } else {
                    let c = cookie_get(&cook_merged, "ck");
                    if c.is_empty() {
                        None
                    } else {
                        Some(c)
                    }
                }
            })
            .unwrap_or_default();
        if !bound_vid.is_empty() && cookie_get(&cook_merged, "visitorId").is_empty() {
            cook_merged = merge_cookie_line(&cook_merged, &format!("visitorId={bound_vid}"));
        }
        let token =
            encode_minmail_token(&bound_vid, &ck_owned, &cook_merged).map_err(|e| e.to_string())?;
        let expire_min = data["expire"].as_i64().unwrap_or(0);
        let expires_at = if expire_min > 0 {
            let now = SystemTime::now()
                .duration_since(UNIX_EPOCH)
                .map(|d| d.as_millis() as i64)
                .unwrap_or(0);
            Some(now + expire_min * 60 * 1000)
        } else {
            None
        };
        Ok(EmailInfo {
            channel: Channel::Minmail,
            email: address.to_string(),
            token: Some(token),
            expires_at,
            created_at: None,
        })
    })
}

pub fn get_emails(email: &str, token: Option<&str>) -> Result<Vec<Email>, String> {
    let (vid, ck, cook) = token
        .map(|s| parse_minmail_token(s))
        .unwrap_or_else(|| (String::new(), String::new(), String::new()));
    if vid.is_empty() {
        return Err("minmail: token must include visitorId".into());
    }
    let email = email.to_string();
    block_on(async {
        let resp = build_list_req(&http_client(), "/mail/list?part=main", &vid, &ck, &cook)
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("minmail list {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let messages = data["message"].as_array().cloned().unwrap_or_default();
        let mut out = Vec::new();
        for raw in messages {
            let to = raw["to"].as_str().unwrap_or("");
            if !to.is_empty() && !minmail_to_matches(to, email.as_str()) {
                continue;
            }
            let to_s = raw["to"].as_str().unwrap_or(email.as_str());
            let flat = serde_json::json!({
                "id": raw["id"],
                "from": raw["from"],
                "fromAddress": raw["fromAddress"],
                "fromName": raw["fromName"],
                "to": to_s,
                "subject": raw["subject"],
                "text": raw["preview"],
                "html": raw["content"],
                "date": raw["date"],
                "isRead": raw["isRead"],
            });
            out.push(normalize_email(&flat, &email));
        }
        Ok(out)
    })
}
