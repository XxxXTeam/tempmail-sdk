/*!
 * GrabMail — https://grabmail.io
 *
 * 地址即邮箱：首封邮件到达时自动存在，无需创建。
 * 列表: GET /api/v1/mailbox?address={email}
 * 详情: GET /api/v1/message/{id}?mailbox={email}
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use rand::RngExt;
use serde_json::{json, Value};

const BASE_URL: &str = "https://grabmail.io/api/v1";
const DOMAIN: &str = "grabmail.io";

fn random_local() -> String {
    const CHARS: &[u8] = b"abcdefghijklmnopqrstuvwxyz0123456789";
    let mut rng = rand::rng();
    let mut out = String::from("sdk");
    for _ in 0..16 {
        out.push(CHARS[rng.random_range(0..CHARS.len())] as char);
    }
    out
}

fn from_value(raw: &Value) -> String {
    let name = raw.get("from_name").and_then(Value::as_str).unwrap_or("");
    let addr = raw.get("from").and_then(Value::as_str).unwrap_or("");
    if name.is_empty() {
        addr.to_string()
    } else {
        format!("{name} <{addr}>")
    }
}

async fn get_json(url: &str) -> Result<Value, String> {
    let resp = http_client()
        .get(url)
        .header("Accept", "application/json")
        .header("User-Agent", get_current_ua())
        .send()
        .await
        .map_err(|e| format!("grabmail: 请求失败: {e}"))?;
    if !resp.status().is_success() {
        return Err(format!("grabmail: HTTP {}", resp.status()));
    }
    resp.json().await.map_err(|e| e.to_string())
}

/// 生成 GrabMail 地址（无需请求服务端）
pub fn generate_email() -> Result<EmailInfo, String> {
    Ok(EmailInfo {
        channel: Channel::Grabmail,
        email: format!("{}@{DOMAIN}", random_local()),
        token: None,
        expires_at: None,
        created_at: None,
    })
}

/// 获取 GrabMail 邮件，逐条补全正文
pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let address = email.trim();
    if address.is_empty() {
        return Err("grabmail: 邮箱地址为空".into());
    }
    block_on(async {
        let list = get_json(&format!(
            "{BASE_URL}/mailbox?address={}",
            urlencoding_encode(address)
        ))
        .await?;
        let rows = list
            .get("messages")
            .and_then(Value::as_array)
            .cloned()
            .unwrap_or_default();
        let mut out = Vec::new();
        for item in rows {
            let id = item.get("id").and_then(Value::as_str).unwrap_or("");
            if id.is_empty() {
                continue;
            }
            if let Ok(detail) = get_json(&format!(
                "{BASE_URL}/message/{}?mailbox={}",
                urlencoding_encode(id),
                urlencoding_encode(address)
            ))
            .await
            {
                out.push(normalize_email(
                    &json!({
                        "id": detail.get("id").cloned().unwrap_or(Value::Null),
                        "from": from_value(&detail),
                        "to": address,
                        "subject": detail.get("subject").cloned().unwrap_or(Value::Null),
                        "text": detail.get("text").cloned().unwrap_or(Value::Null),
                        "html": detail.get("html").cloned().unwrap_or(Value::Null),
                        "date": detail.get("date").cloned().unwrap_or(Value::Null),
                        "seen": detail.get("seen").cloned().unwrap_or(Value::Null),
                        "attachments": detail.get("attachments").cloned().unwrap_or(json!([])),
                    }),
                    address,
                ));
                continue;
            }
            out.push(normalize_email(
                &json!({
                    "id": id,
                    "from": from_value(&item),
                    "to": address,
                    "subject": item.get("subject").cloned().unwrap_or(Value::Null),
                    "text": item.get("preview").cloned().unwrap_or(Value::Null),
                    "date": item.get("date").cloned().unwrap_or(Value::Null),
                    "seen": item.get("seen").cloned().unwrap_or(Value::Null),
                }),
                address,
            ));
        }
        Ok(out)
    })
}

fn urlencoding_encode(value: &str) -> String {
    value
        .bytes()
        .map(|b| match b {
            b'A'..=b'Z' | b'a'..=b'z' | b'0'..=b'9' | b'@' | b'.' | b'-' | b'_' | b'~' => {
                (b as char).to_string()
            }
            _ => format!("%{b:02X}"),
        })
        .collect()
}
