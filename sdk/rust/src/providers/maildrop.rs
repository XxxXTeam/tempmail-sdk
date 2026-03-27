/*!
 * maildrop.cc 渠道实现 (GraphQL)
 *
 * 特点:
 * - 无需认证，公开 GraphQL API
 * - data 字段返回原始 MIME 源码，需要解析提取纯文本
 * - headerfrom/subject 可能含 RFC 2047 编码，需要解码
 */

use std::sync::LazyLock;

use serde_json::Value;
use rand::Rng;
use regex::Regex;
use crate::types::{Channel, EmailInfo, Email};
use crate::config::{http_client, block_on, get_current_ua};

const GRAPHQL_URL: &str = "https://api.maildrop.cc/graphql";
const DOMAIN: &str = "maildrop.cc";

static MAILDROP_STRIP_TAG_RE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r"<[^>]*>").expect("tag strip regex"));
static MAILDROP_STRIP_WS_RE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r"\s+").expect("ws strip regex"));

fn random_username(len: usize) -> String {
    let chars: Vec<char> = "abcdefghijklmnopqrstuvwxyz0123456789".chars().collect();
    let mut rng = rand::thread_rng();
    (0..len).map(|_| chars[rng.gen_range(0..chars.len())]).collect()
}

/**
 * 解码 RFC 2047 编码的邮件头
 * 支持 Base64 (B) 和 Quoted-Printable (Q) 编码
 * 如 =?utf-8?B?b3BlbmVs?= 解码为 "openel"
 */
fn decode_rfc2047(s: &str) -> String {
    use base64::Engine;
    let re = regex::Regex::new(r"=\?([^?]+)\?(B|Q)\?([^?]*)\?=").unwrap();
    let result = re.replace_all(s, |caps: &regex::Captures| {
        let encoding = caps.get(2).map_or("", |m| m.as_str());
        let encoded = caps.get(3).map_or("", |m| m.as_str());
        match encoding.to_uppercase().as_str() {
            "B" => {
                match base64::engine::general_purpose::STANDARD.decode(encoded) {
                    Ok(bytes) => String::from_utf8_lossy(&bytes).to_string(),
                    Err(_) => encoded.to_string(),
                }
            }
            "Q" => {
                let bytes: Vec<u8> = encoded.as_bytes().iter().copied().collect::<Vec<_>>();
                let mut decoded = Vec::new();
                let mut i = 0;
                while i < bytes.len() {
                    if bytes[i] == b'_' {
                        decoded.push(b' ');
                        i += 1;
                    } else if bytes[i] == b'=' && i + 2 < bytes.len() {
                        if let Ok(byte) = u8::from_str_radix(
                            &String::from_utf8_lossy(&bytes[i+1..i+3]), 16
                        ) {
                            decoded.push(byte);
                            i += 3;
                        } else {
                            decoded.push(bytes[i]);
                            i += 1;
                        }
                    } else {
                        decoded.push(bytes[i]);
                        i += 1;
                    }
                }
                String::from_utf8_lossy(&decoded).to_string()
            }
            _ => encoded.to_string(),
        }
    });
    result.to_string()
}

fn split_mime_header_body(raw: &str) -> Option<(&str, &str)> {
    if let Some(pos) = raw.find("\r\n\r\n") {
        return Some((&raw[..pos], &raw[pos + 4..]));
    }
    raw.find("\n\n").map(|pos| (&raw[..pos], &raw[pos + 2..]))
}

fn split_part_header_content(part: &str) -> Option<(&str, &str)> {
    if let Some(p) = part.find("\r\n\r\n") {
        return Some((&part[..p], &part[p + 4..]));
    }
    part.find("\n\n").map(|p| (&part[..p], &part[p + 2..]))
}

fn hex_qp(b: u8) -> Option<u8> {
    match b {
        b'0'..=b'9' => Some(b - b'0'),
        b'a'..=b'f' => Some(b - b'a' + 10),
        b'A'..=b'F' => Some(b - b'A' + 10),
        _ => None,
    }
}

fn decode_quoted_printable(input: &str) -> String {
    let bytes = input.as_bytes();
    let mut out: Vec<u8> = Vec::with_capacity(bytes.len());
    let mut i = 0;
    while i < bytes.len() {
        if bytes[i] == b'=' && i + 1 < bytes.len() {
            if bytes[i + 1] == b'\r' && i + 2 < bytes.len() && bytes[i + 2] == b'\n' {
                i += 3;
                continue;
            }
            if bytes[i + 1] == b'\n' {
                i += 2;
                continue;
            }
            if i + 2 < bytes.len() {
                if let (Some(a), Some(b)) = (hex_qp(bytes[i + 1]), hex_qp(bytes[i + 2])) {
                    out.push(a << 4 | b);
                    i += 3;
                    continue;
                }
            }
        }
        out.push(bytes[i]);
        i += 1;
    }
    String::from_utf8_lossy(&out).into_owned()
}

