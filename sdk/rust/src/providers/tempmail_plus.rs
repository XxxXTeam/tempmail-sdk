/*!
 * TempMail Plus -- https://tempmail.plus
 * 无需认证、无需 Cookie、无需 Token 的简单 REST API 渠道
 * 域名: mailto.plus
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use rand::Rng;
use serde_json::Value;

const API_BASE: &str = "https://tempmail.plus/api/mails";
const DEFAULT_DOMAIN: &str = "mailto.plus";

/// 随机生成 12 位字母数字字符串作为本地部分
fn random_local() -> String {
    const CHARS: &[u8] = b"abcdefghijklmnopqrstuvwxyz0123456789";
    let mut rng = rand::thread_rng();
    (0..12)
        .map(|_| CHARS[rng.gen_range(0..CHARS.len())] as char)
        .collect()
}

pub fn generate_email(domain: Option<&str>, channel: Channel) -> Result<EmailInfo, String> {
    let selected_domain = domain
        .map(str::trim)
        .filter(|d| !d.is_empty())
        .unwrap_or(DEFAULT_DOMAIN);
    let local = random_local();
    let email = format!("{}@{}", local, selected_domain);

    // 调用列表接口验证地址可用
    block_on(async {
        let resp = http_client()
            .get(format!("{}/?email={}&epin=", API_BASE, email))
            .header("Accept", "application/json")
            .header("User-Agent", get_current_ua())
            .header("Referer", "https://tempmail.plus/")
            .header("Origin", "https://tempmail.plus")
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("tempmail-plus: 验证邮箱失败 {}", resp.status()));
        }
        Ok(EmailInfo {
            channel,
            email,
            token: None,
            expires_at: None,
            created_at: None,
        })
    })
}

/// 获取单封邮件正文
fn fetch_body(mail_id: i64, email: &str) -> (String, String) {
    if mail_id == 0 {
        return (String::new(), String::new());
    }
    let url = format!("{}/{}?email={}&epin=", API_BASE, mail_id, email);
    match block_on(async {
        let resp = http_client()
            .get(&url)
            .header("Accept", "application/json")
            .header("User-Agent", get_current_ua())
            .header("Referer", "https://tempmail.plus/")
            .header("Origin", "https://tempmail.plus")
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("tempmail-plus email detail {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let html = data
            .get("html")
            .and_then(|x| x.as_str())
            .unwrap_or("")
            .to_string();
        let text = data
            .get("text")
            .and_then(|x| x.as_str())
            .unwrap_or("")
            .to_string();
        Ok((html, text))
    }) {
        Ok(pair) => pair,
        Err(_) => (String::new(), String::new()),
    }
}

pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let em = email.trim();
    if em.is_empty() {
        return Err("tempmail-plus: 邮箱地址为空".into());
    }

    let url = format!("{}/?email={}&epin=", API_BASE, em);
    block_on(async {
        let resp = http_client()
            .get(&url)
            .header("Accept", "application/json")
            .header("User-Agent", get_current_ua())
            .header("Referer", "https://tempmail.plus/")
            .header("Origin", "https://tempmail.plus")
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("tempmail-plus list {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let rows = data
            .get("mail_list")
            .and_then(|x| x.as_array())
            .cloned()
            .unwrap_or_default();

        let mut out = Vec::new();
        for raw in rows {
            let mail_id = raw.get("mail_id").and_then(|x| x.as_i64()).unwrap_or(0);
            let (html, text) = fetch_body(mail_id, em);

            let flat = serde_json::json!({
                "id": if mail_id != 0 { mail_id.to_string() } else { String::new() },
                "from": raw.get("from_mail").and_then(|x| x.as_str())
                    .or_else(|| raw.get("from_name").and_then(|x| x.as_str()))
                    .unwrap_or(""),
                "to": em,
                "subject": raw.get("subject").and_then(|x| x.as_str()).unwrap_or(""),
                "date": raw.get("time").and_then(|x| x.as_str()).unwrap_or(""),
                "html": html,
                "text": text,
                "isRead": !raw.get("is_new").and_then(|x| x.as_bool()).unwrap_or(true),
            });
            out.push(normalize_email(&flat, em));
        }
        Ok(out)
    })
}
