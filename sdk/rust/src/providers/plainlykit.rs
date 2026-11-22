/*!
 * PlainlyKit — https://plainlykit.com
 *
 * 创建: POST /tools/temporary-email/api/mailboxes（返回 id、address、token）
 * 列表/详情需 Bearer 令牌；内部令牌格式为 `邮箱ID|读取令牌`。
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::{json, Value};

const BASE_URL: &str = "https://plainlykit.com/tools/temporary-email/api";

fn split_token(token: &str) -> Result<(&str, &str), String> {
    let (id, bearer) = token
        .trim()
        .split_once('|')
        .ok_or_else(|| "plainlykit: 内部令牌格式错误".to_string())?;
    if id.is_empty() || bearer.is_empty() {
        return Err("plainlykit: 内部令牌为空".into());
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
        .map_err(|e| format!("plainlykit: 请求失败: {e}"))?;
    if !resp.status().is_success() {
        return Err(format!("plainlykit: HTTP {}", resp.status()));
    }
    resp.json().await.map_err(|e| e.to_string())
}

/// 创建 PlainlyKit 临时邮箱
pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let resp = http_client()
            .post(format!("{BASE_URL}/mailboxes"))
            .header("Content-Type", "application/json")
            .header("Accept", "application/json")
            .header("User-Agent", get_current_ua())
            .body("{}")
            .send()
            .await
            .map_err(|e| format!("plainlykit: 创建邮箱请求失败: {e}"))?;
        if !resp.status().is_success() {
            return Err(format!("plainlykit: 创建邮箱 HTTP {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let address = data.get("address").and_then(Value::as_str).unwrap_or("");
        let id = data.get("id").and_then(Value::as_str).unwrap_or("");
        let bearer = data.get("token").and_then(Value::as_str).unwrap_or("");
        if address.is_empty() || id.is_empty() || bearer.is_empty() {
            return Err("plainlykit: 创建响应不完整".into());
        }
        Ok(EmailInfo {
            channel: Channel::PlainlyKit,
            email: address.to_string(),
            token: Some(format!("{id}|{bearer}")),
            expires_at: data.get("expiresAt").and_then(Value::as_i64),
            created_at: data.get("createdAt").map(|v| v.to_string()),
        })
    })
}

/// 获取 PlainlyKit 邮件，详情提供纯文本正文
pub fn get_emails(email: &str, token: &str) -> Result<Vec<Email>, String> {
    let (id, bearer) = split_token(token)?;
    let base = format!("{BASE_URL}/mailboxes/{id}/messages");
    block_on(async {
        let list = api_get(&base, bearer).await?;
        let rows = list
            .get("messages")
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
                let raw = detail.get("message").unwrap_or(&detail);
                let mut from = raw.get("sender").cloned().unwrap_or(Value::Null);
                if from.as_str().unwrap_or("").is_empty() {
                    from = item.get("sender").cloned().unwrap_or(Value::Null);
                }
                let mut subject = raw.get("subject").cloned().unwrap_or(Value::Null);
                if subject.as_str().unwrap_or("").is_empty() {
                    subject = item.get("subject").cloned().unwrap_or(Value::Null);
                }
                out.push(normalize_email(
                    &json!({
                        "id": raw.get("id").cloned().unwrap_or(Value::Null),
                        "from": from,
                        "to": email,
                        "subject": subject,
                        "text": raw.get("text").cloned().unwrap_or(Value::Null),
                        "html": raw.get("html").cloned().unwrap_or(Value::Null),
                        "date": raw.get("receivedAt").cloned().unwrap_or(Value::Null),
                    }),
                    email,
                ));
                continue;
            }
            out.push(normalize_email(
                &json!({
                    "id": message_id,
                    "from": item.get("sender").cloned().unwrap_or(Value::Null),
                    "to": email,
                    "subject": item.get("subject").cloned().unwrap_or(Value::Null),
                    "text": item.get("preview").cloned().unwrap_or(Value::Null),
                    "date": item.get("receivedAt").cloned().unwrap_or(Value::Null),
                }),
                email,
            ));
        }
        Ok(out)
    })
}
