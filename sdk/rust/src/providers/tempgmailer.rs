/*!
 * TempGmailer 渠道实现
 * 网站: https://tempgmailer.com
 * 提供真实 @gmail.com 地址（dot trick）
 * 需要 CSRF token + Laravel session cookie 认证
 */

use crate::config::{block_on, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::Value;

const BASE_URL: &str = "https://tempgmailer.com";

fn browser_ua() -> &'static str {
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36"
}

/// 从首页 HTML 中提取 CSRF token 和 session cookie
fn init_session() -> Result<(String, String), String> {
    block_on(async {
        let resp = http_client()
            .get(BASE_URL)
            .header(
                "Accept",
                "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
            )
            .header("User-Agent", browser_ua())
            .send()
            .await
            .map_err(|e| format!("tempgmailer init session failed: {}", e))?;

        // 提取 Set-Cookie 中的 session cookie
        let mut cookie_parts: Vec<String> = Vec::new();
        for value in resp.headers().get_all("set-cookie") {
            let cookie_str = value.to_str().unwrap_or("");
            if let Some(name_value) = cookie_str.split(';').next() {
                cookie_parts.push(name_value.to_string());
            }
        }

        let cookies = cookie_parts.join("; ");
        if cookies.is_empty() {
            return Err("tempgmailer: failed to get session cookie".into());
        }

        // 从 HTML 中提取 CSRF token
        let html = resp
            .text()
            .await
            .map_err(|e| format!("tempgmailer read html failed: {}", e))?;

        let csrf_token = extract_csrf_token(&html)
            .ok_or_else(|| "tempgmailer: failed to extract csrf-token from HTML".to_string())?;

        Ok((csrf_token, cookies))
    })
}

/// 从 HTML meta 标签中提取 CSRF token
fn extract_csrf_token(html: &str) -> Option<String> {
    // 匹配 <meta name="csrf-token" content="...">
    let marker = "name=\"csrf-token\"";
    let pos = html.find(marker)?;
    let after = &html[pos..];
    let content_start = after.find("content=\"")? + "content=\"".len();
    let content_slice = &after[content_start..];
    let end = content_slice.find('"')?;
    Some(content_slice[..end].to_string())
}

/// 发送带认证头的 POST 请求
fn post_json(csrf_token: &str, cookies: &str, path: &str, body: Value) -> Result<String, String> {
    let csrf_token = csrf_token.to_string();
    let cookies = cookies.to_string();
    let path = path.to_string();
    block_on(async move {
        let resp = http_client()
            .post(format!("{}{}", BASE_URL, path))
            .header("User-Agent", browser_ua())
            .header("Content-Type", "application/json")
            .header("Accept", "application/json, text/plain, */*")
            .header("Origin", BASE_URL)
            .header("Referer", format!("{}/", BASE_URL))
            .header("X-Requested-With", "XMLHttpRequest")
            .header("X-CSRF-TOKEN", &csrf_token)
            .header("X-TempGmailer-Auth", "frontend")
            .header("Cookie", &cookies)
            .json(&body)
            .send()
            .await
            .map_err(|e| format!("tempgmailer {} failed: {}", path, e))?;

        let status = resp.status();
        let text = resp
            .text()
            .await
            .map_err(|e| format!("tempgmailer {} read failed: {}", path, e))?;
        if !status.is_success() {
            return Err(format!("tempgmailer {} failed: {} {}", path, status, text));
        }
        Ok(text)
    })
}

/// 创建临时邮箱
pub fn generate_email() -> Result<EmailInfo, String> {
    let (csrf_token, cookies) = init_session()?;
    let text = post_json(
        &csrf_token,
        &cookies,
        "/get-gmail",
        serde_json::json!({ "refresh": true, "adblock": 0 }),
    )?;
    let data: Value = serde_json::from_str(&text).map_err(|e| format!("parse failed: {}", e))?;

    if !data["success"].as_bool().unwrap_or(false) {
        return Err(format!("tempgmailer: generate failed: {}", text));
    }

    let email = data["data"]["email"]
        .as_str()
        .ok_or("tempgmailer: empty email in response")?
        .to_string();

    // 将 CSRF token 和 cookie 组合为 JSON 字符串作为 token
    let token_payload = serde_json::json!({
        "csrfToken": csrf_token,
        "cookies": cookies,
    })
    .to_string();

    Ok(EmailInfo {
        channel: Channel::Tempgmailer,
        email,
        token: Some(token_payload),
        expires_at: None,
        created_at: None,
    })
}

/// 获取邮件列表
pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let session: Value =
        serde_json::from_str(token).map_err(|e| format!("parse session token failed: {}", e))?;

    let csrf_token = session["csrfToken"].as_str().unwrap_or("");
    let cookies = session["cookies"].as_str().unwrap_or("");

    let text = post_json(
        csrf_token,
        cookies,
        "/get-inbox",
        serde_json::json!({ "email": email, "adblock": 0 }),
    )?;
    let data: Value = serde_json::from_str(&text).map_err(|e| format!("parse failed: {}", e))?;

    if !data["success"].as_bool().unwrap_or(false) {
        return Err(format!("tempgmailer: get inbox failed: {}", text));
    }

    let messages = data["data"]["messages"].as_array().cloned().unwrap_or_default();

    let mut result = Vec::new();
    for msg in &messages {
        let from_addr = msg["from"]["address"].as_str().unwrap_or("");
        let subject = msg["subject"].as_str().unwrap_or("");
        let html_content = msg["body"].as_str().unwrap_or(msg["intro"].as_str().unwrap_or(""));
        let text_content = msg["intro"].as_str().unwrap_or("");
        let created_at = msg["createdAt"].as_str().unwrap_or("");

        let raw = serde_json::json!({
            "from": from_addr,
            "to": email,
            "subject": subject,
            "text": text_content,
            "html": html_content,
            "date": created_at,
            "isRead": false,
            "attachments": [],
        });
        result.push(normalize_email(&raw, email));
    }

    Ok(result)
}
