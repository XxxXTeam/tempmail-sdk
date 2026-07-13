/*!
 * mytempmail.cc 渠道实现
 * 网站: https://mytempmail.cc
 *
 * 流程:
 *   1. POST https://api.mytempmail.cc/api/address
 *      body: {"domain":"nilvaro.com","name":"<random>","expiry":600}
 *      返回: JSON，包含邮箱地址和 token
 *
 *   2. GET https://api.mytempmail.cc/api/mails/<token>
 *      返回: JSON 邮件列表
 *
 * token 存储: API 返回的 token 字符串
 */

use crate::config::{block_on, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use rand::Rng;
use serde_json::{json, Value};

const API_BASE: &str = "https://api.mytempmail.cc/api";
/// mytempmail.cc 默认使用的邮箱域名
const DOMAIN: &str = "nilvaro.com";
/// 邮箱默认有效期（秒）
const DEFAULT_EXPIRY: u64 = 600;

/// 生成随机用户名（小写字母 + 数字，10 位）
fn random_name() -> String {
    let charset: &[u8] = b"abcdefghijklmnopqrstuvwxyz0123456789";
    let mut rng = rand::thread_rng();
    (0..10)
        .map(|_| charset[rng.gen_range(0..charset.len())] as char)
        .collect()
}

/// 构建通用请求头
fn headers(b: wreq::RequestBuilder) -> wreq::RequestBuilder {
    b.header("Accept", "application/json")
        .header("Content-Type", "application/json")
}

/// 创建临时邮箱
/// POST /api/address，body: {"domain":"nilvaro.com","name":"<random>","expiry":600}
pub fn generate_email() -> Result<EmailInfo, String> {
    let name = random_name();
    block_on(async {
        let body = json!({
            "domain": DOMAIN,
            "name": name,
            "expiry": DEFAULT_EXPIRY,
        });

        let resp = headers(http_client().post(format!("{}/address", API_BASE)))
            .body(body.to_string())
            .send()
            .await
            .map_err(|e| format!("mytempmail-cc: 创建邮箱请求失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!(
                "mytempmail-cc: 创建邮箱返回 HTTP {}",
                resp.status()
            ));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("mytempmail-cc: 解析创建邮箱响应失败: {}", e))?;

        // 尝试从多个可能的字段名提取邮箱地址
        let email = data
            .get("address")
            .or_else(|| data.get("email"))
            .or_else(|| data.get("mail"))
            .and_then(|v| v.as_str())
            .map(|s| s.trim().to_string())
            .unwrap_or_else(|| format!("{}@{}", name, DOMAIN));

        if email.is_empty() || !email.contains('@') {
            return Err("mytempmail-cc: 返回的邮箱地址无效".into());
        }

        // 提取 token
        let token = data
            .get("token")
            .or_else(|| data.get("id"))
            .or_else(|| data.get("key"))
            .and_then(|v| v.as_str())
            .map(|s| s.to_string());

        // 计算过期时间
        let expires_at = {
            let now_ms = std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .map(|d| d.as_millis() as i64)
                .unwrap_or(0);
            Some(now_ms + (DEFAULT_EXPIRY as i64) * 1000)
        };

        Ok(EmailInfo {
            channel: Channel::MytempmailCc,
            email,
            token,
            expires_at,
            created_at: None,
        })
    })
}

/// 获取邮件列表
/// GET /api/mails/<token>
pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let em = email.trim();
    if em.is_empty() {
        return Err("mytempmail-cc: 邮箱地址为空".into());
    }

    let tk = token.trim();
    if tk.is_empty() {
        return Err("mytempmail-cc: token 为空".into());
    }

    block_on(async {
        let url = format!("{}/mails/{}", API_BASE, tk);
        let resp = headers(http_client().get(&url))
            .send()
            .await
            .map_err(|e| format!("mytempmail-cc: 获取邮件列表请求失败: {}", e))?;

        if resp.status().as_u16() == 404 {
            return Ok(Vec::new());
        }

        if !resp.status().is_success() {
            return Err(format!(
                "mytempmail-cc: 获取邮件列表返回 HTTP {}",
                resp.status()
            ));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("mytempmail-cc: 解析邮件列表响应失败: {}", e))?;

        // 响应可能是数组，也可能是包含邮件数组的对象
        let items = if let Some(arr) = data.as_array() {
            arr.clone()
        } else if let Some(arr) = data.get("mails").and_then(|v| v.as_array()) {
            arr.clone()
        } else if let Some(arr) = data.get("emails").and_then(|v| v.as_array()) {
            arr.clone()
        } else {
            return Ok(Vec::new());
        };

        let mut result = Vec::with_capacity(items.len());
        for msg in &items {
            let raw = json!({
                "id": msg.get("id")
                    .or_else(|| msg.get("_id"))
                    .unwrap_or(&Value::Null),
                "from": msg.get("from")
                    .or_else(|| msg.get("sender"))
                    .or_else(|| msg.get("from_address"))
                    .or_else(|| msg.get("mail_sender"))
                    .unwrap_or(&Value::Null),
                "to": msg.get("to")
                    .or_else(|| msg.get("recipient"))
                    .unwrap_or(&Value::String(em.to_string())),
                "subject": msg.get("subject")
                    .unwrap_or(&Value::Null),
                "text": msg.get("text")
                    .or_else(|| msg.get("body_text"))
                    .or_else(|| msg.get("body"))
                    .unwrap_or(&Value::Null),
                "html": msg.get("html")
                    .or_else(|| msg.get("body_html"))
                    .or_else(|| msg.get("htmlBody"))
                    .unwrap_or(&Value::Null),
                "date": msg.get("date")
                    .or_else(|| msg.get("received_at"))
                    .or_else(|| msg.get("created_at"))
                    .or_else(|| msg.get("timestamp"))
                    .unwrap_or(&Value::Null),
                "isRead": msg.get("is_read")
                    .or_else(|| msg.get("read"))
                    .unwrap_or(&Value::Bool(false)),
                "attachments": msg.get("attachments")
                    .unwrap_or(&Value::Array(vec![])),
            });
            result.push(normalize_email(&raw, em));
        }

        Ok(result)
    })
}
