/*!
 * maildrop.cc 渠道实现 (GraphQL)
 *
 * 特点:
 * - 无需认证，公开 GraphQL API
 * - data 字段返回原始 MIME 源码，需要解析提取纯文本
 * - headerfrom/subject 可能含 RFC 2047 编码，需要解码
 */

use reqwest::blocking::Client;
use serde_json::Value;
use rand::Rng;
use crate::types::{Channel, EmailInfo, Email};

const GRAPHQL_URL: &str = "https://api.maildrop.cc/graphql";
const DOMAIN: &str = "maildrop.cc";

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

/**
 * 从原始 MIME 邮件源码中提取纯文本正文
 * maildrop 的 data 字段返回完整 MIME 源码，需要解析出 text/plain 部分
 */
fn extract_text_from_mime(raw: &str) -> String {
    use base64::Engine;
    if raw.is_empty() {
        return String::new();
    }

    /* 分离邮件头和正文 */
    let split_pos = match raw.find("\r\n\r\n") {
        Some(pos) => pos,
        None => return raw.to_string(),
    };
    let headers = &raw[..split_pos];
    let body = &raw[split_pos + 4..];

    /* 检查是否为 multipart，提取 boundary */
    let headers_lower = headers.to_lowercase();
    let boundary = extract_boundary(&headers_lower, headers);

    if let Some(boundary) = boundary {
        let delimiter = format!("--{}", boundary);
        let parts: Vec<&str> = body.split(&delimiter).collect();
        for part in &parts {
            let part_lower = part.to_lowercase();
            if part_lower.contains("content-type:") && part_lower.contains("text/plain") {
                if let Some(part_body_start) = part.find("\r\n\r\n") {
                    let content = part[part_body_start + 4..].trim_end_matches("--").trim();
                    /* 检查 base64 编码 */
                    if part_lower.contains("content-transfer-encoding:") && part_lower.contains("base64") {
                        let clean = content.replace("\r\n", "").replace("\n", "");
                        if let Ok(decoded) = base64::engine::general_purpose::STANDARD.decode(&clean) {
                            return String::from_utf8_lossy(&decoded).trim().to_string();
                        }
                    }
                    return content.trim().to_string();
                }
            }
        }
    }

    /* 非 multipart：检查整体 base64 编码 */
    if headers_lower.contains("content-transfer-encoding:") && headers_lower.contains("base64") {
        let clean = body.replace("\r\n", "").replace("\n", "");
        if let Ok(decoded) = base64::engine::general_purpose::STANDARD.decode(&clean) {
            return String::from_utf8_lossy(&decoded).trim().to_string();
        }
    }

    body.trim().to_string()
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

/**
 * 发送 GraphQL 请求
 * 使用 operationName + variables 的标准 GraphQL 格式
 */
fn graphql_request(
    operation_name: &str,
    query: &str,
    variables: serde_json::Value,
) -> Result<Value, String> {
    let resp = Client::builder()
        .user_agent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36")
        .timeout(std::time::Duration::from_secs(15))
        .build().unwrap()
        .post(GRAPHQL_URL)
        .header("Content-Type", "application/json")
        .header("Origin", "https://maildrop.cc")
        .header("Referer", "https://maildrop.cc/")
        .json(&serde_json::json!({
            "operationName": operation_name,
            "variables": variables,
            "query": query,
        }))
        .send().map_err(|e| format!("maildrop request failed: {}", e))?;

    if !resp.status().is_success() {
        return Err(format!("maildrop request failed: {}", resp.status()));
    }

    let data: Value = resp.json().map_err(|e| format!("parse failed: {}", e))?;
    if let Some(errors) = data["errors"].as_array() {
        if !errors.is_empty() {
            return Err(format!("Maildrop GraphQL error: {}", errors[0]["message"].as_str().unwrap_or("unknown")));
        }
    }

    Ok(data["data"].clone())
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
            emails.push(Email {
                id: msg["id"].as_str().unwrap_or(id).to_string(),
                from_addr: decode_rfc2047(msg["headerfrom"].as_str().unwrap_or("")),
                to: email.to_string(),
                subject: decode_rfc2047(msg["subject"].as_str().unwrap_or("")),
                text: extract_text_from_mime(msg["data"].as_str().unwrap_or("")),
                html: msg["html"].as_str().unwrap_or("").to_string(),
                date: msg["date"].as_str().unwrap_or("").to_string(),
                is_read: false,
                attachments: vec![],
            });
        }
    }

    Ok(emails)
}
