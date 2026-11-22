/*!
 * Nextmail — https://nextmail.sh
 *
 * 创建: POST /temp-mail/inboxes（返回 id、address、token）
 * 列表/详情需 Bearer 令牌；内部令牌格式为 `收件箱ID|读取令牌`。
 * 详情正文位于 message.decodedBody（HTML）。
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::{json, Value};

const BASE_URL: &str = "https://api.nextmail.sh";

fn split_token(token: &str) -> Result<(&str, &str), String> {
    let (id, bearer) = token
        .trim()
        .split_once('|')
        .ok_or_else(|| "nextmail: 内部令牌格式错误".to_string())?;
    if id.is_empty() || bearer.is_empty() {
        return Err("nextmail: 内部令牌为空".into());
    }
    Ok((id, bearer))
}

async fn api_get(url: &str, bearer: &str) -> Result<Value, String> {
    let resp = http_client()
        .get(url)
        .header("Accept", "application/json")
        .header("Authorization", format!("Bearer {bearer}"))
        .header("User-Agent", get_current_ua())
        .send()
        .await
        .map_err(|e| format!("nextmail: 请求失败: {e}"))?;
    if !resp.status().is_success() {
        return Err(format!("nextmail: HTTP {}", resp.status()));
    }
    resp.json().await.map_err(|e| e.to_string())
}

fn from_email(value: Option<&Value>) -> String {
    value
        .and_then(|v| v.get("email").or_else(|| v.as_str().map(|_| v)))
        .and_then(Value::as_str)
        .unwrap_or("")
        .to_string()
}

/// 租用 Nextmail 临时收件箱
pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let resp = http_client()
            .post(format!("{BASE_URL}/temp-mail/inboxes"))
            .header("Content-Type", "application/json")
            .header("Accept", "application/json")
            .header("User-Agent", get_current_ua())
            .body("{}")
            .send()
            .await
            .map_err(|e| format!("nextmail: 创建邮箱请求失败: {e}"))?;
        if !resp.status().is_success() {
            return Err(format!("nextmail: 创建邮箱 HTTP {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let address = data.get("address").and_then(Value::as_str).unwrap_or("");
        let id = data.get("id").and_then(Value::as_str).unwrap_or("");
        let bearer = data.get("token").and_then(Value::as_str).unwrap_or("");
        if address.is_empty() || id.is_empty() || bearer.is_empty() {
            return Err("nextmail: 创建响应不完整".into());
        }
        let expires_at = data
            .get("expiresAt")
            .and_then(Value::as_str)
            .and_then(|s| chrono::DateTime::parse_from_rfc3339(s).ok())
            .map(|d| d.timestamp_millis());
        Ok(EmailInfo {
            channel: Channel::Nextmail,
            email: address.to_string(),
            token: Some(format!("{id}|{bearer}")),
            expires_at,
            created_at: data.get("createdAt").map(|v| v.to_string()),
        })
    })
}

/// 获取 Nextmail 邮件，详情正文位于 message.decodedBody
pub fn get_emails(email: &str, token: &str) -> Result<Vec<Email>, String> {
    let (id, bearer) = split_token(token)?;
    let base = format!("{BASE_URL}/temp-mail/inboxes/{id}/messages");
    block_on(async {
        let list = api_get(&format!("{base}?limit=50"), bearer).await?;
        let rows = list
            .get("data")
            .and_then(Value::as_array)
            .cloned()
            .unwrap_or_default();
        let mut out = Vec::new();
        for item in rows {
            let message_id = item.get("id").and_then(Value::as_str).unwrap_or("");
            if message_id.is_empty() {
                continue;
            }
            if let Ok(detail) = api_get(&format!("{base}/{message_id}"), bearer).await {
                let html = detail
                    .get("message")
                    .and_then(|m| m.get("decodedBody"))
                    .cloned()
                    .unwrap_or(Value::Null);
                out.push(normalize_email(
                    &json!({
                        "id": detail.get("id").cloned().unwrap_or(Value::Null),
                        "from": from_email(detail.get("from")),
                        "to": detail.get("to").cloned().unwrap_or(json!(email)),
                        "subject": detail.get("subject").cloned().unwrap_or(Value::Null),
                        "html": html,
                        "date": detail.get("receivedAt").cloned().unwrap_or(Value::Null),
                        "seen": detail.get("seen").cloned().unwrap_or(Value::Null),
                    }),
                    email,
                ));
                continue;
            }
            out.push(normalize_email(
                &json!({
                    "id": message_id,
                    "from": from_email(item.get("from")),
                    "to": item.get("to").cloned().unwrap_or(json!(email)),
                    "subject": item.get("subject").cloned().unwrap_or(Value::Null),
                    "text": item.get("snippet").cloned().unwrap_or(Value::Null),
                    "date": item.get("receivedAt").cloned().unwrap_or(Value::Null),
                    "seen": item.get("seen").cloned().unwrap_or(Value::Null),
                }),
                email,
            ));
        }
        Ok(out)
    })
}
