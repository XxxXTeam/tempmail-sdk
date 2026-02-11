/*!
 * awamail.com 渠道实现
 * 需要保存 session cookie
 */

use serde_json::Value;
use crate::types::{Channel, EmailInfo, Email};
use crate::normalize::normalize_email;
use crate::config::http_client;

const BASE_URL: &str = "https://awamail.com/welcome";

pub fn generate_email() -> Result<EmailInfo, String> {
    let c = http_client();
    let resp = c
        .post(format!("{}/change_mailbox", BASE_URL))
        .header("Content-Length", "0")
        .header("accept-language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6")
        .header("cache-control", "no-cache")
        .header("dnt", "1")
        .header("origin", "https://awamail.com")
        .header("pragma", "no-cache")
        .header("referer", "https://awamail.com/?lang=zh")
        .header("sec-ch-ua", "\"Not(A:Brand\";v=\"8\", \"Chromium\";v=\"144\", \"Microsoft Edge\";v=\"144\"")
        .header("sec-ch-ua-mobile", "?0")
        .header("sec-ch-ua-platform", "\"Windows\"")
        .header("sec-fetch-dest", "empty")
        .header("sec-fetch-mode", "cors")
        .header("sec-fetch-site", "same-origin")
        .send().map_err(|e| format!("awamail request failed: {}", e))?;

    if !resp.status().is_success() {
        return Err(format!("awamail generate failed: {}", resp.status()));
    }

    // 从 Set-Cookie 提取 session
    let session_cookie = resp.headers().get_all("set-cookie")
        .iter()
        .filter_map(|v| v.to_str().ok())
        .find(|s| s.contains("awamail_session="))
        .and_then(|s| {
            s.split(';').next()
                .filter(|p| p.starts_with("awamail_session="))
                .map(|p| p.to_string())
        })
        .ok_or("Failed to extract session cookie")?;

    let data: Value = resp.json().map_err(|e| format!("parse failed: {}", e))?;
    if !data["success"].as_bool().unwrap_or(false) {
        return Err("Failed to generate email".into());
    }

    Ok(EmailInfo {
        channel: Channel::Awamail,
        email: data["data"]["email_address"].as_str().unwrap_or("").to_string(),
        token: Some(session_cookie),
        expires_at: data["data"]["expired_at"].as_i64(),
        created_at: data["data"]["created_at"].as_str().map(|s| s.to_string()),
    })
}

pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let resp = http_client()
        .get(format!("{}/get_emails", BASE_URL))
        .header("Cookie", token)
        .header("x-requested-with", "XMLHttpRequest")
        .header("dnt", "1")
        .header("origin", "https://awamail.com")
        .header("referer", "https://awamail.com/?lang=zh")
        .header("sec-fetch-dest", "empty")
        .header("sec-fetch-mode", "cors")
        .header("sec-fetch-site", "same-origin")
        .send().map_err(|e| format!("awamail request failed: {}", e))?;

    if !resp.status().is_success() {
        return Err(format!("awamail get emails failed: {}", resp.status()));
    }

    let data: Value = resp.json().map_err(|e| format!("parse failed: {}", e))?;
    if !data["success"].as_bool().unwrap_or(false) {
        return Err("Failed to get emails".into());
    }

    Ok(data["data"]["emails"].as_array()
        .map(|arr| arr.iter().map(|raw| normalize_email(raw, email)).collect())
        .unwrap_or_default())
}
