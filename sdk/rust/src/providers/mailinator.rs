/*!
 * Mailinator — https://mailinator.com
 * 使用公开 REST 接口读取 public inbox。
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use rand::Rng;
use serde_json::{json, Value};

const BASE_URL: &str = "https://mailinator.com";
const PUBLIC_DOMAIN: &str = "public";

fn random_string(len: usize) -> String {
    const CHARS: &[u8] = b"abcdefghijklmnopqrstuvwxyz0123456789";
    let mut rng = rand::thread_rng();
    (0..len)
        .map(|_| CHARS[rng.gen_range(0..CHARS.len())] as char)
        .collect()
}

fn default_headers(b: wreq::RequestBuilder) -> wreq::RequestBuilder {
    b.header("Accept", "application/json")
        .header(
            "Accept-Language",
            "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
        )
        .header("Cache-Control", "no-cache")
        .header("Pragma", "no-cache")
        .header("User-Agent", get_current_ua())
}

async fn request_json(url: String) -> Result<Value, String> {
    let resp = default_headers(http_client().get(url))
        .send()
        .await
        .map_err(|e| format!("mailinator request failed: {}", e))?;
    if !resp.status().is_success() {
        return Err(format!("mailinator http {}", resp.status()));
    }
    resp.json::<Value>()
        .await
        .map_err(|e| format!("parse failed: {}", e))
}

fn parse_messages(data: &Value) -> Vec<Value> {
    if let Some(arr) = data.as_array() {
        return arr.clone();
    }
    if let Some(arr) = data.get("msgs").and_then(|v| v.as_array()) {
        return arr.clone();
    }
    if let Some(arr) = data.get("data").and_then(|v| v.as_array()) {
        return arr.clone();
    }
    vec![]
}

fn first_text(obj: &Value, keys: &[&str]) -> String {
    for key in keys {
        if let Some(value) = obj.get(*key) {
            if let Some(s) = value.as_str() {
                return s.to_string();
            }
            if !value.is_null() {
                return value.to_string();
            }
        }
    }
    String::new()
}

fn attachment_url(value: Option<&Value>) -> Option<String> {
    let Some(Value::String(url)) = value else {
        return None;
    };
    if url.trim().is_empty() {
        return None;
    }
    if url.starts_with("http://") || url.starts_with("https://") {
        Some(url.to_string())
    } else {
        Some(format!("{}{}", BASE_URL, url))
    }
}

fn flatten_message(
    summary: &Value,
    recipient: &str,
    text_payload: &Value,
    html_payload: &Value,
    attachments_payload: &Value,
) -> Value {
    let attachments = attachments_payload
        .get("attachments")
        .and_then(|v| v.as_array())
        .map(|arr| {
            arr.iter()
                .filter_map(|item| {
                    let obj = item.as_object()?;
                    Some(json!({
                        "filename": obj.get("name").cloned().or_else(|| obj.get("filename").cloned()).unwrap_or_else(|| Value::String(String::new())),
                        "size": obj.get("size").cloned().or_else(|| obj.get("filesize").cloned()).unwrap_or(Value::Null),
                        "contentType": obj.get("contentType").cloned()
                            .or_else(|| obj.get("content_type").cloned())
                            .or_else(|| obj.get("mimeType").cloned())
                            .or_else(|| obj.get("mime_type").cloned())
                            .unwrap_or(Value::Null),
                        "url": attachment_url(obj.get("downloadUrl").or_else(|| obj.get("url"))),
                    }))
                })
                .collect::<Vec<_>>()
        })
        .unwrap_or_default();

    let id = summary
        .get("id")
        .cloned()
        .or_else(|| summary.get("messageId").cloned())
        .unwrap_or_else(|| Value::String(String::new()));
    let from = summary
        .get("from")
        .and_then(|v| v.as_str())
        .filter(|s| !s.trim().is_empty())
        .map(|s| s.to_string())
        .or_else(|| {
            summary
                .get("origfrom")
                .and_then(|v| v.as_str())
                .map(|s| s.to_string())
        })
        .unwrap_or_default();
    let to = summary
        .get("to")
        .and_then(|v| v.as_str())
        .filter(|s| !s.trim().is_empty())
        .map(|s| s.to_string())
        .unwrap_or_else(|| recipient.to_string());
    let date = summary
        .get("time")
        .cloned()
        .filter(|v| !v.is_null())
        .or_else(|| summary.get("date").cloned())
        .unwrap_or(Value::Null);

    json!({
        "id": id,
        "from": from,
        "to": to,
        "subject": summary.get("subject").cloned().unwrap_or_else(|| Value::String(String::new())),
        "text": first_text(text_payload, &["text/plain", "text", "body"]),
        "html": first_text(html_payload, &["text/html", "html", "body"]),
        "date": date,
        "isRead": false,
        "attachments": attachments,
    })
}

pub fn generate_email() -> Result<EmailInfo, String> {
    let local = random_string(12);
    Ok(EmailInfo {
        channel: Channel::Mailinator,
        email: format!("{}@mailinator.com", local),
        token: None,
        expires_at: None,
        created_at: None,
    })
}

pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let inbox = email
        .trim()
        .split('@')
        .next()
        .unwrap_or("")
        .trim()
        .to_string();
    if inbox.is_empty() {
        return Ok(vec![]);
    }
    let email = email.to_string();
    block_on(async move {
        let data = request_json(format!(
            "{}/api/v2/domains/{}/inboxes/{}",
            BASE_URL,
            PUBLIC_DOMAIN,
            urlencoding::encode(&inbox)
        ))
        .await?;
        let messages = parse_messages(&data);
        if messages.is_empty() {
            return Ok(vec![]);
        }

        let mut result = Vec::new();
        for msg in messages {
            let message_id = msg
                .get("id")
                .and_then(|v| v.as_str())
                .filter(|s| !s.trim().is_empty())
                .map(|s| s.to_string())
                .or_else(|| {
                    msg.get("messageId")
                        .and_then(|v| v.as_str())
                        .map(|s| s.to_string())
                })
                .unwrap_or_default();
            if message_id.is_empty() {
                continue;
            }

            let text_payload = request_json(format!(
                "{}/api/v2/domains/{}/messages/{}/text",
                BASE_URL,
                PUBLIC_DOMAIN,
                urlencoding::encode(&message_id)
            ))
            .await
            .unwrap_or(Value::Null);
            let html_payload = request_json(format!(
                "{}/api/v2/domains/{}/messages/{}/texthtml",
                BASE_URL,
                PUBLIC_DOMAIN,
                urlencoding::encode(&message_id)
            ))
            .await
            .unwrap_or(Value::Null);
            let attachments_payload = request_json(format!(
                "{}/api/v2/domains/{}/messages/{}/attachments",
                BASE_URL,
                PUBLIC_DOMAIN,
                urlencoding::encode(&message_id)
            ))
            .await
            .unwrap_or(Value::Null);

            let flat = flatten_message(
                &msg,
                &email,
                &text_payload,
                &html_payload,
                &attachments_payload,
            );
            result.push(normalize_email(&flat, &email));
        }
        Ok(result)
    })
}
