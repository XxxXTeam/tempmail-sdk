/*!
 * mailcat.ai — https://mailcat.ai
 * 流程：POST /mailboxes 创建邮箱（返回 Bearer token）
 * 收信：GET /inbox 带 Authorization: Bearer {token}
 */

use crate::config::{block_on, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::{json, Value};

const API_BASE: &str = "https://api.mailcat.ai";

/// 创建 mailcat.ai 临时邮箱
pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let client = http_client();

        let resp = client
            .post(format!("{}/mailboxes", API_BASE))
            .send()
            .await
            .map_err(|e| format!("mailcat-ai: 创建邮箱失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!(
                "mailcat-ai: 创建邮箱失败 HTTP {}",
                resp.status()
            ));
        }

        let body: Value = resp
            .json()
            .await
            .map_err(|e| format!("mailcat-ai: 解析响应失败: {}", e))?;

        let email = body["data"]["email"]
            .as_str()
            .ok_or("mailcat-ai: 响应中未找到邮箱地址")?
            .to_string();

        let token = body["data"]["token"]
            .as_str()
            .ok_or("mailcat-ai: 响应中未找到 token")?
            .to_string();

        Ok(EmailInfo {
            channel: Channel::MailcatAi,
            email,
            token: Some(token),
            expires_at: None,
            created_at: None,
        })
    })
}

/// 获取 mailcat.ai 邮件列表
pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let address = email.trim();
    if address.is_empty() {
        return Err("mailcat-ai: 邮箱地址为空".into());
    }

    block_on(async {
        let client = http_client();

        let resp = client
            .get(format!("{}/inbox", API_BASE))
            .header("Authorization", format!("Bearer {}", token))
            .send()
            .await
            .map_err(|e| format!("mailcat-ai: 获取邮件失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!(
                "mailcat-ai: 获取邮件失败 HTTP {}",
                resp.status()
            ));
        }

        let body: Value = resp
            .json()
            .await
            .map_err(|e| format!("mailcat-ai: 解析响应失败: {}", e))?;

        let messages = match body["data"].as_array() {
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
