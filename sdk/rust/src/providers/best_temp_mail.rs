/*!
 * best-temp-mail.com 渠道实现
 * 网站: https://best-temp-mail.com
 * 域名: @aabkmail.com（由 API 返回，不固定）
 *
 * 流程（纯 JSON REST API，无需认证，无需 Session）:
 *   1. POST https://best-temp-mail.com/api/v3/createEmail
 *      body: {"intToken":"<客户端生成的UUID>"}
 *      返回: {"data":{"address":"xxx@aabkmail.com","id":"...","update_tag":"..."},"status":"success","t":1}
 *
 *   2. POST https://best-temp-mail.com/api/v3/getEmailList
 *      body: {"address":"邮箱地址","id":"邮箱id","intToken":"之前的uuid","update_tag":"之前返回的update_tag"}
 *      返回: {"data":{"hasNewEmail":true/false,"emails":[...]},"status":"success","t":N}
 *
 * token 存储: JSON 字符串 {"intToken":"...","id":"...","update_tag":"..."}
 */

use crate::config::{block_on, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use rand::Rng;
use serde_json::Value;

const BASE_URL: &str = "https://best-temp-mail.com";

/// 生成 UUID v4 格式字符串
/// 格式: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
fn generate_uuid() -> String {
    let mut rng = rand::thread_rng();
    let mut bytes = [0u8; 16];
    rng.fill(&mut bytes);
    // 设置版本号为 4
    bytes[6] = (bytes[6] & 0x0f) | 0x40;
    // 设置变体位
    bytes[8] = (bytes[8] & 0x3f) | 0x80;
    format!(
        "{:02x}{:02x}{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
        bytes[0], bytes[1], bytes[2], bytes[3],
        bytes[4], bytes[5],
        bytes[6], bytes[7],
        bytes[8], bytes[9],
        bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15],
    )
}

/// 创建临时邮箱
/// 1. 生成客户端 UUID 作为 intToken
/// 2. POST /api/v3/createEmail 创建邮箱
/// 3. 将 intToken + id + update_tag 序列化为 JSON 存入 token
pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let int_token = generate_uuid();

        let body = serde_json::json!({
            "intToken": int_token,
        });

        let resp = http_client()
            .post(format!("{}/api/v3/createEmail", BASE_URL))
            .header("Content-Type", "application/json")
            .header("Accept", "application/json")
            .header("Referer", format!("{}/", BASE_URL))
            .header("Origin", BASE_URL)
            .json(&body)
            .send()
            .await
            .map_err(|e| format!("best-temp-mail: 创建邮箱请求失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!(
                "best-temp-mail: 创建邮箱返回 HTTP {}",
                resp.status()
            ));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("best-temp-mail: 解析创建邮箱响应失败: {}", e))?;

        let status = data
            .get("status")
            .and_then(|v| v.as_str())
            .unwrap_or("");
        if status != "success" {
            return Err(format!(
                "best-temp-mail: 创建邮箱失败, status={}",
                status
            ));
        }

        let info = data
            .get("data")
            .ok_or("best-temp-mail: 响应中缺少 data 字段")?;

        let address = info
            .get("address")
            .and_then(|v| v.as_str())
            .ok_or("best-temp-mail: 响应中缺少 address 字段")?;

        let email_id = info
            .get("id")
            .and_then(|v| v.as_str())
            .ok_or("best-temp-mail: 响应中缺少 id 字段")?;

        let update_tag = info
            .get("update_tag")
            .and_then(|v| v.as_str())
            .ok_or("best-temp-mail: 响应中缺少 update_tag 字段")?;

        if address.is_empty() || !address.contains('@') {
            return Err(format!(
                "best-temp-mail: 返回的邮箱地址无效: {}",
                address
            ));
        }

        // 将 intToken + id + update_tag 序列化为 JSON 存入 token
        let token = serde_json::json!({
            "intToken": int_token,
            "id": email_id,
            "update_tag": update_tag,
        })
        .to_string();

        Ok(EmailInfo {
            channel: Channel::BestTempMail,
            email: address.to_string(),
            token: Some(token),
            expires_at: None,
            created_at: None,
        })
    })
}

/// 获取邮件列表
/// 1. 从 token 中反序列化出 intToken, id, update_tag
/// 2. POST /api/v3/getEmailList 获取邮件列表
pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let em = email.trim();
    if em.is_empty() {
        return Err("best-temp-mail: 邮箱地址为空".into());
    }

    // 从 token 中解析 intToken, id, update_tag
    let token_data: Value = serde_json::from_str(token)
        .map_err(|e| format!("best-temp-mail: 解析 token 失败: {}", e))?;

    let int_token = token_data
        .get("intToken")
        .and_then(|v| v.as_str())
        .ok_or("best-temp-mail: token 中缺少 intToken")?;

    let email_id = token_data
        .get("id")
        .and_then(|v| v.as_str())
        .ok_or("best-temp-mail: token 中缺少 id")?;

    let update_tag = token_data
        .get("update_tag")
        .and_then(|v| v.as_str())
        .ok_or("best-temp-mail: token 中缺少 update_tag")?;

    block_on(async {
        let body = serde_json::json!({
            "address": em,
            "id": email_id,
            "intToken": int_token,
            "update_tag": update_tag,
        });

        let resp = http_client()
            .post(format!("{}/api/v3/getEmailList", BASE_URL))
            .header("Content-Type", "application/json")
            .header("Accept", "application/json")
            .header("Referer", format!("{}/", BASE_URL))
            .header("Origin", BASE_URL)
            .json(&body)
            .send()
            .await
            .map_err(|e| format!("best-temp-mail: 获取邮件列表请求失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!(
                "best-temp-mail: 获取邮件列表返回 HTTP {}",
                resp.status()
            ));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("best-temp-mail: 解析邮件列表响应失败: {}", e))?;

        let status = data
            .get("status")
            .and_then(|v| v.as_str())
            .unwrap_or("");
        if status != "success" {
            return Err(format!(
                "best-temp-mail: 获取邮件列表失败, status={}",
                status
            ));
        }

        let info = match data.get("data") {
            Some(d) => d,
            None => return Ok(vec![]),
        };

        // 检查是否有新邮件
        let has_new = info
            .get("hasNewEmail")
            .and_then(|v| v.as_bool())
            .unwrap_or(false);

        if !has_new {
            return Ok(vec![]);
        }

        let emails = match info.get("emails").and_then(|v| v.as_array()) {
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
                "date": item.get("date").or_else(|| item.get("created_at")).unwrap_or(&Value::Null),
                "isRead": item.get("isRead").unwrap_or(&Value::Bool(false)),
                "attachments": item.get("attachments").unwrap_or(&Value::Array(vec![])),
            });
            result.push(normalize_email(&raw, em));
        }

        Ok(result)
    })
}
