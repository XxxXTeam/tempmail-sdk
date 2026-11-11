/*!
 * LinshiXYZ 渠道实现（linshi.xyz）
 * 无建箱请求：本地随机 6 位 hex 前缀（官网 client 即用短 id）+ @linshi.xyz。
 * 读信 GET https://linshi.xyz/api/mails/{前缀}，无邮件时返回空数组，
 * 有邮件时为邮件对象数组（元素字段 headers.from/to/subject/date/html）。
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use rand::RngExt;
use serde_json::{json, Value};

const BASE_URL: &str = "https://linshi.xyz";
const DOMAIN: &str = "linshi.xyz";

/// 生成本地随机 6 位 hex 前缀（与官网 client 相同格式）
fn local_name() -> String {
    const HEX_DIGITS: &[u8] = b"0123456789abcdef";
    let mut rng = rand::rng();
    (0..6)
        .map(|_| {
            let idx = rng.random_range(0..HEX_DIGITS.len());
            HEX_DIGITS[idx] as char
        })
        .collect()
}

/// 创建 linshi.xyz 临时邮箱
/// 无需建箱请求，token 复用完整地址。
pub fn generate_email() -> Result<EmailInfo, String> {
    let addr = format!("{}@{DOMAIN}", local_name());
    Ok(EmailInfo {
        channel: Channel::LinshiXyz,
        email: addr.clone(),
        token: Some(addr),
        expires_at: None,
        created_at: None,
    })
}

/// 归一化单封邮件：headers 对象平铺为顶层字段并注入收件人地址
fn normalize_mail(raw: &Value, address: &str) -> Email {
    let mut flat = raw.clone();
    if let Some(headers) = raw.get("headers").and_then(|v| v.as_object()) {
        if let Some(obj) = flat.as_object_mut() {
            for (k, v) in headers {
                obj.insert(k.clone(), v.clone());
            }
        }
    }
    if flat.get("to").is_none() {
        if let Some(obj) = flat.as_object_mut() {
            obj.insert("to".to_string(), json!(address));
        }
    }
    normalize_email(&flat, address)
}

/// 读取 linshi.xyz 收件箱（GET /api/mails/{前缀}，响应为邮件对象数组）
pub fn get_emails(email: &str, _token: &str) -> Result<Vec<Email>, String> {
    let em = email.trim();
    let local = match em.split_once('@') {
        Some((prefix, _)) => prefix.to_string(),
        None => return Err(format!("linshi-xyz: 邮箱地址无效: {email:?}")),
    };

    block_on(async {
        let resp = http_client()
            .get(format!(
                "{BASE_URL}/api/mails/{}",
                urlencoding::encode(&local)
            ))
            .header("Accept", "application/json")
            .header("User-Agent", get_current_ua())
            .send()
            .await
            .map_err(|e| format!("linshi-xyz: 读取收件箱请求失败: {e}"))?;

        let status = resp.status();
        let body = resp
            .text()
            .await
            .map_err(|e| format!("linshi-xyz: 读取收件箱响应失败: {e}"))?;
        if !status.is_success() {
            return Err(format!("linshi-xyz: 读取收件箱失败 http {status}: {body}"));
        }

        let list: Value = serde_json::from_str(&body)
            .map_err(|e| format!("linshi-xyz: 解析收件箱响应失败: {e}"))?;
        let list = list
            .as_array()
            .cloned()
            .ok_or_else(|| "linshi-xyz: 收件箱响应非数组骨架".to_string())?;

        Ok(list.iter().map(|m| normalize_mail(m, em)).collect())
    })
}
