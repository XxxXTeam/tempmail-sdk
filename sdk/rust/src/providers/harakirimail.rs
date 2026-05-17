/*!
 * HarakiriMail -- https://harakirimail.com
 * 无需认证、无需 Cookie、无需 Token 的简单 REST API 渠道
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use rand::Rng;
use serde_json::Value;

const BASE: &str = "https://harakirimail.com";

/// 随机生成 12 位字母数字字符串作为收件箱名
fn random_name() -> String {
    const CHARS: &[u8] = b"abcdefghijklmnopqrstuvwxyz0123456789";
    let mut rng = rand::thread_rng();
    (0..12)
        .map(|_| CHARS[rng.gen_range(0..CHARS.len())] as char)
        .collect()
}

pub fn generate_email() -> Result<EmailInfo, String> {
    let name = random_name();
    let email = format!("{}@harakirimail.com", name);

    // 可选：调用收件箱接口验证地址可用
    block_on(async {
        let resp = http_client()
            .get(format!("{}/api/v1/inbox/{}", BASE, name))
            .header("Accept", "application/json, text/plain, */*")
            .header("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
            .header("User-Agent", get_current_ua())
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("harakirimail: 验证收件箱失败 {}", resp.status()));
        }
        Ok(EmailInfo {
            channel: Channel::Harakirimail,
            email,
            token: None,
            expires_at: None,
            created_at: None,
        })
    })
}

/// 获取单封邮件正文
fn fetch_body(id: &str) -> (String, String) {
    if id.is_empty() {
        return (String::new(), String::new());
    }
    let url = format!("{}/api/v1/email/{}", BASE, id);
    match block_on(async {
        let resp = http_client()
            .get(&url)
            .header("Accept", "application/json, text/plain, */*")
            .header("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
            .header("User-Agent", get_current_ua())
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("harakirimail email detail {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let html = data
            .get("body_html")
            .or_else(|| data.get("html"))
            .and_then(|x| x.as_str())
            .unwrap_or("")
            .to_string();
        let text = data
            .get("body_text")
            .or_else(|| data.get("text"))
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
        return Err("harakirimail: 邮箱地址为空".into());
    }

    // 从邮箱地址提取收件箱名
    let name = em.rsplit_once('@').map(|(a, _)| a).unwrap_or(em);

    let url = format!("{}/api/v1/inbox/{}", BASE, name);
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
            return Err(format!("harakirimail inbox {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let rows = data
            .get("emails")
            .and_then(|x| x.as_array())
            .cloned()
            .unwrap_or_default();

        let mut out = Vec::new();
        for raw in rows {
            let id = raw.get("_id").and_then(|x| x.as_str()).unwrap_or("");
            let (html, text) = fetch_body(id);

            let flat = serde_json::json!({
                "id": id,
                "from": raw.get("from").and_then(|x| x.as_str()).unwrap_or(""),
                "to": em,
                "subject": raw.get("subject").and_then(|x| x.as_str()).unwrap_or(""),
                "date": raw.get("received").and_then(|x| x.as_str()).unwrap_or(""),
                "html": html,
                "text": text,
                "isRead": false,
            });
            out.push(normalize_email(&flat, em));
        }
        Ok(out)
    })
}
