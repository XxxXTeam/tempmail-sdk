/*!
 * Zerodrop 渠道实现（zerodrop.dev）
 *
 * 无认证 REST：
 *   建箱: 无需请求，本地生成 "sdk"+8 位随机名，地址为 <名>@zerodrop-sandbox.online
 *   读信: GET /api/inbox/{name}?source=sdk，响应 {"emails":[...],"count":N}
 * 平台邮件对象只有 id/from/to/subject/receivedAt/raw/otp/magicLink：
 * 正文仅存在于 raw（完整 MIME 原文），须剥离头部（首个空行之后）作 text，
 * html 留空由归一化层兜底合成。
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use rand::RngExt;
use serde_json::{json, Value};

const BASE_URL: &str = "https://zerodrop.dev";
const DOMAIN: &str = "zerodrop-sandbox.online";

/// 生成 "sdk"+8 位随机本地名
fn random_local() -> String {
    const CHARS: &[u8] = b"abcdefghijklmnopqrstuvwxyz0123456789";
    let mut rng = rand::rng();
    let suffix: String = (0..8)
        .map(|_| CHARS[rng.random_range(0..CHARS.len())] as char)
        .collect();
    format!("sdk{suffix}")
}

/// 从 raw（完整 MIME 原文）提取纯文本正文：
/// 定位首个空行（RFC 5322 头部/正文分隔，\r\n\r\n 或 \n\n），其后部分即 body
fn raw_body(raw: &str) -> String {
    if let Some(idx) = raw.find("\r\n\r\n") {
        return raw[idx + 4..].to_string();
    }
    if let Some(idx) = raw.find("\n\n") {
        return raw[idx + 2..].to_string();
    }
    String::new()
}

/// 创建临时邮箱
/// 建箱无需请求，本地生成随机名，token 复用完整地址以便收件箱回查
pub fn generate_email() -> Result<EmailInfo, String> {
    let email = format!("{}@{DOMAIN}", random_local());
    Ok(EmailInfo {
        channel: Channel::Zerodrop,
        email: email.clone(),
        token: Some(email),
        expires_at: None,
        created_at: None,
    })
}

/// 读取收件箱
/// GET /api/inbox/{name}?source=sdk，原始字段交归一化层处理
pub fn get_emails(email: &str, _token: &str) -> Result<Vec<Email>, String> {
    let em = email.trim();
    let (name, domain) = em
        .rsplit_once('@')
        .ok_or("zerodrop 读信: 邮箱地址格式无效")?;
    if domain != DOMAIN {
        return Err(format!("zerodrop 读信: 非 {DOMAIN} 域邮箱地址"));
    }

    let url = format!(
        "{BASE_URL}/api/inbox/{}?source=sdk",
        urlencoding::encode(name)
    );

    block_on(async {
        let resp = http_client()
            .get(&url)
            .header("User-Agent", get_current_ua())
            .header("Accept", "application/json")
            .send()
            .await
            .map_err(|e| format!("zerodrop: 获取收件箱失败: {e}"))?;

        if !resp.status().is_success() {
            return Err(format!("zerodrop 读信: http {}", resp.status()));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("zerodrop: 解析收件箱响应失败: {e}"))?;

        let emails_row = data
            .get("emails")
            .and_then(|v| v.as_array())
            .cloned()
            .unwrap_or_default();

        let mut out = Vec::with_capacity(emails_row.len());
        for m in &emails_row {
            let mut flat = m.clone();
            /* 平台响应无 to 字段，收件人固定为当前邮箱 */
            if let Some(obj) = flat.as_object_mut() {
                obj.insert("to".to_string(), json!(em));
                /* 正文仅存在于 raw（完整 MIME 原文）：提取纯文本 body 作 text */
                if let Some(raw) = m.get("raw").and_then(|v| v.as_str()) {
                    let body = raw_body(raw);
                    if !body.is_empty() {
                        obj.insert("text".to_string(), json!(body));
                    }
                }
                /* date 用 receivedAt（归一化层已支持该候选键） */
                if let Some(date) = m.get("receivedAt").and_then(|v| v.as_str()) {
                    obj.entry("date".to_string()).or_insert_with(|| json!(date));
                }
            }
            out.push(normalize_email(&flat, em));
        }
        Ok(out)
    })
}