fn part_decode_body(content: &str, part_head_lower: &str) -> String {
    use base64::Engine;
    let l = part_head_lower.to_lowercase();
    if l.contains("content-transfer-encoding:") && l.contains("base64") {
        let clean: String = content.chars().filter(|c| !c.is_whitespace()).collect();
        if let Ok(decoded) = base64::engine::general_purpose::STANDARD.decode(&clean) {
            return String::from_utf8_lossy(&decoded).into_owned();
        }
    }
    if l.contains("content-transfer-encoding:") && l.contains("quoted-printable") {
        return decode_quoted_printable(content);
    }
    content.to_string()
}

fn strip_html_to_text(html: &str) -> String {
    if html.is_empty() {
        return String::new();
    }
    let s = MAILDROP_STRIP_TAG_RE.replace_all(html, " ");
    MAILDROP_STRIP_WS_RE.replace_all(&s, " ").trim().to_string()
}

/* 从邮件头中提取 boundary 参数 */
fn extract_boundary<'a>(headers_lower: &str, headers_orig: &'a str) -> Option<String> {
    if let Some(ct_pos) = headers_lower.find("content-type:") {
        let ct_rest = &headers_orig[ct_pos..];
        /* 合并折叠行 */
        let ct_full = ct_rest
            .replace("\r\n\t", " ")
            .replace("\r\n ", " ");
        if let Some(b_pos) = ct_full.to_lowercase().find("boundary=") {
            let after = &ct_full[b_pos + 9..];
            let boundary = if after.starts_with('"') {
                after[1..].split('"').next().unwrap_or("").to_string()
            } else {
                after.split(|c: char| c.is_whitespace() || c == ';')
                    .next().unwrap_or("").to_string()
            };
            if !boundary.is_empty() {
                return Some(boundary);
            }
        }
    }
    None
}

fn extract_text_from_mime(raw: &str) -> String {
    use base64::Engine;
    if raw.is_empty() {
        return String::new();
    }
    let (headers, body) = match split_mime_header_body(raw) {
        Some(hb) => hb,
        None => return raw.trim().to_string(),
    };
    let headers_lower = headers.to_lowercase();
    if let Some(boundary) = extract_boundary(&headers_lower, headers) {
        let delimiter = format!("--{}", boundary);
        for part in body.split(&delimiter) {
            let pl = part.to_lowercase();
            if pl.contains("content-type:") && pl.contains("text/plain") {
                if let Some((ph, pc)) = split_part_header_content(part) {
                    let phl = ph.to_lowercase();
                    let content = pc
                        .trim_end_matches('\r')
                        .trim_end_matches('\n')
                        .trim_end_matches('-')
                        .trim();
                    let decoded = part_decode_body(content, &phl);
                    return decoded.trim().to_string();
                }
            }
        }
    }

    if headers_lower.contains("content-transfer-encoding:") && headers_lower.contains("base64") {
        let clean: String = body.chars().filter(|c| !c.is_whitespace()).collect();
        if let Ok(decoded) = base64::engine::general_purpose::STANDARD.decode(&clean) {
            return String::from_utf8_lossy(&decoded).trim().to_string();
        }
    }
    if headers_lower.contains("content-transfer-encoding:") && headers_lower.contains("quoted-printable") {
        return decode_quoted_printable(body).trim().to_string();
    }

    body.trim().to_string()
}

