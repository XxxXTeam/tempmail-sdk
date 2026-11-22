/*!
 * DummyInbox — https://dummyinbox.com
 *
 * 无需密钥；创建随机地址后，地址本身即读取凭据。
 * 创建: POST /api/v1/inboxes
 * 列表: GET /api/v1/inboxes/{address}/messages
 * 详情: GET /api/v1/inboxes/{address}/messages/{id}
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::{json, Value};

const BASE_URL: &str = "https://dummyinbox.com/api/v1";

/// 对路径段做最小转义，保留地址中的 @
fn encode_path(value: &str) -> String {
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
        .map_err(|e| format!("dummyinbox: 请求失败: {e}"))?;
    if !resp.status().is_success() {
        return Err(format!("dummyinbox: HTTP {}", resp.status()));
    }
    resp.json().await.map_err(|e| e.to_string())
}

/// 创建 DummyInbox 临时邮箱
pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let resp = http_client()
            .post(format!("{BASE_URL}/inboxes"))
            .header("Accept", "application/json")
            .header("User-Agent", get_current_ua())
            .send()
            .await
            .map_err(|e| format!("dummyinbox: 创建邮箱请求失败: {e}"))?;
        if !resp.status().is_success() {
            return Err(format!("dummyinbox: 创建邮箱 HTTP {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let address = data.get("address").and_then(Value::as_str).unwrap_or("");
        if address.is_empty() {
            return Err("dummyinbox: 创建响应缺少地址".into());
        }
        Ok(EmailInfo {
            channel: Channel::DummyInbox,
            email: address.to_string(),
            token: None,
            expires_at: None,
            created_at: None,
        })
    })
}

/// 获取 DummyInbox 邮件，逐条补全正文
pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let address = email.trim();
    if address.is_empty() {
        return Err("dummyinbox: 邮箱地址为空".into());
    }
    let path_addr = encode_path(address);
    block_on(async {
        let list = get_json(&format!(
            "{BASE_URL}/inboxes/{path_addr}/messages?limit=50"
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
            let detail = get_json(&format!(
                "{BASE_URL}/inboxes/{path_addr}/messages/{}",
                encode_path(id)
            ))
            .await;
            if let Ok(raw) = detail {
                if let Some(message) = raw.get("message") {
                    let to = message
                        .get("to")
                        .and_then(Value::as_array)
                        .and_then(|a| a.first())
                        .and_then(Value::as_str)
                        .unwrap_or(address);
                    out.push(normalize_email(
                        &json!({
                            "id": message.get("id").cloned().unwrap_or(Value::Null),
                            "from": message.get("from").cloned().unwrap_or(Value::Null),
                            "to": to,
                            "subject": message.get("subject").cloned().unwrap_or(Value::Null),
                            "text": message.get("text").cloned().unwrap_or(Value::Null),
                            "html": message.get("html").cloned().unwrap_or(Value::Null),
                            "date": message.get("receivedAt").cloned().unwrap_or(Value::Null),
                            "attachments": message.get("attachments").cloned().unwrap_or(json!([])),
                        }),
                        address,
                    ));
                    continue;
                }
            }
            // 详情失败时退回摘要
            out.push(normalize_email(
                &json!({
                    "id": id,
                    "from": item.get("from").cloned().unwrap_or(Value::Null),
                    "to": address,
                    "subject": item.get("subject").cloned().unwrap_or(Value::Null),
                    "text": item.get("snippet").cloned().unwrap_or(Value::Null),
                    "date": item.get("receivedAt").cloned().unwrap_or(Value::Null),
                }),
                address,
            ));
        }
        Ok(out)
    })
}
