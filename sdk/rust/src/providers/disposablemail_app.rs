/*!
 * disposablemail.app 渠道实现
 * 网站: https://disposablemail.app
 * 域名: @disposablemail.dev, @mailmehere.cc
 *
 * 流程（纯 REST JSON API，无需认证）:
 *   1. POST https://disposablemail.app/api/inbox
 *      body: {} (空 JSON 对象)
 *      返回: {"id":"...","address":"xxx@disposablemail.dev","token":"...","domain":"disposablemail.dev","expiresAt":"...","createdAt":"..."}
 *
 *   2. GET https://disposablemail.app/api/inbox/emails?token={token}
 *      返回: {"emails":[],"total":0,"inbox":{"address":"...","expiresAt":"..."}}
 *
 * token 存储: 直接存储 API 返回的 token 字符串
 */

use crate::config::{block_on, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::Value;

const BASE_URL: &str = "https://disposablemail.app";

/// 创建临时邮箱
/// POST /api/inbox，将返回的 token 直接存储
pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let body = serde_json::json!({});

        let resp = http_client()
            .post(format!("{}/api/inbox", BASE_URL))
            .header("Content-Type", "application/json")
            .header("Accept", "application/json")
            .header("Referer", format!("{}/", BASE_URL))
            .header("Origin", BASE_URL)
            .json(&body)
            .send()
            .await
            .map_err(|e| format!("disposablemail-app: 创建邮箱请求失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!(
                "disposablemail-app: 创建邮箱返回 HTTP {}",
                resp.status()
            ));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("disposablemail-app: 解析创建邮箱响应失败: {}", e))?;

        let address = data
            .get("address")
            .and_then(|v| v.as_str())
            .ok_or("disposablemail-app: 响应中缺少 address 字段")?;

        if address.is_empty() || !address.contains('@') {
            return Err(format!(
                "disposablemail-app: 返回的邮箱地址无效: {}",
                address
            ));
        }

        let token = data
            .get("token")
            .and_then(|v| v.as_str())
            .ok_or("disposablemail-app: 响应中缺少 token 字段")?;

        if token.is_empty() {
            return Err("disposablemail-app: 返回的 token 为空".into());
        }

        // 解析过期时间（ISO 8601 格式转毫秒时间戳）
        let expires_at = data
            .get("expiresAt")
            .and_then(|v| v.as_str())
            .and_then(|s| chrono::DateTime::parse_from_rfc3339(s).ok())
            .map(|dt| dt.timestamp_millis());

        let created_at = data
            .get("createdAt")
            .and_then(|v| v.as_str())
            .map(|s| s.to_string());

        Ok(EmailInfo {
            channel: Channel::DisposablemailApp,
            email: address.to_string(),
            token: Some(token.to_string()),
            expires_at,
            created_at,
        })
    })
}

/// 获取邮件列表
/// GET /api/inbox/emails?token={token}
pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let em = email.trim();
    if em.is_empty() {
        return Err("disposablemail-app: 邮箱地址为空".into());
    }

    block_on(async {
        let resp = http_client()
            .get(format!("{}/api/inbox/emails?token={}", BASE_URL, token))
            .header("Accept", "application/json")
            .header("Referer", format!("{}/", BASE_URL))
            .send()
            .await
            .map_err(|e| format!("disposablemail-app: 获取邮件列表请求失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!(
                "disposablemail-app: 获取邮件列表返回 HTTP {}",
                resp.status()
            ));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("disposablemail-app: 解析邮件列表响应失败: {}", e))?;

        let emails = match data.get("emails").and_then(|v| v.as_array()) {
            Some(arr) => arr,
            None => return Ok(vec![]),
        };

        let mut result = Vec::with_capacity(emails.len());
        for item in emails {
            let raw = serde_json::json!({
                "id": item.get("id").unwrap_or(&Value::Null),
                "from": item.get("from").or_else(|| item.get("from_address")).or_else(|| item.get("sender")).unwrap_or(&Value::Null),
                "to": em,
                "subject": item.get("subject").unwrap_or(&Value::Null),
                "text": item.get("text").or_else(|| item.get("body")).unwrap_or(&Value::Null),
                "html": item.get("html").or_else(|| item.get("html_body")).unwrap_or(&Value::Null),
                "date": item.get("date").or_else(|| item.get("receivedAt")).or_else(|| item.get("created_at")).unwrap_or(&Value::Null),
                "isRead": item.get("isRead").unwrap_or(&Value::Bool(false)),
                "attachments": item.get("attachments").unwrap_or(&Value::Array(vec![])),
            });
            result.push(normalize_email(&raw, em));
        }

        Ok(result)
    })
}
