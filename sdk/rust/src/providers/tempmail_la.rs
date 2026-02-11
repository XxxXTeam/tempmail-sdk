/*!
 * tempmail.la 渠道实现（支持分页）
 */

use reqwest::blocking::Client;
use serde_json::Value;
use crate::types::{Channel, EmailInfo, Email};
use crate::normalize::normalize_email;

const BASE_URL: &str = "https://tempmail.la/api";

fn client() -> Client {
    Client::builder()
        .user_agent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36 Edg/144.0.0.0")
        .timeout(std::time::Duration::from_secs(15))
        .build().unwrap()
}

pub fn generate_email() -> Result<EmailInfo, String> {
    let resp = client()
        .post(format!("{}/mail/create", BASE_URL))
        .header("Content-Type", "application/json")
        .header("accept-language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6")
        .header("cache-control", "no-cache")
        .header("dnt", "1")
        .header("locale", "zh-CN")
        .header("origin", "https://tempmail.la")
        .header("platform", "PC")
        .header("pragma", "no-cache")
        .header("product", "TEMP_MAIL")
        .header("referer", "https://tempmail.la/zh-CN/tempmail")
        .header("sec-ch-ua", "\"Not(A:Brand\";v=\"8\", \"Chromium\";v=\"144\", \"Microsoft Edge\";v=\"144\"")
        .header("sec-ch-ua-mobile", "?0")
        .header("sec-ch-ua-platform", "\"Windows\"")
        .header("sec-fetch-dest", "empty")
        .header("sec-fetch-mode", "cors")
        .header("sec-fetch-site", "same-origin")
        .json(&serde_json::json!({"turnstile": ""}))
        .send().map_err(|e| format!("tempmail-la request failed: {}", e))?;

    if !resp.status().is_success() {
        return Err(format!("tempmail-la generate failed: {}", resp.status()));
    }

    let data: Value = resp.json().map_err(|e| format!("parse failed: {}", e))?;
    if data["code"].as_i64() != Some(0) {
        return Err("Failed to generate email".into());
    }

    Ok(EmailInfo {
        channel: Channel::TempmailLa,
        email: data["data"]["address"].as_str().unwrap_or("").to_string(),
        token: None,
        expires_at: data["data"]["endAt"].as_i64(),
        created_at: data["data"]["startAt"].as_str().map(|s| s.to_string()),
    })
}

pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let mut all_emails = Vec::new();
    let mut cursor: Option<String> = None;

    loop {
        let resp = client()
            .post(format!("{}/mail/box", BASE_URL))
            .header("Content-Type", "application/json")
            .header("dnt", "1")
            .header("origin", "https://tempmail.la")
            .header("platform", "PC")
            .header("product", "TEMP_MAIL")
            .header("referer", "https://tempmail.la/zh-CN/tempmail")
            .header("sec-ch-ua", "\"Not(A:Brand\";v=\"8\", \"Chromium\";v=\"144\", \"Microsoft Edge\";v=\"144\"")
            .header("sec-ch-ua-mobile", "?0")
            .header("sec-ch-ua-platform", "\"Windows\"")
            .header("sec-fetch-dest", "empty")
            .header("sec-fetch-mode", "cors")
            .header("sec-fetch-site", "same-origin")
            .json(&serde_json::json!({"address": email, "cursor": cursor}))
            .send().map_err(|e| format!("tempmail-la request failed: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("tempmail-la get emails failed: {}", resp.status()));
        }

        let data: Value = resp.json().map_err(|e| format!("parse failed: {}", e))?;
        if data["code"].as_i64() != Some(0) {
            return Err("Failed to get emails".into());
        }

        if let Some(rows) = data["data"]["rows"].as_array() {
            for raw in rows {
                all_emails.push(normalize_email(raw, email));
            }
        }

        if data["data"]["hasMore"].as_bool() == Some(true) {
            if let Some(c) = data["data"]["cursor"].as_str() {
                cursor = Some(c.to_string());
                continue;
            }
        }
        break;
    }

    Ok(all_emails)
}
