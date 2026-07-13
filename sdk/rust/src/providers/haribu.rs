/*!
 * Haribu 渠道实现
 * 网站: https://haribu.net
 * 模式: Tempail_etc 模式，通过 kontrol API 创建邮箱和获取邮件
 * 域名: yevme.com
 *
 * 流程:
 *   1. GET https://haribu.net 获取首页 HTML 和 session cookies
 *   2. 从 HTML 中提取 <input id="eposta_adres" value="xxx@yevme.com"> 获取邮箱地址
 *   3. GET https://haribu.net/en/api-kontrol/ 通过 kontrol API 获取邮件列表
 */

use crate::config::{block_on, http_client_no_cookie_jar};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use regex_lite::Regex;
use serde_json::Value;

const BASE_URL: &str = "https://haribu.net";

/// 浏览器 User-Agent
fn browser_ua() -> &'static str {
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36"
}

/// 从响应头中提取 set-cookie 并拼接为 Cookie 字符串
fn extract_cookies(headers: &wreq::header::HeaderMap) -> String {
    let mut parts: Vec<String> = Vec::new();
    for value in headers.get_all("set-cookie") {
        let cookie_str = value.to_str().unwrap_or("");
        if let Some(name_value) = cookie_str.split(';').next() {
            let trimmed = name_value.trim();
            if !trimmed.is_empty() {
                parts.push(trimmed.to_string());
            }
        }
    }
    parts.join("; ")
}

/// 初始化会话：访问首页获取邮箱地址和 session cookies
fn init_session() -> Result<(String, String), String> {
    block_on(async {
        let resp = http_client_no_cookie_jar()
            .get(BASE_URL)
            .header(
                "Accept",
                "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
            )
            .header("Accept-Language", "en-US,en;q=0.9")
            .header("User-Agent", browser_ua())
            .send()
            .await
            .map_err(|e| format!("haribu: 初始化会话失败: {}", e))?;

        let cookies = extract_cookies(resp.headers());
        if cookies.is_empty() {
            return Err("haribu: 未获取到 session cookies".into());
        }

        let html = resp
            .text()
            .await
            .map_err(|e| format!("haribu: 读取首页失败: {}", e))?;

        /* 从 HTML 中提取 <input id="eposta_adres" value="xxx@yevme.com"> */
        let re =
            Regex::new(r#"<input[^>]+id\s*=\s*"eposta_adres"[^>]+value\s*=\s*"([^"]+)""#).unwrap();
        let email = re
            .captures(&html)
            .and_then(|c| c.get(1))
            .map(|m| m.as_str().trim().to_string())
            .ok_or_else(|| "haribu: 未从 HTML 中提取到邮箱地址".to_string())?;

        if email.is_empty() || !email.contains('@') {
            return Err(format!("haribu: 提取到的邮箱地址无效: {}", email));
        }

        Ok((email, cookies))
    })
}

/// 创建临时邮箱
pub fn generate_email() -> Result<EmailInfo, String> {
    let (email, cookies) = init_session()?;

    Ok(EmailInfo {
        channel: Channel::Haribu,
        email,
        token: Some(cookies),
        expires_at: None,
        created_at: None,
    })
}

/// 获取邮件列表
pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    if token.is_empty() {
        return Err("haribu: token(cookies) 不能为空".into());
    }

    block_on(async {
        let resp = http_client_no_cookie_jar()
            .get(format!("{}/en/api-kontrol/", BASE_URL))
            .header("Accept", "application/json, text/plain, */*")
            .header("Accept-Language", "en-US,en;q=0.9")
            .header("User-Agent", browser_ua())
            .header("Referer", format!("{}/", BASE_URL))
            .header("X-Requested-With", "XMLHttpRequest")
            .header("Cookie", token)
            .send()
            .await
            .map_err(|e| format!("haribu: kontrol API 请求失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("haribu: kontrol API 返回 {}", resp.status()));
        }

        let body = resp
            .text()
            .await
            .map_err(|e| format!("haribu: 读取 kontrol 响应失败: {}", e))?;

        /* 尝试解析为 JSON 数组或包含邮件列表的对象 */
        let data: Value = serde_json::from_str(&body).unwrap_or(Value::Null);

        let items: Vec<&Value> = if let Some(arr) = data.as_array() {
            arr.iter().collect()
        } else if let Some(arr) = data.get("emails").and_then(|v| v.as_array()) {
            arr.iter().collect()
        } else if let Some(arr) = data.get("data").and_then(|v| v.as_array()) {
            arr.iter().collect()
        } else if let Some(arr) = data.get("mail").and_then(|v| v.as_array()) {
            arr.iter().collect()
        } else if let Some(arr) = data.get("mails").and_then(|v| v.as_array()) {
            arr.iter().collect()
        } else {
            /* 无邮件或响应格式不匹配，返回空列表 */
            return Ok(Vec::new());
        };

        let mut result = Vec::new();
        for msg in items {
            let id = msg
                .get("id")
                .or_else(|| msg.get("mail_id"))
                .or_else(|| msg.get("uid"))
                .cloned()
                .unwrap_or(Value::Null);

            let from = msg["from"]
                .as_str()
                .or_else(|| msg["from_address"].as_str())
                .or_else(|| msg["sender"].as_str())
                .or_else(|| msg["mail_from"].as_str())
                .unwrap_or("")
                .to_string();

            let subject = msg["subject"]
                .as_str()
                .or_else(|| msg["mail_subject"].as_str())
                .or_else(|| msg["konu"].as_str())
                .unwrap_or("")
                .to_string();

            let text_body = msg["text"]
                .as_str()
                .or_else(|| msg["body"].as_str())
                .or_else(|| msg["text_body"].as_str())
                .or_else(|| msg["content"].as_str())
                .unwrap_or("")
                .to_string();

            let html_body = msg["html"]
                .as_str()
                .or_else(|| msg["html_body"].as_str())
                .or_else(|| msg["body_html"].as_str())
                .unwrap_or("")
                .to_string();

            let date = msg["date"]
                .as_str()
                .or_else(|| msg["received_at"].as_str())
                .or_else(|| msg["tarih"].as_str())
                .or_else(|| msg["zaman"].as_str())
                .unwrap_or("")
                .to_string();

            let raw = serde_json::json!({
                "id": id,
                "from": from,
                "to": email,
                "subject": subject,
                "text": text_body,
                "html": html_body,
                "date": date,
                "isRead": false,
                "attachments": [],
            });
            result.push(normalize_email(&raw, email));
        }

        Ok(result)
    })
}
