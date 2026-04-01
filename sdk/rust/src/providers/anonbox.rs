/*!
 * anonbox.net (Chaos Computer Club) — GET /en/ 解析 HTML，mbox 拉取邮件
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use regex::Regex;
use std::sync::LazyLock;

const PAGE_URL: &str = "https://anonbox.net/en/";
const BASE: &str = "https://anonbox.net";

static MAIL_LINK_RE: LazyLock<Regex> = LazyLock::new(|| {
    Regex::new(r#"<a href="https://anonbox\.net/([^/]+)/([^"]+)">https://anonbox\.net/[^"]+</a>"#).unwrap()
});
static DD_RE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r"(?is)<dd([^>]*)>([\s\S]*?)</dd>").unwrap());
static DISPLAY_NONE: LazyLock<Regex> = LazyLock::new(|| Regex::new(r"display\s*:\s*none").unwrap());
static P_RE: LazyLock<Regex> = LazyLock::new(|| Regex::new(r"(?is)<p>([\s\S]*?)</p>").unwrap());
static TAG_RE: LazyLock<Regex> = LazyLock::new(|| Regex::new(r"<[^>]+>").unwrap());
static EXPIRES_RE: LazyLock<Regex> = LazyLock::new(|| {
    Regex::new(r"(?is)Your mail address is valid until:</dt>\s*<dd><p>([^<]+)</p>").unwrap()
});
static MBOX_SPLIT: LazyLock<Regex> = LazyLock::new(|| Regex::new(r"\r?\n(?=From )").unwrap());

fn strip_tags(html: &str) -> String {
    let s = TAG_RE.replace_all(html, "");
    s.replace("&nbsp;", " ")
        .replace("&amp;", "&")
        .replace("&lt;", "<")
        .replace("&gt;", ">")
        .trim()
        .to_string()
}

fn u32_to_base36(mut n: u64) -> String {
    if n == 0 {
        return "0".into();
    }
    const DIGITS: &[u8; 36] = b"0123456789abcdefghijklmnopqrstuvwxyz";
    let mut buf = Vec::new();
    while n > 0 {
        buf.push(DIGITS[(n % 36) as usize]);
        n /= 36;
    }
    buf.reverse();
    String::from_utf8(buf).unwrap_or_else(|_| "0".into())
}

fn simple_hash(s: &str) -> String {
    let mut h: u32 = 0;
    for b in s.bytes() {
        h = h.wrapping_mul(31).wrapping_add(b as u32);
    }
    u32_to_base36(h as u64)
}

fn parse_en_page(html: &str) -> Result<(String, String, Option<String>), String> {
    let caps = MAIL_LINK_RE
        .captures(html)
        .ok_or_else(|| "anonbox: mailbox link not found".to_string())?;
    let inbox = caps.get(1).map(|m| m.as_str()).unwrap_or("");
    let secret = caps.get(2).map(|m| m.as_str()).unwrap_or("");
    let token = format!("{inbox}/{secret}");

    let mut address_html: Option<String> = None;
    for cap in DD_RE.captures_iter(html) {
        let attrs = cap.get(1).map(|m| m.as_str()).unwrap_or("");
        if DISPLAY_NONE.is_match(attrs) {
            continue;
        }
        let inner = cap.get(2).map(|m| m.as_str()).unwrap_or("");
        let Some(pm) = P_RE.captures(inner) else { continue };
        let p_inner = pm.get(1).map(|m| m.as_str()).unwrap_or("");
        if !p_inner.contains('@') {
            continue;
        }
        if p_inner.to_lowercase().contains("googlemail.com") {
            continue;
        }
        if !p_inner.to_lowercase().contains("anonbox") {
            continue;
        }
        address_html = Some(p_inner.to_string());
        break;
    }
    let addr = address_html.ok_or_else(|| "anonbox: address paragraph not found".to_string())?;
    let merged = strip_tags(&addr);
    let at = merged.find('@').ok_or_else(|| "anonbox: bad address".to_string())?;
    let local = merged[..at].trim();
    if local.is_empty() {
        return Err("anonbox: empty local part".into());
    }
    let email = format!("{local}@{inbox}.anonbox.net");
    let exp = EXPIRES_RE
        .captures(html)
        .and_then(|c| c.get(1).map(|m| m.as_str().trim().to_string()));
    Ok((email, token, exp))
}

fn decode_qp_if_needed(body: &str, header_block: &str) -> String {
    let te_re = Regex::new(r"(?i)^content-transfer-encoding:\s*([^\s]+)").unwrap();
    let enc = te_re
        .captures(header_block)
        .and_then(|c| c.get(1).map(|m| m.as_str().to_lowercase()))
        .unwrap_or_default();
    if enc != "quoted-printable" {
        return body.trim_end_matches(['\r', '\n']).to_string();
    }
    let soft = Regex::new(r"=\r?\n").unwrap().replace_all(body, "");
    let mut out = Vec::new();
    let s = soft.as_ref().as_bytes();
    let mut i = 0;
    while i < s.len() {
        if s[i] == b'=' && i + 2 < s.len() {
            let a = s[i + 1];
            let b = s[i + 2];
            if a.is_ascii_hexdigit() && b.is_ascii_hexdigit() {
                let v = u8::from_str_radix(std::str::from_utf8(&[a, b]).unwrap_or("00"), 16).unwrap_or(0);
                out.push(v);
                i += 3;
                continue;
            }
        }
        out.push(s[i]);
        i += 1;
    }
    String::from_utf8_lossy(&out).trim_end_matches(['\r', '\n']).to_string()
}

fn mbox_block_to_email(block: &str, recipient: &str) -> Email {
    let normalized = block.replace("\r\n", "\n");
    let lines: Vec<&str> = normalized.split('\n').collect();
    let mut i = 0;
    if !lines.is_empty() && lines[0].starts_with("From ") {
        i = 1;
    }
    let mut headers: std::collections::HashMap<String, String> = std::collections::HashMap::new();
    let mut cur_key = String::new();
    while i < lines.len() {
        let line = lines[i];
        if line.is_empty() {
            i += 1;
            break;
        }
        if line.starts_with(' ') || line.starts_with('\t') {
            if !cur_key.is_empty() {
                if let Some(v) = headers.get_mut(&cur_key) {
                    *v = format!("{} {}", v, line.trim());
                }
            }
        } else if let Some(idx) = line.find(':') {
            cur_key = line[..idx].trim().to_lowercase();
            let val = line[idx + 1..].trim().to_string();
            headers.insert(cur_key.clone(), val);
        }
        i += 1;
    }
    let body = lines[i..].join("\n");
    let ct = headers
        .get("content-type")
        .map(|s| s.to_lowercase())
        .unwrap_or_default();

    let mut text = String::new();
    let mut html = String::new();
    if ct.contains("multipart/") {
        let b_re = Regex::new(r#"(?i)boundary="?([^";\s]+)"?"#).unwrap();
        if let Some(bm) = b_re.captures(headers.get("content-type").map(|s| s.as_str()).unwrap_or("")) {
            let boundary = bm.get(1).map(|m| m.as_str()).unwrap_or("");
            let esc = regex::escape(boundary);
            let part_re = Regex::new(&format!(r"\r?\n--{esc}(?:--)?\r?\n")).unwrap();
            for part in part_re.split(&body) {
                let part = part.trim();
                if part.is_empty() || part == "--" {
                    continue;
                }
                if let Some(sep) = part.find("\n\n") {
                    let ph = &part[..sep];
                    let pb = &part[sep + 2..];
                    let pct_re = Regex::new(r"(?i)^content-type:\s*([^\s;]+)").unwrap();
                    let pct = pct_re
                        .captures(ph)
                        .and_then(|c| c.get(1).map(|m| m.as_str().to_lowercase()))
                        .unwrap_or_default();
                    if pct == "text/plain" {
                        text = decode_qp_if_needed(pb, ph);
                    } else if pct == "text/html" {
                        html = decode_qp_if_needed(pb, ph);
                    }
                }
            }
        }
    }
    if text.is_empty() && html.is_empty() {
        if ct.contains("text/html") {
            html = decode_qp_if_needed(&body, headers.get("content-type").map(|s| s.as_str()).unwrap_or(""));
        } else {
            text = decode_qp_if_needed(&body, headers.get("content-type").map(|s| s.as_str()).unwrap_or(""));
        }
    }

    let from = headers.get("from").cloned().unwrap_or_default();
    let to = headers.get("to").cloned().unwrap_or_else(|| recipient.to_string());
    let subject = headers.get("subject").cloned().unwrap_or_default();
    let date = headers
        .get("date")
        .and_then(|d| chrono::DateTime::parse_from_rfc2822(d.trim()).ok())
        .map(|d| d.with_timezone(&chrono::Utc).to_rfc3339())
        .unwrap_or_else(|| chrono::Utc::now().to_rfc3339());
    let id = headers
        .get("message-id")
        .cloned()
        .unwrap_or_else(|| simple_hash(&block.chars().take(512).collect::<String>()));

    normalize_email(
        &serde_json::json!({
            "id": id,
            "from": from,
            "to": to,
            "subject": subject,
            "body_text": text,
            "body_html": html,
            "date": date,
            "isRead": false,
            "attachments": [],
        }),
        recipient,
    )
}

pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let resp = http_client()
            .get(PAGE_URL)
            .header("User-Agent", get_current_ua())
            .header("Accept", "text/html,application/xhtml+xml")
            .send()
            .await
            .map_err(|e| format!("anonbox: {}", e))?;
        if !resp.status().is_success() {
            return Err(format!("anonbox: generate HTTP {}", resp.status()));
        }
        let html = resp.text().await.map_err(|e| format!("anonbox: {}", e))?;
        let (email, token, exp) = parse_en_page(&html)?;
        Ok(EmailInfo {
            channel: Channel::Anonbox,
            email,
            token: Some(token),
            expires_at: None,
            created_at: exp,
        })
    })
}

pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    if token.is_empty() {
        return Err("token is required for anonbox".into());
    }
    let path = if token.ends_with('/') {
        token.to_string()
    } else {
        format!("{token}/")
    };
    let url = format!("{BASE}/{path}");
    block_on(async {
        let resp = http_client()
            .get(&url)
            .header("User-Agent", get_current_ua())
            .header("Accept", "text/plain,*/*")
            .send()
            .await
            .map_err(|e| format!("anonbox: {}", e))?;
        if resp.status().as_u16() == 404 {
            return Ok(vec![]);
        }
        if !resp.status().is_success() {
            return Err(format!("anonbox: get emails HTTP {}", resp.status()));
        }
        let raw = resp.text().await.map_err(|e| format!("anonbox: {}", e))?;
        let t = raw.trim();
        if t.is_empty() {
            return Ok(vec![]);
        }
        let blocks: Vec<String> = MBOX_SPLIT
            .split(t)
            .map(|s| s.trim().to_string())
            .filter(|s| !s.is_empty())
            .collect();
        Ok(blocks.iter().map(|b| mbox_block_to_email(b, email)).collect())
    })
}
