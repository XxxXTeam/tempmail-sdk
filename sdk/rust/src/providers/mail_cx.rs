/*!
 * mail.cx 渠道实现
 * API 文档: https://api.mail.cx/
 * 无需注册，任意邮箱名即可接收邮件
 */

use serde_json::Value;
use rand::Rng;
use crate::types::{Channel, EmailInfo, Email};
use crate::normalize::normalize_email;
use crate::config::http_client;

const BASE_URL: &str = "https://api.mail.cx/api/v1";
const DOMAINS: &[&str] = &["qabq.com", "nqmo.com"];

fn random_username(len: usize) -> String {
    let chars: Vec<char> = "abcdefghijklmnopqrstuvwxyz0123456789".chars().collect();
    let mut rng = rand::thread_rng();
    (0..len).map(|_| chars[rng.gen_range(0..chars.len())]).collect()
}

fn get_token() -> Result<String, String> {
    let resp = http_client()
        .post(format!("{}/auth/authorize_token", BASE_URL))
        .header("Content-Type", "application/json")
        .header("Accept", "application/json")
        .body("{}")
        .send().map_err(|e| format!("mail-cx get token failed: {}", e))?;

    if !resp.status().is_success() {
        return Err(format!("mail-cx get token failed: {}", resp.status()));
    }

    let text = resp.text().map_err(|e| format!("read body failed: {}", e))?;
    Ok(text.trim().trim_matches('"').to_string())
}

pub fn generate_email() -> Result<EmailInfo, String> {
    let token = get_token()?;
    let mut rng = rand::thread_rng();
    let domain = DOMAINS[rng.gen_range(0..DOMAINS.len())];
    let username = random_username(8);
    let email = format!("{}@{}", username, domain);

    Ok(EmailInfo {
        channel: Channel::MailCx,
        email,
        token: Some(token),
        expires_at: None,
        created_at: None,
    })
}

pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let encoded = urlencoding::encode(email);
    let resp = http_client()
        .get(format!("{}/mailbox/{}", BASE_URL, encoded))
        .header("Accept", "application/json")
        .header("Authorization", format!("Bearer {}", token))
        .send().map_err(|e| format!("mail-cx request failed: {}", e))?;

    if !resp.status().is_success() {
        return Err(format!("mail-cx get emails failed: {}", resp.status()));
    }

    let data: Value = resp.json().map_err(|e| format!("parse failed: {}", e))?;
    let messages = data.as_array().cloned().unwrap_or_default();

    if messages.is_empty() {
        return Ok(vec![]);
    }

    let mut result = Vec::new();
    for msg in &messages {
        let detail = http_client()
            .get(format!("{}/mailbox/{}/{}", BASE_URL, encoded, msg["id"].as_str().unwrap_or("")))
            .header("Accept", "application/json")
            .header("Authorization", format!("Bearer {}", token))
            .send().ok()
            .and_then(|r| if r.status().is_success() { r.json::<Value>().ok() } else { None });

        result.push(normalize_email(detail.as_ref().unwrap_or(msg), email));
    }

    Ok(result)
}
