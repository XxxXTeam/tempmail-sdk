/*!
 * restmail-net 渠道 — https://restmail.net
 *
 * Mozilla 开源项目，无需创建邮箱，ad-hoc 模式。
 * 随机生成 username，邮箱即为 username@restmail.net。
 * 收件: GET https://restmail.net/mail/{username}
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use rand::Rng;
use serde_json::Value;

const BASE_URL: &str = "https://restmail.net";

/// 生成 10-12 位随机用户名
fn random_username() -> String {
    const CHARS: &[u8] = b"abcdefghijklmnopqrstuvwxyz0123456789";
    let mut rng = rand::thread_rng();
    let len = rng.gen_range(10..=12);
    (0..len)
        .map(|_| CHARS[rng.gen_range(0..CHARS.len())] as char)
        .collect()
}

/// 创建 restmail.net 临时邮箱
/// 无需请求服务端，随机生成 username 即可使用
pub fn generate_email() -> Result<EmailInfo, String> {
    let username = random_username();
    let email = format!("{}@restmail.net", username);
    Ok(EmailInfo {
        channel: Channel::RestmailNet,
        email,
        token: None,
        expires_at: None,
        created_at: None,
    })
}

/// 获取 restmail.net 邮件列表
/// GET /mail/{username}，返回 JSON 数组
pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let em = email.trim();
    if em.is_empty() {
        return Err("restmail-net: 邮箱地址为空".into());
    }

    // 从邮箱地址中提取 username（@前面部分）
    let username = em.split('@').next().unwrap_or("");
    if username.is_empty() {
        return Err("restmail-net: 无法提取用户名".into());
    }

    let url = format!("{}/mail/{}", BASE_URL, username);
    block_on(async {
        let resp = http_client()
            .get(&url)
            .header("Accept", "application/json")
            .header("User-Agent", get_current_ua())
            .send()
            .await
            .map_err(|e| e.to_string())?;

        if resp.status().as_u16() == 404 {
            return Ok(Vec::new());
        }
        if !resp.status().is_success() {
            return Err(format!("restmail-net: HTTP {}", resp.status()));
        }

        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let rows = match data.as_array() {
            Some(arr) => arr,
            None => return Ok(Vec::new()),
        };

        let mut out = Vec::new();
        for msg in rows {
            // 提取发件人：优先 from[0].address，其次 headers.from
            let from_addr = msg
                .get("from")
                .and_then(|f| f.as_array())
                .and_then(|arr| arr.first())
                .and_then(|obj| obj.get("address"))
                .and_then(|v| v.as_str())
                .unwrap_or("")
                .to_string();

            let from_addr = if from_addr.is_empty() {
                msg.get("headers")
                    .and_then(|h| h.get("from"))
                    .and_then(|v| v.as_str())
                    .unwrap_or("")
                    .to_string()
            } else {
                from_addr
            };

            // 邮件正文：优先 html，其次 text
            let html = msg.get("html").and_then(|v| v.as_str()).unwrap_or("");
            let text = msg.get("text").and_then(|v| v.as_str()).unwrap_or("");

            let flat = serde_json::json!({
                "id": msg.get("id").and_then(|v| v.as_str()).unwrap_or(""),
                "from": from_addr,
                "to": em,
                "subject": msg.get("subject").and_then(|v| v.as_str()).unwrap_or(""),
                "text": text,
                "html": html,
                "date": msg.get("receivedAt").and_then(|v| v.as_str()).unwrap_or(""),
                "is_read": false,
            });
            out.push(normalize_email(&flat, em));
        }
        Ok(out)
    })
}
