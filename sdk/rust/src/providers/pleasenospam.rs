/*!
 * PleeaseNoSpam -- https://pleasenospam.email
 * 无需认证的简单 REST API 渠道
 * 支持域名: pleasenospam.email, spamlessmail.org
 * 获取邮件: GET https://pleasenospam.email/{email}.json
 * 返回 JSON 数组，from 字段为数组格式
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use rand::Rng;
use serde_json::Value;

const BASE: &str = "https://pleasenospam.email";
const DEFAULT_DOMAIN: &str = "pleasenospam.email";

/// 随机生成 12 位字母数字字符串作为本地部分
fn random_local() -> String {
    const CHARS: &[u8] = b"abcdefghijklmnopqrstuvwxyz0123456789";
    let mut rng = rand::thread_rng();
    (0..12)
        .map(|_| CHARS[rng.gen_range(0..CHARS.len())] as char)
        .collect()
}

/// 创建临时邮箱
/// 无需调用 API，直接生成 {randomLocal}@pleasenospam.email 格式地址
pub fn generate_email() -> Result<EmailInfo, String> {
    let local = random_local();
    let email = format!("{}@{}", local, DEFAULT_DOMAIN);
    Ok(EmailInfo {
        channel: Channel::Pleasenospam,
        email,
        token: None,
        expires_at: None,
        created_at: None,
    })
}

/// 获取邮件列表
/// API: GET https://pleasenospam.email/{email}.json
/// 返回 JSON 数组，每个元素的 from 字段为数组，取 from[0] 作为发件人
pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let em = email.trim();
    if em.is_empty() {
        return Err("pleasenospam: 邮箱地址为空".into());
    }

    let url = format!("{}/{}.json", BASE, em);
    block_on(async {
        let resp = http_client()
            .get(&url)
            .header("Accept", "application/json, text/plain, */*")
            .header("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
            .header("User-Agent", get_current_ua())
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("pleasenospam: 获取邮件失败 {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let rows = data.as_array().cloned().unwrap_or_default();

        let mut out = Vec::new();
        for raw in rows {
            // from 字段是数组，取第一个元素作为发件人地址
            let from_addr = raw
                .get("from")
                .and_then(|v| v.as_array())
                .and_then(|arr| arr.first())
                .and_then(|v| v.as_str())
                .unwrap_or("");

            // receivedDate 为秒级时间戳
            let date = raw
                .get("receivedDate")
                .and_then(|v| v.as_f64())
                .unwrap_or(0.0);

            let flat = serde_json::json!({
                "id": raw.get("id").and_then(|x| x.as_str()).unwrap_or(""),
                "from": from_addr,
                "to": em,
                "subject": raw.get("subject").and_then(|x| x.as_str()).unwrap_or(""),
                "date": date,
                "text": raw.get("text").and_then(|x| x.as_str()).unwrap_or(""),
                "html": raw.get("html").and_then(|x| x.as_str()).unwrap_or(""),
                "isRead": false,
            });
            out.push(normalize_email(&flat, em));
        }
        Ok(out)
    })
}
