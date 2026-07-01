/*!
 * ExpressInboxHub 渠道实现
 * 网站: https://expressinboxhub.com
 * 模式: Fake_trash_mail（CSRF token + session cookies）
 * 域名: mail42.shop
 */

use crate::config::{block_on, http_client_no_cookie_jar};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use regex_lite::Regex;
use serde_json::Value;

const BASE_URL: &str = "https://expressinboxhub.com";

/// 浏览器 User-Agent
fn browser_ua() -> &'static str {
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36"
}

/// 初始化会话：访问首页获取 CSRF token 和 session cookies
fn init_session() -> Result<(String, String), String> {
    block_on(async {
        let resp = http_client_no_cookie_jar()
            .get(BASE_URL)
            .header(
                "Accept",
                "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
            )
            .header("User-Agent", browser_ua())
            .send()
            .await
            .map_err(|e| format!("expressinboxhub 初始化会话失败: {}", e))?;

        /* 提取 set-cookie 头中的所有 cookie */
        let mut cookie_parts: Vec<String> = Vec::new();
        for value in resp.headers().get_all("set-cookie") {
            let cookie_str = value.to_str().unwrap_or("");
            if let Some(name_value) = cookie_str.split(';').next() {
                cookie_parts.push(name_value.to_string());
            }
        }

        let html = resp
            .text()
            .await
            .map_err(|e| format!("expressinboxhub 读取页面失败: {}", e))?;

        /* 从 HTML 中提取 <meta name="csrf-token" content="xxx"> */
        let re = Regex::new(r#"<meta\s+name="csrf-token"\s+content="([^"]+)""#).unwrap();
        let csrf_token = re
            .captures(&html)
            .and_then(|c| c.get(1))
            .map(|m| m.as_str().to_string())
            .ok_or_else(|| "expressinboxhub: 未找到 CSRF token".to_string())?;

        if cookie_parts.is_empty() {
            return Err("expressinboxhub: 未获取到 session cookies".into());
        }

        Ok((csrf_token, cookie_parts.join("; ")))
    })
}

/// 向 /messages 端点发送 POST 请求
fn post_messages(csrf_token: &str, cookies: &str) -> Result<String, String> {
    let csrf_token = csrf_token.to_string();
    let cookies = cookies.to_string();
    block_on(async move {
        let resp = http_client_no_cookie_jar()
            .post(format!("{}/messages", BASE_URL))
            .header("User-Agent", browser_ua())
            .header("Content-Type", "application/json")
            .header("Accept", "application/json, text/plain, */*")
            .header("Origin", BASE_URL)
            .header("Referer", format!("{}/", BASE_URL))
            .header("X-Requested-With", "XMLHttpRequest")
            .header("X-CSRF-TOKEN", &csrf_token)
            .header("Cookie", &cookies)
            .json(&serde_json::json!({ "_token": csrf_token }))
            .send()
            .await
            .map_err(|e| format!("expressinboxhub /messages 请求失败: {}", e))?;

        let status = resp.status();
        let text = resp
            .text()
            .await
            .map_err(|e| format!("expressinboxhub /messages 读取失败: {}", e))?;

        if !status.is_success() {
            return Err(format!(
                "expressinboxhub /messages 失败: {} {}",
                status, text
            ));
        }
        Ok(text)
    })
}

/// 创建临时邮箱
pub fn generate_email() -> Result<EmailInfo, String> {
    let (csrf_token, cookies) = init_session()?;
    let text = post_messages(&csrf_token, &cookies)?;

    let data: Value =
        serde_json::from_str(&text).map_err(|e| format!("expressinboxhub 解析响应失败: {}", e))?;

    let mailbox = data["mailbox"]
        .as_str()
        .unwrap_or("")
        .trim()
        .to_string();

    if mailbox.is_empty() {
        return Err("expressinboxhub: 创建邮箱失败，未返回邮箱地址".into());
    }

    /* 将 CSRF token 和 cookies 序列化为 token，供后续获取邮件使用 */
    let token_payload = serde_json::json!({
        "csrfToken": csrf_token,
        "cookies": cookies,
    })
    .to_string();

    Ok(EmailInfo {
        channel: Channel::Expressinboxhub,
        email: mailbox,
        token: Some(token_payload),
        expires_at: None,
        created_at: None,
    })
}

/// 获取邮件列表
pub fn get_emails(token: &str, _email: &str) -> Result<Vec<Email>, String> {
    let session: Value = serde_json::from_str(token)
        .map_err(|e| format!("expressinboxhub 解析 session token 失败: {}", e))?;

    let csrf_token = session["csrfToken"]
        .as_str()
        .ok_or("expressinboxhub: session 中缺少 csrfToken")?;
    let cookies = session["cookies"]
        .as_str()
        .ok_or("expressinboxhub: session 中缺少 cookies")?;

    let text = post_messages(csrf_token, cookies)?;
    let data: Value =
        serde_json::from_str(&text).map_err(|e| format!("expressinboxhub 解析响应失败: {}", e))?;

    let messages = data["messages"].as_array().cloned().unwrap_or_default();

    let mailbox = data["mailbox"].as_str().unwrap_or(_email);

    let mut result = Vec::new();
    for msg in &messages {
        let raw = serde_json::json!({
            "id": msg["id"],
            "from": "",
            "to": mailbox,
            "subject": msg["subject"].as_str().unwrap_or(""),
            "text": "",
            "html": msg["content"].as_str().unwrap_or(""),
            "receivedAt": msg["receivedAt"].as_str().unwrap_or(""),
            "isRead": false,
            "attachments": [],
        });
        result.push(normalize_email(&raw, mailbox));
    }

    Ok(result)
}
