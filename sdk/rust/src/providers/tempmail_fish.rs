/*!
 * TempMail Fish — https://tempmail.fish
 * 生成邮箱返回 email + authKey，token 存为 JSON: {"email":"...","authKey":"..."}
 * 获取邮件需在请求头 Authorization 直接携带 authKey（无 Bearer 前缀）
 */

use crate::config::{block_on, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::{json, Value};

const API_BASE: &str = "https://api.tempmail.fish";

/// 将 tempmail.fish 原始邮件映射为标准化中间格式
fn flatten_message(raw: &Value, recipient: &str) -> Value {
    /* textBody 中常内嵌 HTML，交由 normalize_email 自动识别归位 */
    let body = raw
        .get("textBody")
        .and_then(Value::as_str)
        .unwrap_or("")
        .to_string();
    let to = raw
        .get("to")
        .and_then(Value::as_str)
        .filter(|s| !s.is_empty())
        .unwrap_or(recipient);
    /* timestamp 为毫秒数，转为 ISO 字符串 */
    let date = raw
        .get("timestamp")
        .and_then(Value::as_i64)
        .map(|ms| ms.to_string())
        .unwrap_or_default();
    json!({
        "from": raw.get("from").and_then(Value::as_str).unwrap_or(""),
        "to": to,
        "subject": raw.get("subject").and_then(Value::as_str).unwrap_or(""),
        "text": body,
        "html": raw.get("htmlBody").and_then(Value::as_str).unwrap_or(""),
        "timestamp": raw.get("timestamp").cloned().unwrap_or(Value::Null),
        "date": date,
        "attachments": raw.get("attachments").cloned().unwrap_or(Value::Array(vec![])),
    })
}

/// 创建 tempmail.fish 临时邮箱
/// API: POST /emails/new-email → {"email":"...","authKey":"...","emails":[]}
pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let resp = http_client()
            .post(format!("{}/emails/new-email", API_BASE))
            .header("Content-Type", "application/json")
            .header("Accept", "application/json")
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("tempmail-fish: 创建邮箱失败 {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let email = data
            .get("email")
            .and_then(Value::as_str)
            .unwrap_or("")
            .trim()
            .to_string();
        let auth_key = data
            .get("authKey")
            .and_then(Value::as_str)
            .unwrap_or("")
            .trim()
            .to_string();
        if email.is_empty() || !email.contains('@') || auth_key.is_empty() {
            return Err("tempmail-fish: 创建邮箱响应无效".into());
        }
        let token = json!({ "email": email, "authKey": auth_key }).to_string();
        Ok(EmailInfo {
            channel: Channel::TempmailFish,
            email,
            token: Some(token),
            expires_at: None,
            created_at: None,
        })
    })
}

/// 获取 tempmail.fish 邮件列表
/// API: GET /emails/emails?emailAddress={email}，请求头 Authorization: {authKey}
pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    if token.trim().is_empty() {
        return Err("tempmail-fish: token 不能为空".into());
    }
    /* token 为 JSON 字符串，解析出 email 与 authKey */
    let parsed: Value =
        serde_json::from_str(token).map_err(|_| "tempmail-fish: token 格式无效".to_string())?;
    let address = parsed
        .get("email")
        .and_then(Value::as_str)
        .unwrap_or(email)
        .trim()
        .to_string();
    let auth_key = parsed
        .get("authKey")
        .and_then(Value::as_str)
        .unwrap_or("")
        .trim()
        .to_string();
    if address.is_empty() || auth_key.is_empty() {
        return Err("tempmail-fish: token 缺少 email 或 authKey".into());
    }

    block_on(async {
        let url = format!(
            "{}/emails/emails?emailAddress={}",
            API_BASE,
            urlencoding::encode(&address)
        );
        let resp = http_client()
            .get(url)
            .header("Accept", "application/json")
            .header("Authorization", &auth_key)
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("tempmail-fish: 获取邮件失败 {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        /* 响应通常是邮件数组，兼容 {"emails":[...]} 包裹形式 */
        let rows = if let Some(arr) = data.as_array() {
            arr.clone()
        } else if let Some(arr) = data.get("emails").and_then(Value::as_array) {
            arr.clone()
        } else {
            vec![]
        };
        let out = rows
            .iter()
            .map(|row| normalize_email(&flatten_message(row, &address), &address))
            .collect();
        Ok(out)
    })
}
