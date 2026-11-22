/*!
 * temp.tf — https://temp.tf
 *
 * 创建: GET /api/account?plus=1 → {email}。
 * 收件: POST /api/check {email} → {data:[{subject,from,date,body,bodyContentType,id,attachments}]}。
 * body 按 bodyContentType 归位为 text 或 html。
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::{json, Value};

const BASE_URL: &str = "https://temp.tf/api";

/// 获取一个一次性地址
pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let resp = http_client()
            .get(format!("{BASE_URL}/account?plus=1"))
            .header("Accept", "application/json")
            .header("User-Agent", get_current_ua())
            .send()
            .await
            .map_err(|e| format!("temp-tf: 创建邮箱请求失败: {e}"))?;
        if !resp.status().is_success() {
            return Err(format!("temp-tf: 创建邮箱 HTTP {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let email = data.get("email").and_then(Value::as_str).unwrap_or("");
        if email.is_empty() {
            return Err("temp-tf: 创建响应缺少地址".into());
        }
        Ok(EmailInfo {
            channel: Channel::TempTF,
            email: email.to_string(),
            token: None,
            expires_at: None,
            created_at: None,
        })
    })
}

/// 获取 temp.tf 邮件，body 按 bodyContentType 归位
pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let address = email.trim();
    if address.is_empty() {
        return Err("temp-tf: 邮箱地址为空".into());
    }
    block_on(async {
        let resp = http_client()
            .post(format!("{BASE_URL}/check"))
            .header("Content-Type", "application/json")
            .header("Accept", "application/json")
            .header("User-Agent", get_current_ua())
            .body(format!(r#"{{"email":"{address}"}}"#))
            .send()
            .await
            .map_err(|e| format!("temp-tf: 获取邮件请求失败: {e}"))?;
        if !resp.status().is_success() {
            return Err(format!("temp-tf: HTTP {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let rows = data
            .get("data")
            .and_then(Value::as_array)
            .cloned()
            .unwrap_or_default();
        Ok(rows
            .iter()
            .map(|item| {
                let is_text = item
                    .get("bodyContentType")
                    .and_then(Value::as_str)
                    .map(|s| s.eq_ignore_ascii_case("text"))
                    .unwrap_or(false);
                let body = item.get("body").cloned().unwrap_or(Value::Null);
                let (text, html) = if is_text {
                    (body, Value::Null)
                } else {
                    (Value::Null, body)
                };
                normalize_email(
                    &json!({
                        "id": item.get("id").cloned().unwrap_or(Value::Null),
                        "from": item.get("from").cloned().unwrap_or(Value::Null),
                        "to": address,
                        "subject": item.get("subject").cloned().unwrap_or(Value::Null),
                        "text": text,
                        "html": html,
                        "date": item.get("date").cloned().unwrap_or(Value::Null),
                        "attachments": item.get("attachments").cloned().unwrap_or(json!([])),
                    }),
                    address,
                )
            })
            .collect())
    })
}
