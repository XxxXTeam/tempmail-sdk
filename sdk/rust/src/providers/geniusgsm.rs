/*!
 * GeniusGSM — https://tempmail.geniusgsm.com
 *
 * 列表: GET /api/domains/ 选择激活域名。
 * 创建: POST /api/inboxes/create_inbox/ {domain_id,ttl} → id/email。
 * 收件: GET /api/inboxes/{id}/emails/（响应已包含 body_text/body_html）。
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::{json, Value};

const BASE_URL: &str = "https://tempmail.geniusgsm.com/api";

fn rows_of(data: &Value) -> Vec<Value> {
    if let Some(arr) = data.as_array() {
        return arr.clone();
    }
    data.get("results")
        .and_then(Value::as_array)
        .cloned()
        .unwrap_or_default()
}

fn from_value(raw: &Value) -> String {
    let name = raw.get("from_name").and_then(Value::as_str).unwrap_or("");
    let addr = raw.get("from_email").and_then(Value::as_str).unwrap_or("");
    if name.is_empty() {
        addr.to_string()
    } else {
        format!("{name} <{addr}>")
    }
}

async fn api_get(url: &str) -> Result<Value, String> {
    let resp = http_client()
        .get(url)
        .header("Accept", "application/json")
        .header("User-Agent", get_current_ua())
        .send()
        .await
        .map_err(|e| format!("geniusgsm: 请求失败: {e}"))?;
    if !resp.status().is_success() {
        return Err(format!("geniusgsm: HTTP {}", resp.status()));
    }
    resp.json().await.map_err(|e| e.to_string())
}

/// 选择激活域名并创建收件箱；内部令牌即收件箱 ID
pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let domains = api_get(&format!("{BASE_URL}/domains/")).await?;
        let domain_id = rows_of(&domains)
            .iter()
            .find(|item| item.get("is_active").and_then(Value::as_bool) == Some(true))
            .and_then(|item| item.get("id"))
            .and_then(Value::as_i64)
            .unwrap_or(0);
        if domain_id == 0 {
            return Err("geniusgsm: 无可用激活域名".into());
        }
        let resp = http_client()
            .post(format!("{BASE_URL}/inboxes/create_inbox/"))
            .header("Content-Type", "application/json")
            .header("Accept", "application/json")
            .header("User-Agent", get_current_ua())
            .body(format!(r#"{{"domain_id":{domain_id},"ttl":3600}}"#))
            .send()
            .await
            .map_err(|e| format!("geniusgsm: 创建邮箱请求失败: {e}"))?;
        if !resp.status().is_success() {
            return Err(format!("geniusgsm: 创建邮箱 HTTP {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let id = data.get("id").and_then(Value::as_str).unwrap_or("");
        let email = data.get("email").and_then(Value::as_str).unwrap_or("");
        if id.is_empty() || email.is_empty() {
            return Err("geniusgsm: 创建响应不完整".into());
        }
        let expires_at = data
            .get("expires_at")
            .and_then(Value::as_str)
            .and_then(|s| chrono::DateTime::parse_from_rfc3339(s).ok())
            .map(|d| d.timestamp_millis());
        Ok(EmailInfo {
            channel: Channel::GeniusGSM,
            email: email.to_string(),
            token: Some(id.to_string()),
            expires_at,
            created_at: data.get("created_at").map(|v| v.to_string()),
        })
    })
}

/// 列表响应已包含完整正文
pub fn get_emails(email: &str, token: &str) -> Result<Vec<Email>, String> {
    if token.is_empty() {
        return Err("geniusgsm: 收件箱 ID 为空".into());
    }
    block_on(async {
        let data = api_get(&format!(
            "{BASE_URL}/inboxes/{}/emails/",
            urlencoding_encode(token)
        ))
        .await?;
        let rows = rows_of(&data);
        Ok(rows
            .iter()
            .map(|item| {
                normalize_email(
                    &json!({
                        "id": item.get("id").cloned().unwrap_or(Value::Null),
                        "from": from_value(item),
                        "to": item.get("inbox_email").cloned().unwrap_or(json!(email)),
                        "subject": item.get("subject").cloned().unwrap_or(Value::Null),
                        "text": item.get("body_text").cloned().unwrap_or(Value::Null),
                        "html": item.get("body_html").cloned().unwrap_or(Value::Null),
                        "date": item.get("received_at").cloned().unwrap_or(Value::Null),
                        "read": item.get("is_read").cloned().unwrap_or(Value::Null),
                        "attachments": item.get("attachments").cloned().unwrap_or(json!([])),
                    }),
                    email,
                )
            })
            .collect())
    })
}

fn urlencoding_encode(value: &str) -> String {
    value
        .bytes()
        .map(|b| match b {
            b'A'..=b'Z' | b'a'..=b'z' | b'0'..=b'9' | b'-' | b'_' | b'.' | b'~' => {
                (b as char).to_string()
            }
            _ => format!("%{b:02X}"),
        })
        .collect()
}
