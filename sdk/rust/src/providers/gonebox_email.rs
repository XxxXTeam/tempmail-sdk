/*!
 * gonebox.email — https://gonebox.email
 * 流程：POST /api/v1/inboxes 创建邮箱（无需认证）
 * 收信：GET /api/v1/inboxes/{address}/messages
 */

use crate::config::{block_on, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::{json, Value};

const API_BASE: &str = "https://api.gonebox.email/api/v1";

/// 创建 gonebox.email 临时邮箱
pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let client = http_client();

        let resp = client
            .post(format!("{}/inboxes", API_BASE))
            .header("Content-Type", "application/json")
            .body(r#"{"domain":"gonebox.email"}"#)
            .send()
            .await
            .map_err(|e| format!("gonebox-email: 创建邮箱失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!(
                "gonebox-email: 创建邮箱失败 HTTP {}",
                resp.status()
            ));
        }

        let body: Value = resp
            .json()
            .await
            .map_err(|e| format!("gonebox-email: 解析响应失败: {}", e))?;

        let address = body["data"]["address"]
            .as_str()
            .ok_or("gonebox-email: 响应中未找到邮箱地址")?
            .to_string();

        let expires_at = body["data"]["expiresAt"].as_i64();

        Ok(EmailInfo {
            channel: Channel::GoneboxEmail,
            email: address,
            token: None,
            expires_at,
            created_at: None,
        })
    })
}

/// 获取 gonebox.email 邮件列表
pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let address = email.trim();
    if address.is_empty() {
        return Err("gonebox-email: 邮箱地址为空".into());
    }

    block_on(async {
        let client = http_client();
        let url = format!("{}/inboxes/{}/messages", API_BASE, address);

        let resp = client
            .get(&url)
            .send()
            .await
            .map_err(|e| format!("gonebox-email: 获取邮件失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!(
                "gonebox-email: 获取邮件失败 HTTP {}",
                resp.status()
            ));
        }

        let body: Value = resp
            .json()
            .await
            .map_err(|e| format!("gonebox-email: 解析响应失败: {}", e))?;

        let messages = match body["data"]["messages"].as_array() {
            Some(arr) => arr.clone(),
            None => return Ok(Vec::new()),
        };

        if messages.is_empty() {
            return Ok(Vec::new());
        }

        let mut out = Vec::with_capacity(messages.len());
        for msg in &messages {
            let raw = json!({
                "id": msg.get("id"),
                "from": msg.get("from").or_else(|| msg.get("sender")),
                "to": address,
                "subject": msg.get("subject"),
                "date": msg.get("date").or_else(|| msg.get("receivedAt")),
                "html": msg.get("html").or_else(|| msg.get("body")).and_then(|v| v.as_str()).unwrap_or(""),
                "text": msg.get("text").and_then(|v| v.as_str()).unwrap_or(""),
            });
            out.push(normalize_email(&raw, address));
        }
        Ok(out)
    })
}
