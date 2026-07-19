/*!
 * ten-minute-mail-net 渠道 — https://10minutemail.net
 *
 * PHP session 管理的临时邮箱。
 * 创建: GET /address.api.php（提取 PHPSESSID cookie + mail_get_mail）
 * 收信: GET /address.api.php（带 Cookie）→ mail_list
 *       GET /mail.api.php?mailid={id}（带 Cookie）→ 邮件详情
 * token 格式: JSON {"cookie":"PHPSESSID=xxx"}
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::Value;

const BASE_URL: &str = "https://10minutemail.net";

/// 创建 10minutemail.net 临时邮箱
pub fn generate_email() -> Result<EmailInfo, String> {
    let url = format!("{}/address.api.php", BASE_URL);
    block_on(async {
        let resp = http_client()
            .get(&url)
            .header("Accept", "application/json, text/plain, */*")
            .header("User-Agent", get_current_ua())
            .send()
            .await
            .map_err(|e| format!("ten-minute-mail-net: 请求失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("ten-minute-mail-net: HTTP {}", resp.status()));
        }

        let mut session_id = String::new();
        for cookie_str in resp.headers().get_all("set-cookie") {
            if let Ok(s) = cookie_str.to_str() {
                if s.contains("PHPSESSID=") {
                    if let Some(start) = s.find("PHPSESSID=") {
                        let rest = &s[start + 10..];
                        let end = rest.find(';').unwrap_or(rest.len());
                        session_id = rest[..end].to_string();
                    }
                }
            }
        }
        if session_id.is_empty() {
            return Err("ten-minute-mail-net: 响应中未找到 PHPSESSID".into());
        }

        let data: Value = resp.json().await.map_err(|e| format!("ten-minute-mail-net: 解析响应失败: {}", e))?;
        let email_addr = data.get("mail_get_mail")
            .and_then(|v| v.as_str())
            .unwrap_or("")
            .to_string();
        if email_addr.is_empty() || !email_addr.contains('@') {
            return Err("ten-minute-mail-net: 响应缺少有效邮箱地址".into());
        }

        let token = serde_json::json!({ "cookie": format!("PHPSESSID={}", session_id) }).to_string();

        Ok(EmailInfo {
            channel: Channel::TenMinuteMailNet,
            email: email_addr,
            token: Some(token),
            expires_at: None,
            created_at: None,
        })
    })
}

/// 获取 10minutemail.net 邮件列表
pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    if token.is_empty() {
        return Err("ten-minute-mail-net: token 为空".into());
    }

    let parsed: Value = serde_json::from_str(token)
        .map_err(|_| "ten-minute-mail-net: token 格式无效".to_string())?;
    let cookie = parsed.get("cookie")
        .and_then(|v| v.as_str())
        .unwrap_or("")
        .to_string();
    if cookie.is_empty() {
        return Err("ten-minute-mail-net: token 缺少 cookie 字段".into());
    }

    let list_url = format!("{}/address.api.php", BASE_URL);
    block_on(async {
        let resp = http_client()
            .get(&list_url)
            .header("Accept", "application/json, text/plain, */*")
            .header("User-Agent", get_current_ua())
            .header("Cookie", &cookie)
            .send()
            .await
            .map_err(|e| format!("ten-minute-mail-net: 获取列表失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("ten-minute-mail-net: HTTP {}", resp.status()));
        }

        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let mail_list = match data.get("mail_list").and_then(|v| v.as_array()) {
            Some(arr) => arr.clone(),
            None => return Ok(Vec::new()),
        };

        if mail_list.is_empty() {
            return Ok(Vec::new());
        }

        let mut emails = Vec::new();
        for item in &mail_list {
            let mail_id = item.get("mail_id").and_then(|v| v.as_str()).unwrap_or("");
            if mail_id.is_empty() {
                continue;
            }
            match fetch_detail(&cookie, mail_id, email).await {
                Ok(e) => emails.push(e),
                Err(_) => continue,
            }
        }
        Ok(emails)
    })
}

async fn fetch_detail(cookie: &str, mail_id: &str, recipient: &str) -> Result<Email, String> {
    let url = format!("{}/mail.api.php?mailid={}", BASE_URL, mail_id);
    let resp = http_client()
        .get(&url)
        .header("Accept", "application/json, text/plain, */*")
        .header("User-Agent", get_current_ua())
        .header("Cookie", cookie)
        .send()
        .await
        .map_err(|e| e.to_string())?;

    if !resp.status().is_success() {
        return Err(format!("HTTP {}", resp.status()));
    }

    let data: Value = resp.json().await.map_err(|e| e.to_string())?;

    let mut text_body = String::new();
    let mut html_body = String::new();
    if let Some(bodies) = data.get("body").and_then(|v| v.as_array()) {
        for b in bodies {
            let content_type = b.get("content").and_then(|v| v.as_str()).unwrap_or("");
            let body_text = b.get("body").and_then(|v| v.as_str()).unwrap_or("");
            match content_type {
                "text/plain" => text_body = body_text.to_string(),
                "text/html" => html_body = body_text.to_string(),
                _ => {}
            }
        }
    }
    if text_body.is_empty() {
        if let Some(plain) = data.get("plain").and_then(|v| v.as_array()) {
            if let Some(first) = plain.first().and_then(|v| v.as_str()) {
                text_body = first.to_string();
            }
        }
    }

    let flat = serde_json::json!({
        "id": mail_id,
        "from": data.get("from").and_then(|v| v.as_str()).unwrap_or(""),
        "to": recipient,
        "subject": data.get("subject").and_then(|v| v.as_str()).unwrap_or(""),
        "text": text_body,
        "html": html_body,
        "date": data.get("datetime").and_then(|v| v.as_str()).unwrap_or(""),
    });
    Ok(normalize_email(&flat, recipient))
}