fn extract_html_from_mime(raw: &str) -> String {
    use base64::Engine;
    if raw.is_empty() {
        return String::new();
    }
    let (headers, body) = match split_mime_header_body(raw) {
        Some(hb) => hb,
        None => return String::new(),
    };
    let headers_lower = headers.to_lowercase();
    if let Some(boundary) = extract_boundary(&headers_lower, headers) {
        let delimiter = format!("--{}", boundary);
        for part in body.split(&delimiter) {
            let pl = part.to_lowercase();
            if pl.contains("content-type:") && pl.contains("text/html") {
                if let Some((ph, pc)) = split_part_header_content(part) {
                    let phl = ph.to_lowercase();
                    let content = pc
                        .trim_end_matches('\r')
                        .trim_end_matches('\n')
                        .trim_end_matches('-')
                        .trim();
                    let decoded = part_decode_body(content, &phl);
                    return decoded.trim().to_string();
                }
            }
        }
    }

    if headers_lower.contains("content-type:") && headers_lower.contains("text/html") {
        let decoded = if headers_lower.contains("content-transfer-encoding:") && headers_lower.contains("base64") {
            let clean: String = body.chars().filter(|c| !c.is_whitespace()).collect();
            base64::engine::general_purpose::STANDARD
                .decode(&clean)
                .map(|d| String::from_utf8_lossy(&d).into_owned())
                .unwrap_or_else(|_| body.to_string())
        } else if headers_lower.contains("content-transfer-encoding:")
            && headers_lower.contains("quoted-printable")
        {
            decode_quoted_printable(body)
        } else {
            body.to_string()
        };
        return decoded.trim().to_string();
    }

    String::new()
}

/**
 * 发送 GraphQL 请求
 * 使用 operationName + variables 的标准 GraphQL 格式
 */
fn graphql_request(
    operation_name: &str,
    query: &str,
    variables: serde_json::Value,
) -> Result<Value, String> {
    let op = operation_name.to_string();
    let q = query.to_string();
    block_on(async {
        let resp = http_client()
            .post(GRAPHQL_URL)
            .header("Content-Type", "application/json")
            .header("User-Agent", get_current_ua())
            .header("Origin", "https://maildrop.cc")
            .header("Referer", "https://maildrop.cc/")
            .json(&serde_json::json!({
                "operationName": op,
                "variables": variables,
                "query": q,
            }))
            .send().await.map_err(|e| format!("maildrop request failed: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("maildrop request failed: {}", resp.status()));
        }

        let data: Value = resp.json().await.map_err(|e| format!("parse failed: {}", e))?;
        if let Some(errors) = data["errors"].as_array() {
            if !errors.is_empty() {
                return Err(format!("Maildrop GraphQL error: {}", errors[0]["message"].as_str().unwrap_or("unknown")));
            }
        }

        Ok(data["data"].clone())
    })
}

pub fn generate_email() -> Result<EmailInfo, String> {
    let username = random_username(10);
    let email = format!("{}@{}", username, DOMAIN);

    /* 验证 API 可用 */
    graphql_request(
        "GetInbox",
        "query GetInbox($mailbox: String!) { inbox(mailbox: $mailbox) { id } }",
        serde_json::json!({"mailbox": username}),
    )?;

    Ok(EmailInfo {
        channel: Channel::Maildrop,
        email,
        token: Some(username),
        expires_at: None,
        created_at: None,
    })
}

pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let data = graphql_request(
        "GetInbox",
        "query GetInbox($mailbox: String!) { inbox(mailbox: $mailbox) { id headerfrom subject date } }",
        serde_json::json!({"mailbox": token}),
    )?;

    let inbox = data["inbox"].as_array().cloned().unwrap_or_default();
    if inbox.is_empty() {
        return Ok(vec![]);
    }

    let mut emails = Vec::new();
    for item in &inbox {
        let id = item["id"].as_str().unwrap_or("");
        let msg_data = graphql_request(
            "GetMessage",
            "query GetMessage($mailbox: String!, $id: String!) { message(mailbox: $mailbox, id: $id) { id headerfrom subject date data html } }",
            serde_json::json!({"mailbox": token, "id": id}),
        );

        if let Ok(data) = msg_data {
            let msg = &data["message"];
            let raw = msg["data"].as_str().unwrap_or("");
            let plain = extract_text_from_mime(raw);
            let from_mime = extract_html_from_mime(raw);
            let api_html = msg["html"].as_str().unwrap_or("").trim();
            let html_out = if !api_html.is_empty() {
                api_html.to_string()
            } else {
                from_mime
            };
            let text_out = if !plain.is_empty() {
                plain
            } else {
                strip_html_to_text(&html_out)
            };
            emails.push(Email {
                id: msg["id"].as_str().unwrap_or(id).to_string(),
                from_addr: decode_rfc2047(msg["headerfrom"].as_str().unwrap_or("")),
                to: email.to_string(),
                subject: decode_rfc2047(msg["subject"].as_str().unwrap_or("")),
                text: text_out,
                html: html_out,
                date: msg["date"].as_str().unwrap_or("").to_string(),
                is_read: false,
                attachments: vec![],
            });
        }
    }

    Ok(emails)
}
