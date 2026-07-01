/*!
 * byom.de — 纯 GET 无认证的简单临时邮箱
 * 创建邮箱: 直接生成随机用户名 + "@byom.de"
 * 获取邮件: GET https://api.byom.de/mails/{username} → JSON 数组
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use rand::Rng;
use serde_json::Value;

/// 随机生成 10 位字母数字字符串作为用户名
fn random_username() -> String {
    const CHARS: &[u8] = b"abcdefghijklmnopqrstuvwxyz0123456789";
    let mut rng = rand::thread_rng();
    (0..10)
        .map(|_| CHARS[rng.gen_range(0..CHARS.len())] as char)
        .collect()
}

/// 创建邮箱，无需调用 API，直接生成随机地址
pub fn generate_email() -> Result<EmailInfo, String> {
    let username = random_username();
    let email = format!("{}@byom.de", username);
    Ok(EmailInfo {
        channel: Channel::Byom,
        email,
        token: None,
        expires_at: None,
        created_at: None,
    })
}

/// 获取邮件列表
pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let em = email.trim();
    if em.is_empty() {
        return Err("byom: 邮箱地址为空".into());
    }
    // 从邮箱地址提取用户名部分
    let username = em.rsplit_once('@').map(|(a, _)| a).unwrap_or(em);
    let url = format!("https://api.byom.de/mails/{}", username);

    block_on(async {
        let resp = http_client()
            .get(&url)
            .header("Accept", "application/json, text/plain, */*")
            .header("User-Agent", get_current_ua())
            .send()
            .await
            .map_err(|e| format!("byom: {}", e))?;
        if !resp.status().is_success() {
            return Err(format!("byom: 获取邮件失败 {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| format!("byom: {}", e))?;
        let rows = data.as_array().cloned().unwrap_or_default();
        let mut out = Vec::new();
        for raw in rows {
            out.push(normalize_email(&raw, em));
        }
        Ok(out)
    })
}
