/*!
 * tempgo-email — https://tempgo.email
 * 流程：POST /api/generate 创建邮箱（无 body，不发送 Content-Type）
 * 收信：GET /api/inbox?email={email}&mailbox_id={token}
 */

use crate::config::{block_on, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::{json, Value};

const API_BASE: &str = "https://tempgo.email";

/// 创建 tempgo.email 临时邮箱
pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let client = http_client();

        let resp = client
            .post(format!("{}/api/generate", API_BASE))
            .send()
            .await
            .map_err(|e| format!("tempgo-email: 创建邮箱失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!(
                "tempgo-email: 创建邮箱失败 HTTP {}",
                resp.status()
            ));
        }

        let body: Value = resp
            .json()
            .await
            .map_err(|e| format!("tempgo-email: 解析响应失败: {}", e))?;

        let email = body["email"]
            .as_str()
            .ok_or("tempgo-email: 响应中未找到邮箱地址")?
            .to_string();

        let mailbox_id = body["mailbox_id"]
            .as_str()
            .ok_or("tempgo-email: 响应中未找到 mailbox_id")?
            .to_string();

        Ok(EmailInfo {
            channel: Channel::TempgoEmail,
            email,
            token: Some(mailbox_id),
            expires_at: None,
            created_at: None,
        })
    })
}

/// 获取 tempgo.email 邮件列表
pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let address = email.trim();
    if address.is_empty() {
        return Err("tempgo-email: 邮箱地址为空".into());
    }

    block_on(async {
        let client = http_client();

        let resp = client
            .get(format!(
                "{}/api/inbox?email={}&mailbox_id={}",
                API_BASE, address, token
            ))
            .send()
            .await
            .map_err(|e| format!("tempgo-email: 获取邮件失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!(
                "tempgo-email: 获取邮件失败 HTTP {}",
                resp.status()
            ));
        }

        let body: Value = resp
            .json()
            .await
            .map_err(|e| format!("tempgo-email: 解析响应失败: {}", e))?;

        let messages = match body["messages"].as_array() {
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
                "date": msg.get("date").or_else(|| msg.get("received_at")),
                "html": msg.get("html").or_else(|| msg.get("body_html")).and_then(|v| v.as_str()).unwrap_or(""),
                "text": msg.get("text").or_else(|| msg.get("body_plain")).and_then(|v| v.as_str()).unwrap_or(""),
            });
            out.push(normalize_email(&raw, address));
        }
        Ok(out)
    })
}
