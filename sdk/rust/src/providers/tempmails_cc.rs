/*!
 * tempmails.cc — https://tempmails.cc
 *
 * 地址即邮箱：在公开免费域名上生成地址，无需注册。
 * 列表: GET /api/inbox/{email}
 * 详情: GET /api/message/{id}?address={email}&mark_read=false
 * 详情正文位于 textBody / htmlBody。
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use rand::RngExt;
use serde_json::{json, Value};

const BASE_URL: &str = "https://tempmails.cc";

fn random_local() -> String {
    const CHARS: &[u8] = b"abcdefghijklmnopqrstuvwxyz0123456789";
    let mut rng = rand::rng();
    (0..16)
        .map(|_| CHARS[rng.random_range(0..CHARS.len())] as char)
        .collect()
}

fn encode(value: &str) -> String {
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

async fn get_json(url: &str) -> Result<Value, String> {
    let resp = http_client()
        .get(url)
        .header("Accept", "application/json")
        .header("User-Agent", get_current_ua())
        .send()
        .await
        .map_err(|e| format!("tempmails-cc: 请求失败: {e}"))?;
    if !resp.status().is_success() {
        return Err(format!("tempmails-cc: HTTP {}", resp.status()));
    }
    resp.json().await.map_err(|e| e.to_string())
}

/// 在公开免费域名上生成地址
pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let data = get_json(&format!("{BASE_URL}/api/domains")).await?;
        let domain = data
            .get("domainInfo")
            .and_then(Value::as_array)
            .and_then(|list| {
                list.iter()
                    .find_map(|item| item.get("domain").and_then(Value::as_str))
            })
            .unwrap_or("");
        if domain.is_empty() {
            return Err("tempmails-cc: 无可用公开域名".into());
        }
        Ok(EmailInfo {
            channel: Channel::TempmailsCc,
            email: format!("{}@{domain}", random_local()),
            token: None,
            expires_at: None,
            created_at: None,
        })
    })
}

/// 跳过选域，在指定固定域名上生成地址
pub fn generate_email_at_domain(domain: &str, channel: Channel) -> Result<EmailInfo, String> {
    let domain = domain.trim();
    if domain.is_empty() {
        return Err("tempmails-cc: 域名为空".into());
    }
    Ok(EmailInfo {
        channel,
        email: format!("{}@{domain}", random_local()),
        token: None,
        expires_at: None,
        created_at: None,
    })
}

/// 获取 tempmails.cc 邮件，详情正文位于 textBody / htmlBody
pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let address = email.trim();
    if address.is_empty() {
        return Err("tempmails-cc: 邮箱地址为空".into());
    }
    block_on(async {
        let list = get_json(&format!("{BASE_URL}/api/inbox/{}", encode(address))).await?;
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
                "{BASE_URL}/api/message/{}?address={}&mark_read=false",
                encode(id),
                encode(address)
            ))
            .await
            {
                let to = detail
                    .get("to")
                    .and_then(Value::as_array)
                    .and_then(|a| a.first())
                    .and_then(Value::as_str)
                    .unwrap_or(address);
                out.push(normalize_email(
                    &json!({
                        "id": detail.get("id").cloned().unwrap_or(Value::Null),
                        "from": detail.get("from").cloned().unwrap_or(Value::Null),
                        "to": to,
                        "subject": detail.get("subject").cloned().unwrap_or(Value::Null),
                        "text": detail.get("textBody").cloned().unwrap_or(Value::Null),
                        "html": detail.get("htmlBody").cloned().unwrap_or(Value::Null),
                        "date": detail.get("date").cloned().unwrap_or(Value::Null),
                        "seen": detail.get("isRead").cloned().unwrap_or(Value::Null),
                    }),
                    address,
                ));
                continue;
            }
            out.push(normalize_email(
                &json!({
                    "id": id,
                    "from": item.get("from").cloned().unwrap_or(Value::Null),
                    "to": address,
                    "subject": item.get("subject").cloned().unwrap_or(Value::Null),
                    "date": item.get("date").cloned().unwrap_or(Value::Null),
                    "seen": item.get("isRead").cloned().unwrap_or(Value::Null),
                }),
                address,
            ));
        }
        Ok(out)
    })
}
