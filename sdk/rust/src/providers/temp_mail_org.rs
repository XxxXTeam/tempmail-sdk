/*!
 * temp-mail.org 渠道实现
 * 网站: https://temp-mail.org
 * API 基础地址: https://web2.temp-mail.org
 * 创建邮箱返回 JWT token + mailbox 地址
 * 获取邮件需在 Authorization: Bearer {token} 中携带 JWT
 */

use crate::config::{block_on, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::{json, Value};

const BASE_URL: &str = "https://web2.temp-mail.org";

/// 公共请求头：模拟浏览器访问
fn common_headers(
    builder: wreq::RequestBuilder,
) -> wreq::RequestBuilder {
    builder
        .header("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36")
        .header("Origin", "https://temp-mail.org")
        .header("Referer", "https://temp-mail.org/")
}

/// 将邮件详情 API 返回的 JSON 映射为标准化中间格式
fn flatten_message(detail: &Value, recipient: &str) -> Value {
    let date = detail
        .get("receivedAt")
        .and_then(Value::as_i64)
        .map(|ts| ts.to_string())
        .or_else(|| {
            detail
                .get("createdAt")
                .and_then(Value::as_str)
                .map(|s| s.to_string())
        })
        .unwrap_or_default();

    json!({
        "from": detail.get("from").and_then(Value::as_str).unwrap_or(""),
        "to": recipient,
        "subject": detail.get("subject").and_then(Value::as_str).unwrap_or(""),
        "text": detail.get("bodyPreview").and_then(Value::as_str).unwrap_or(""),
        "html": detail.get("bodyHtml").and_then(Value::as_str).unwrap_or(""),
        "date": date,
        "attachments": detail.get("attachments").cloned().unwrap_or(Value::Array(vec![])),
    })
}

/// 创建 temp-mail.org 临时邮箱
/// API: POST https://web2.temp-mail.org/mailbox → {"token":"...","mailbox":"..."}
pub fn generate_email(_duration: u32, _domain: Option<&str>) -> Result<EmailInfo, String> {
    block_on(async {
        let resp = common_headers(http_client().post(format!("{}/mailbox", BASE_URL)))
            .send()
            .await
            .map_err(|e| format!("temp-mail-org: 创建邮箱请求失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!(
                "temp-mail-org: 创建邮箱失败, HTTP {}",
                resp.status()
            ));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("temp-mail-org: 解析响应失败: {}", e))?;

        let token = data
            .get("token")
            .and_then(Value::as_str)
            .unwrap_or("")
            .trim()
            .to_string();
        let mailbox = data
            .get("mailbox")
            .and_then(Value::as_str)
            .unwrap_or("")
            .trim()
            .to_string();

        if token.is_empty() || mailbox.is_empty() || !mailbox.contains('@') {
            return Err("temp-mail-org: 创建邮箱响应无效".into());
        }

        Ok(EmailInfo {
            channel: Channel::TempMailOrg,
            email: mailbox,
            token: Some(token),
            expires_at: None,
            created_at: None,
        })
    })
}

/// 获取 temp-mail.org 邮件列表
/// 流程: GET /messages 获取邮件摘要列表 → 逐封 GET /messages/{id} 获取完整内容
pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    if token.trim().is_empty() {
        return Err("temp-mail-org: token 不能为空".into());
    }

    block_on(async {
        // 获取邮件列表
        let resp = common_headers(http_client().get(format!("{}/messages", BASE_URL)))
            .header("Authorization", format!("Bearer {}", token))
            .send()
            .await
            .map_err(|e| format!("temp-mail-org: 获取邮件列表失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!(
                "temp-mail-org: 获取邮件列表失败, HTTP {}",
                resp.status()
            ));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("temp-mail-org: 解析邮件列表失败: {}", e))?;

        let messages = data
            .get("messages")
            .and_then(Value::as_array)
            .cloned()
            .unwrap_or_default();

        if messages.is_empty() {
            return Ok(vec![]);
        }

        // 逐封获取邮件详情
        let mut result: Vec<Email> = Vec::new();
        for msg in &messages {
            let msg_id = match msg.get("_id").and_then(Value::as_str) {
                Some(id) if !id.is_empty() => id,
                _ => continue,
            };

            let detail_resp =
                common_headers(http_client().get(format!("{}/messages/{}", BASE_URL, msg_id)))
                    .header("Authorization", format!("Bearer {}", token))
                    .send()
                    .await;

            let detail = match detail_resp {
                Ok(r) if r.status().is_success() => {
                    r.json::<Value>().await.unwrap_or(Value::Null)
                }
                _ => {
                    // 详情获取失败时回退到摘要数据
                    msg.clone()
                }
            };

            let raw = flatten_message(&detail, email);
            result.push(normalize_email(&raw, email));
        }

        Ok(result)
    })
}
