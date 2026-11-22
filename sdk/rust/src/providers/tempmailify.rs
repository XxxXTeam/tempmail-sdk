/*!
 * Tempmailify — https://tempmailify.com
 *
 * 创建: POST /api/inbox（返回 address、token）
 * 列表/详情需 Bearer 令牌；详情提供完整 text/html。
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::{json, Value};

const BASE_URL: &str = "https://tempmailify.com";

fn from_address(value: Option<&Value>) -> String {
    match value {
        Some(Value::Object(obj)) => obj
            .get("address")
            .and_then(Value::as_str)
            .unwrap_or("")
            .to_string(),
        Some(Value::String(s)) => s.clone(),
        _ => String::new(),
    }
}

async fn api_get(url: &str, bearer: &str) -> Result<Value, String> {
    let resp = http_client()
        .get(url)
        .header("Accept", "application/json")
        .header("Authorization", format!("Bearer {bearer}"))
        .header("User-Agent", get_current_ua())
        .send()
        .await
        .map_err(|e| format!("tempmailify: 请求失败: {e}"))?;
    if !resp.status().is_success() {
        return Err(format!("tempmailify: HTTP {}", resp.status()));
    }
    resp.json().await.map_err(|e| e.to_string())
}

/// 创建 Tempmailify 临时邮箱
pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let resp = http_client()
            .post(format!("{BASE_URL}/api/inbox"))
            .header("Content-Type", "application/json")
            .header("Accept", "application/json")
            .header("User-Agent", get_current_ua())
            .body("{}")
            .send()
            .await
            .map_err(|e| format!("tempmailify: 创建邮箱请求失败: {e}"))?;
        if !resp.status().is_success() {
            return Err(format!("tempmailify: 创建邮箱 HTTP {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let address = data.get("address").and_then(Value::as_str).unwrap_or("");
        let token = data.get("token").and_then(Value::as_str).unwrap_or("");
        if address.is_empty() || token.is_empty() {
            return Err("tempmailify: 创建响应不完整".into());
        }
        Ok(EmailInfo {
            channel: Channel::Tempmailify,
            email: address.to_string(),
            token: Some(token.to_string()),
            expires_at: data.get("expiresAt").and_then(Value::as_i64),
            created_at: data.get("createdAt").map(|v| v.to_string()),
        })
    })
}

/// 获取 Tempmailify 邮件，详情提供完整 text/html
pub fn get_emails(email: &str, token: &str) -> Result<Vec<Email>, String> {
    let mut out = Vec::new();
    let base = format!(
        "{BASE_URL}/api/inbox/{}",
        urlencoding_encode(email.trim())
    );
    block_on(async {
        let list = api_get(&base, token).await?;
        let rows = list
            .get("messages")
            .and_then(Value::as_array)
            .cloned()
            .unwrap_or_default();
        for item in rows {
            let message_id = item.get("id").and_then(Value::as_str).unwrap_or("");
            if message_id.is_empty() {
                continue;
            }
            if let Ok(detail) =
                api_get(&format!("{base}/messages/{message_id}"), token).await
            {
                out.push(normalize_email(
                    &json!({
                        "id": detail.get("id").cloned().unwrap_or(Value::Null),
                        "from": from_address(detail.get("from")),
                        "to": detail.get("to").cloned().unwrap_or(json!(email)),
                        "subject": detail.get("subject").cloned().unwrap_or(Value::Null),
                        "text": detail.get("text").cloned().unwrap_or(Value::Null),
                        "html": detail.get("html").cloned().unwrap_or(Value::Null),
                        "date": detail.get("receivedAt").cloned().unwrap_or(Value::Null),
                        "seen": detail.get("seen").cloned().unwrap_or(Value::Null),
                        "attachments": detail.get("attachments").cloned().unwrap_or(json!([])),
                    }),
                    email,
                ));
                continue;
            }
            out.push(normalize_email(
                &json!({
                    "id": message_id,
                    "from": from_address(item.get("from")),
                    "to": email,
                    "subject": item.get("subject").cloned().unwrap_or(Value::Null),
                    "text": item.get("preview").cloned().unwrap_or(Value::Null),
                    "date": item.get("receivedAt").cloned().unwrap_or(Value::Null),
                    "seen": item.get("seen").cloned().unwrap_or(Value::Null),
                }),
                email,
            ));
        }
        Ok(out)
    })
}

/// 对路径段做最小转义
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
