/*!
 * segamail.com — 基于 session cookie 的临时邮箱
 * 创建邮箱: POST https://segamail.com/en/getEmailAddress → JSON (含 address 和 recover_key)
 * 获取邮件: POST https://segamail.com/en/getInbox (依赖 session cookie) → JSON 数组
 * 域名: segamail.com（单域名）
 * token: recover_key
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::Value;

/// 创建临时邮箱
pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let resp = http_client()
            .post("https://segamail.com/en/getEmailAddress")
            .header("Accept", "application/json, text/plain, */*")
            .header("User-Agent", get_current_ua())
            .send()
            .await
            .map_err(|e| format!("segamail: 创建邮箱请求失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("segamail: 创建邮箱失败 {}", resp.status()));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("segamail: 解析响应失败: {}", e))?;

        let address = data
            .get("address")
            .and_then(|v| v.as_str())
            .ok_or("segamail: 响应中缺少 address 字段")?;

        let recover_key = data
            .get("recover_key")
            .and_then(|v| v.as_str())
            .unwrap_or("");

        let created_at = data
            .get("creation_time")
            .and_then(|v| v.as_str())
            .map(|s| s.to_string());

        Ok(EmailInfo {
            channel: Channel::Segamail,
            email: address.to_string(),
            token: Some(recover_key.to_string()),
            expires_at: None,
            created_at,
        })
    })
}

/// 获取邮件列表
pub fn get_emails(_token: &str, email: &str) -> Result<Vec<Email>, String> {
    let em = email.trim();
    if em.is_empty() {
        return Err("segamail: 邮箱地址为空".into());
    }

    block_on(async {
        let resp = http_client()
            .post("https://segamail.com/en/getInbox")
            .header("Accept", "application/json, text/plain, */*")
            .header("User-Agent", get_current_ua())
            .send()
            .await
            .map_err(|e| format!("segamail: 获取邮件请求失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("segamail: 获取邮件失败 {}", resp.status()));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("segamail: 解析响应失败: {}", e))?;

        let rows = data.as_array().cloned().unwrap_or_default();
        let mut out = Vec::new();
        for raw in rows {
            out.push(normalize_email(&raw, em));
        }
        Ok(out)
    })
}
