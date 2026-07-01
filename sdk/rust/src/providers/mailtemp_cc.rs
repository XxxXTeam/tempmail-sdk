/*!
 * mailtemp.cc 渠道实现
 * 网站: https://mailtemp.cc
 * 域名: @neocea.com
 *
 * 流程（PHP POST form-urlencoded API）:
 *   1. POST https://mailtemp.cc/api.php
 *      body: action=inbox
 *      返回: 纯文本/JSON 字符串（带引号），如 "vindictiverate"（仅用户名部分）
 *      完整邮箱: {username}@neocea.com
 *
 *   2. POST https://mailtemp.cc/api.php
 *      body: action=fetch&inbox={username}&last_id=0
 *      返回: JSON 数组 [{id, subject, sender, sender_email, received_at, ...}]
 *
 *   3. POST https://mailtemp.cc/api.php
 *      body: action=view&id={id}&inbox={username}
 *      返回: JSON {id, subject, sender, sender_email, received_at, body_html, advertisement}
 *
 * token 存储: 存储 username（@前面的部分）
 * 注意: 创建收件箱返回的用户名是 JSON 字符串格式（带引号），需要去掉引号
 */

use crate::config::{block_on, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::Value;

const BASE_URL: &str = "https://mailtemp.cc";
/// mailtemp.cc 固定使用的邮箱域名
const DOMAIN: &str = "neocea.com";

/// 创建临时邮箱
/// POST api.php, body: action=inbox
/// 返回的用户名是 JSON 字符串格式（带引号），需要 trim 后去掉首尾引号
pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let resp = http_client()
            .post(format!("{}/api.php", BASE_URL))
            .header("Content-Type", "application/x-www-form-urlencoded")
            .header("Accept", "*/*")
            .header("Referer", format!("{}/", BASE_URL))
            .header("Origin", BASE_URL)
            .body("action=inbox")
            .send()
            .await
            .map_err(|e| format!("mailtemp-cc: 创建邮箱请求失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!(
                "mailtemp-cc: 创建邮箱返回 HTTP {}",
                resp.status()
            ));
        }

        let raw_text = resp
            .text()
            .await
            .map_err(|e| format!("mailtemp-cc: 读取创建邮箱响应失败: {}", e))?;

        // API 返回的是 JSON 字符串格式（带双引号），如 "vindictiverate"
        // 需要去掉首尾的双引号和空白字符
        let username = raw_text.trim().trim_matches('"');

        if username.is_empty() {
            return Err("mailtemp-cc: 返回的用户名为空".into());
        }

        let address = format!("{}@{}", username, DOMAIN);

        Ok(EmailInfo {
            channel: Channel::MailtempCc,
            email: address,
            token: Some(username.to_string()),
            expires_at: None,
            created_at: None,
        })
    })
}

/// 获取邮件列表
/// 1. POST api.php, body: action=fetch&inbox={token}&last_id=0 获取邮件摘要列表
/// 2. 对每封邮件 POST api.php, body: action=view&id={id}&inbox={token} 获取完整内容
pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let em = email.trim();
    if em.is_empty() {
        return Err("mailtemp-cc: 邮箱地址为空".into());
    }

    let username = token.trim();
    if username.is_empty() {
        return Err("mailtemp-cc: token（用户名）为空".into());
    }

    block_on(async {
        // 第一步: 获取邮件摘要列表
        let fetch_body = format!("action=fetch&inbox={}&last_id=0", username);

        let resp = http_client()
            .post(format!("{}/api.php", BASE_URL))
            .header("Content-Type", "application/x-www-form-urlencoded")
            .header("Accept", "*/*")
            .header("Referer", format!("{}/", BASE_URL))
            .header("Origin", BASE_URL)
            .body(fetch_body)
            .send()
            .await
            .map_err(|e| format!("mailtemp-cc: 获取邮件列表请求失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!(
                "mailtemp-cc: 获取邮件列表返回 HTTP {}",
                resp.status()
            ));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("mailtemp-cc: 解析邮件列表响应失败: {}", e))?;

        // 返回值是 JSON 数组
        let items = match data.as_array() {
            Some(arr) => arr,
            None => return Ok(vec![]),
        };

        if items.is_empty() {
            return Ok(vec![]);
        }

        // 第二步: 逐封获取邮件详情
        let mut result = Vec::with_capacity(items.len());
        for item in items {
            let mail_id = match item.get("id") {
                Some(v) => {
                    // id 可能是数字或字符串
                    if let Some(s) = v.as_str() {
                        s.to_string()
                    } else if let Some(n) = v.as_i64() {
                        n.to_string()
                    } else if let Some(n) = v.as_u64() {
                        n.to_string()
                    } else {
                        continue;
                    }
                }
                None => continue,
            };

            let view_body = format!("action=view&id={}&inbox={}", mail_id, username);

            let detail_resp = http_client()
                .post(format!("{}/api.php", BASE_URL))
                .header("Content-Type", "application/x-www-form-urlencoded")
                .header("Accept", "*/*")
                .header("Referer", format!("{}/", BASE_URL))
                .header("Origin", BASE_URL)
                .body(view_body)
                .send()
                .await;

            let detail = match detail_resp {
                Ok(r) if r.status().is_success() => {
                    r.json::<Value>().await.unwrap_or(Value::Null)
                }
                _ => Value::Null,
            };

            // 合并摘要和详情数据，详情优先
            let raw = serde_json::json!({
                "id": mail_id,
                "from": detail.get("sender_email")
                    .or_else(|| item.get("sender_email"))
                    .or_else(|| detail.get("sender"))
                    .or_else(|| item.get("sender"))
                    .unwrap_or(&Value::Null),
                "to": em,
                "subject": detail.get("subject")
                    .or_else(|| item.get("subject"))
                    .unwrap_or(&Value::Null),
                "html": detail.get("body_html").unwrap_or(&Value::Null),
                "text": Value::Null,
                "date": detail.get("received_at")
                    .or_else(|| item.get("received_at"))
                    .unwrap_or(&Value::Null),
                "isRead": false,
                "attachments": [],
            });
            result.push(normalize_email(&raw, em));
        }

        Ok(result)
    })
}
