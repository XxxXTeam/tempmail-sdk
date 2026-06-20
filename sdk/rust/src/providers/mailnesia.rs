/*!
 * Mailnesia — https://mailnesia.com
 */

use crate::config::{block_on, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use rand::Rng;
use regex::Regex;
use serde_json::{json, Value};

const BASE_URL: &str = "https://mailnesia.com";
const DOMAIN: &str = "mailnesia.com";

fn random_local() -> String {
    const CHARS: &[u8] = b"abcdefghijklmnopqrstuvwxyz0123456789";
    let mut rng = rand::thread_rng();
    let mut out = String::from("sdk");
    for _ in 0..16 {
        out.push(CHARS[rng.gen_range(0..CHARS.len())] as char);
    }
    out
}

fn local_part(email: &str) -> String {
    email
        .trim()
        .split('@')
        .next()
        .unwrap_or("")
        .trim()
        .to_string()
}

fn mailbox_url(local: &str) -> String {
    format!("{}/mailbox/{}", BASE_URL, urlencoding::encode(local))
}

fn detail_url(local: &str, message_id: &str) -> String {
    format!("{}/{}", mailbox_url(local), urlencoding::encode(message_id))
}

fn fetch_html(url: String) -> Result<String, String> {
    block_on(async {
        let resp = http_client()
            .get(url)
            .header("Accept", "text/html,*/*")
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("mailnesia http {}", resp.status()));
        }
        resp.text().await.map_err(|e| e.to_string())
    })
}

fn light_unescape(s: &str) -> String {
    let mut out = s
        .replace("&nbsp;", " ")
        .replace("&amp;", "&")
        .replace("&lt;", "<")
        .replace("&gt;", ">")
        .replace("&quot;", "\"")
        .replace("&#39;", "'");
    if let Ok(hex_re) = Regex::new(r"&#x([0-9a-fA-F]+);") {
        out = hex_re
            .replace_all(&out, |caps: &regex::Captures<'_>| {
                u32::from_str_radix(&caps[1], 16)
                    .ok()
                    .and_then(char::from_u32)
                    .map(|c| c.to_string())
                    .unwrap_or_default()
            })
            .to_string();
    }
    if let Ok(dec_re) = Regex::new(r"&#(\d+);") {
        out = dec_re
            .replace_all(&out, |caps: &regex::Captures<'_>| {
                caps[1]
                    .parse::<u32>()
                    .ok()
                    .and_then(char::from_u32)
                    .map(|c| c.to_string())
                    .unwrap_or_default()
            })
            .to_string();
    }
    out
}

fn clean_text(raw: &str) -> String {
    let stripped = Regex::new(r"(?is)<[^>]+>")
        .map(|re| re.replace_all(raw, " ").to_string())
        .unwrap_or_else(|_| raw.to_string());
    light_unescape(&stripped)
        .split_whitespace()
        .collect::<Vec<_>>()
        .join(" ")
}

fn parse_rows(page: &str) -> Vec<Value> {
    let row_re = Regex::new(
        r#"(?is)<tr\s+id="([^"]+)"[^>]*class="[^"]*\bemailheader\b[^"]*"[^>]*>(.*?)</tr>"#,
    )
    .expect("row regex");
    let time_re = Regex::new(r#"(?is)<time\s+datetime="([^"]+)""#).expect("time regex");
    let anchor_re =
        Regex::new(r#"(?is)<a\b[^>]*class="email"[^>]*>(.*?)</a>"#).expect("anchor regex");
    let mut rows = Vec::new();
    for caps in row_re.captures_iter(page) {
        let id = caps.get(1).map(|m| m.as_str().trim()).unwrap_or("");
        let row_html = caps.get(2).map(|m| m.as_str()).unwrap_or("");
        let date = time_re
            .captures(row_html)
            .and_then(|m| m.get(1))
            .map(|m| light_unescape(m.as_str().trim()))
            .unwrap_or_default();
        let anchors: Vec<String> = anchor_re
            .captures_iter(row_html)
            .filter_map(|m| m.get(1))
            .map(|m| clean_text(m.as_str()))
            .collect();
        if anchors.len() < 3 {
            continue;
        }
        rows.push(json!({
            "id": id,
            "date": date,
            "from": anchors[0],
            "to": anchors[1],
            "subject": anchors[2],
        }));
    }
    rows
}

fn extract_div_by_id(page: &str, div_id: &str, next_id: &str) -> String {
    let needle = format!("id=\"{}\"", div_id);
    let Some(id_at) = page.find(&needle) else {
        return String::new();
    };
    let Some(open_rel) = page[id_at..].find('>') else {
        return String::new();
    };
    let start = id_at + open_rel + 1;
    let mut end = None;
    if !next_id.is_empty() {
        let next = format!("<div id=\"{}\"", next_id);
        if let Some(pos) = page[start..].find(&next) {
            end = Some(start + pos);
        }
    }
    if end.is_none() {
        if let Some(pos) = page[start..].find("</div>") {
            end = Some(start + pos + "</div>".len());
        }
    }
    let Some(end) = end else {
        return String::new();
    };
    let mut content = page[start..end].trim().to_string();
    if content.ends_with("</div>") {
        content.truncate(content.len() - "</div>".len());
        content = content.trim().to_string();
    }
    content
}

fn extract_plain(page: &str, message_id: &str) -> String {
    let pattern = format!(
        r#"(?is)<div\s+id="text_plain_{}"[^>]*>\s*<pre>(.*?)</pre>\s*</div>"#,
        regex::escape(message_id)
    );
    let Ok(re) = Regex::new(&pattern) else {
        return String::new();
    };
    re.captures(page)
        .and_then(|m| m.get(1))
        .map(|m| light_unescape(m.as_str()).trim().to_string())
        .unwrap_or_default()
}

fn fetch_detail(local: &str, row: &Value) -> Value {
    let id = row.get("id").and_then(|v| v.as_str()).unwrap_or("").trim();
    if id.is_empty() {
        return row.clone();
    }
    let Ok(page) = fetch_html(detail_url(local, id)) else {
        return row.clone();
    };
    let mut detail = row.clone();
    if let Some(obj) = detail.as_object_mut() {
        obj.insert("text".to_string(), Value::String(extract_plain(&page, id)));
        obj.insert(
            "html".to_string(),
            Value::String(extract_div_by_id(
                &page,
                &format!("text_html_{}", id),
                &format!("text_plain_{}", id),
            )),
        );
    }
    detail
}

pub fn generate_email() -> Result<EmailInfo, String> {
    let local = random_local();
    fetch_html(mailbox_url(&local))?;
    Ok(EmailInfo {
        channel: Channel::Mailnesia,
        email: format!("{}@{}", local, DOMAIN),
        token: None,
        expires_at: None,
        created_at: None,
    })
}

pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let local = local_part(email);
    if local.is_empty() {
        return Err("mailnesia: empty email".into());
    }
    let page = fetch_html(mailbox_url(&local))?;
    let rows = parse_rows(&page);
    let mut out = Vec::new();
    for row in rows {
        out.push(normalize_email(&fetch_detail(&local, &row), email));
    }
    Ok(out)
}
