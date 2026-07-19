/*!
 * mailgolem.com 渠道实现
 * 网站: https://mailgolem.com
 * 域名: @mailgolem.com
 *
 * 流程（需要 Cookie Session + CSRF Token）:
 *   1. GET https://mailgolem.com/ 获取 session cookie 并从 HTML 提取 CSRF token
 *   2. GET https://mailgolem.com/random-email-address 获取随机邮箱地址（纯文本）
 *   3. POST https://mailgolem.com/fetch-emails/{email}
 *      body: _token={csrf}  Content-Type: application/x-www-form-urlencoded
 *      X-Requested-With: XMLHttpRequest
 *      返回 JSON 数组: [{id, from, subject}] 或空数组 []
 *
 * token 存储: CSRF token 值
 * 注意: get_emails 时需要重新建立 session（GET / 获取新 cookie + CSRF），
 *       因为 Laravel session 可能过期
 */

use crate::config::{block_on, http_client_no_cookie_jar};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use regex::Regex;
use serde_json::Value;
use std::sync::LazyLock;

const BASE_URL: &str = "https://mailgolem.com";

/// 匹配 CSRF token: <input type="hidden" name="_token" id="token" value="{csrf}">
static CSRF_RE: LazyLock<Regex> = LazyLock::new(|| {
    Regex::new(r#"<input[^>]*name="_token"[^>]*value="([^"]+)""#).expect("csrf regex")
});

/// 备用匹配顺序：value 在 name 前面的情况
static CSRF_RE_ALT: LazyLock<Regex> = LazyLock::new(|| {
    Regex::new(r#"<input[^>]*value="([^"]+)"[^>]*name="_token""#).expect("csrf alt regex")
});

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

/// 从 HTML 中提取 CSRF token
fn extract_csrf(html: &str) -> Option<String> {
    CSRF_RE
        .captures(html)
        .or_else(|| CSRF_RE_ALT.captures(html))
        .and_then(|c| c.get(1))
        .map(|m| m.as_str().to_string())
}

/// 获取首页并返回 (cookies, csrf_token)
async fn fetch_session() -> Result<(String, String), String> {
    let resp = http_client_no_cookie_jar()
        .get(format!("{}/", BASE_URL))
        .header(
            "Accept",
            "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
        )
        .header("Accept-Language", "en-US,en;q=0.9")
        .header("User-Agent", browser_ua())
        .send()
        .await
        .map_err(|e| format!("mailgolem: 获取首页失败: {}", e))?;

    if !resp.status().is_success() {
        return Err(format!("mailgolem: 获取首页返回 HTTP {}", resp.status()));
    }

    let cookies = extract_cookies(resp.headers());
    if cookies.is_empty() {
        return Err("mailgolem: 未获取到 session cookie".into());
    }

    let html = resp
        .text()
        .await
        .map_err(|e| format!("mailgolem: 读取首页响应失败: {}", e))?;

    let csrf = extract_csrf(&html)
        .ok_or_else(|| "mailgolem: 未从 HTML 中提取到 CSRF token".to_string())?;

    Ok((cookies, csrf))
}

/// 创建临时邮箱
/// 1. GET / 获取 session cookie + CSRF token
/// 2. GET /random-email-address 获取随机邮箱地址
///
/// token 存储 CSRF token 值
pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        /* 第一步：获取 session 和 CSRF token */
        let (cookies, csrf) = fetch_session().await?;

        /* 第二步：获取随机邮箱地址 */
        let resp = http_client_no_cookie_jar()
            .get(format!("{}/random-email-address", BASE_URL))
            .header(
                "Accept",
                "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
            )
            .header("Accept-Language", "en-US,en;q=0.9")
            .header("User-Agent", browser_ua())
            .header("Referer", format!("{}/", BASE_URL))
            .header("Cookie", &cookies)
            .send()
            .await
            .map_err(|e| format!("mailgolem: 获取随机邮箱失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!(
                "mailgolem: 获取随机邮箱返回 HTTP {}",
                resp.status()
            ));
        }

        let email = resp
            .text()
            .await
            .map_err(|e| format!("mailgolem: 读取邮箱地址响应失败: {}", e))?
            .trim()
            .to_string();

        if email.is_empty() || !email.contains('@') {
            return Err(format!("mailgolem: 返回的邮箱地址无效: {}", email));
        }

        Ok(EmailInfo {
            channel: Channel::Mailgolem,
            email,
            token: Some(csrf),
            expires_at: None,
            created_at: None,
        })
    })
}

/// 获取邮件列表
/// 1. GET / 获取新 session + CSRF token（session 可能过期）
/// 2. POST /fetch-emails/{email} 获取邮件列表
///
/// token 参数为之前保存的 CSRF token（此处忽略，重新获取新 session）
pub fn get_emails(_token: &str, email: &str) -> Result<Vec<Email>, String> {
    let em = email.trim();
    if em.is_empty() {
        return Err("mailgolem: 邮箱地址为空".into());
    }

    block_on(async {
        /* 重新获取 session 和 CSRF token */
        let (cookies, csrf) = fetch_session().await?;

        /* POST /fetch-emails/{email} */
        let resp = http_client_no_cookie_jar()
            .post(format!(
                "{}/fetch-emails/{}",
                BASE_URL,
                urlencoding::encode(em)
            ))
            .header("Content-Type", "application/x-www-form-urlencoded")
            .header("X-Requested-With", "XMLHttpRequest")
            .header("Accept", "application/json, text/javascript, */*; q=0.01")
            .header("Accept-Language", "en-US,en;q=0.9")
            .header("User-Agent", browser_ua())
            .header("Referer", format!("{}/", BASE_URL))
            .header("Cookie", &cookies)
            .body(format!("_token={}", urlencoding::encode(&csrf)))
            .send()
            .await
            .map_err(|e| format!("mailgolem: 获取邮件列表失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!(
                "mailgolem: 获取邮件列表返回 HTTP {}",
                resp.status()
            ));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("mailgolem: 解析邮件列表响应失败: {}", e))?;

        /* 响应为 JSON 数组: [{id, from, subject}, ...] 或空数组 [] */
        let arr = match data.as_array() {
            Some(a) => a,
            None => return Ok(vec![]),
        };

        let mut result = Vec::with_capacity(arr.len());
        for item in arr {
            let id = item
                .get("id")
                .map(|v| {
                    if let Some(s) = v.as_str() {
                        s.to_string()
                    } else {
                        v.to_string()
                    }
                })
                .unwrap_or_default();
            let from = item
                .get("from")
                .and_then(|v| v.as_str())
                .unwrap_or("")
                .to_string();
            let subject = item
                .get("subject")
                .and_then(|v| v.as_str())
                .unwrap_or("")
                .to_string();

            let raw = serde_json::json!({
                "id": id,
                "from": from,
                "to": em,
                "subject": subject,
                "text": "",
                "html": "",
                "date": "",
                "isRead": false,
                "attachments": [],
            });
            result.push(normalize_email(&raw, em));
        }

        Ok(result)
    })
}
