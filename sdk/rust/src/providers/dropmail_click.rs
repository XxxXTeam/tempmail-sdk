/*!
 * dropmail.click 渠道实现
 * 网站: https://dropmail.click
 *
 * 独立自研后端，免注册、无鉴权、纯 JSON API：
 *   1. POST /api/v1/public/mailbox 创建邮箱，返回 {address, created_at, expires_at}，有效期 10 分钟
 *   2. GET /api/v1/public/mailbox/{email} 读取邮件列表
 *
 * token 即邮箱地址本身。
 */

use serde_json::Value;

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};

const BASE_URL: &str = "https://dropmail.click";

/// URL 编码邮箱地址中的 @ 符号
fn encode_mail(email: &str) -> String {
    email.replace('@', "%40")
}

/// 创建 dropmail.click 临时邮箱
/// POST /api/v1/public/mailbox → {address, created_at, expires_at}
pub fn generate_email() -> Result<EmailInfo, String> {
    let client = http_client();

    block_on(async {
        let resp = client
            .post(format!("{}/api/v1/public/mailbox", BASE_URL))
            .header("User-Agent", get_current_ua())
            .header("Accept", "application/json")
            .send()
            .await
            .map_err(|e| format!("dropmail-click: 创建邮箱请求失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!(
                "dropmail-click: 创建邮箱失败 HTTP {}",
                resp.status()
            ));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("dropmail-click: 解析创建响应失败: {}", e))?;

        let address = data
            .get("address")
            .and_then(|v| v.as_str())
            .filter(|s| !s.is_empty())
            .ok_or("dropmail-click: 无效响应，缺少 address")?
            .to_string();

        let expires_at = data
            .get("expires_at")
            .and_then(|v| v.as_str())
            .map(|s| s.to_string());
        let created_at = data
            .get("created_at")
            .and_then(|v| v.as_str())
            .map(|s| s.to_string());

        Ok(EmailInfo {
            channel: Channel::DropmailClick,
            email: address.clone(),
            token: Some(address),
            expires_at: expires_at.and_then(|s| s.parse::<i64>().ok()),
            created_at,
        })
    })
}

/// 获取 dropmail.click 邮件列表
/// GET /api/v1/public/mailbox/{email} → {messages:[{id,address,from,subject,text,html}]}
pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let addr = if token.is_empty() { email } else { token };
    if addr.is_empty() {
        return Err("dropmail-click: 缺少邮箱地址".into());
    }
    let client = http_client();

    block_on(async {
        let url = format!("{}/api/v1/public/mailbox/{}", BASE_URL, encode_mail(addr));
        let resp = client
            .get(url)
            .header("User-Agent", get_current_ua())
            .header("Accept", "application/json")
            .send()
            .await
            .map_err(|e| format!("dropmail-click: 获取邮件请求失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!(
                "dropmail-click: 获取邮件失败 HTTP {}",
                resp.status()
            ));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("dropmail-click: 解析邮件响应失败: {}", e))?;

        let list = match data.get("messages").and_then(|v| v.as_array()) {
            Some(arr) if !arr.is_empty() => arr,
            _ => return Ok(vec![]),
        };

        let mut out = Vec::with_capacity(list.len());
        for msg in list {
            let to = {
                let m = json_str(msg.get("address"));
                if m.is_empty() {
                    addr.to_string()
                } else {
                    m
                }
            };
            let date = {
                let r = json_str(msg.get("received_at"));
                if r.is_empty() {
                    json_str(msg.get("date"))
                } else {
                    r
                }
            };
            let flat = serde_json::json!({
                "id": json_str(msg.get("id")),
                "from": json_str(msg.get("from")),
                "to": to,
                "subject": json_str(msg.get("subject")),
                "text": json_str(msg.get("text")),
                "html": json_str(msg.get("html")),
                "date": date,
                "isRead": false,
            });
            out.push(normalize_email(&flat, addr));
        }
        Ok(out)
    })
}

/// 安全提取 JSON 值为字符串（字符串原样、数字转文本）
fn json_str(v: Option<&Value>) -> String {
    match v {
        Some(v) if v.is_string() => v.as_str().unwrap_or("").to_string(),
        Some(v) if v.is_number() => v.to_string(),
        _ => String::new(),
    }
}
