/*!
 * mail.tm 渠道实现
 * 流程: 获取域名 → 生成随机邮箱/密码 → 创建账号 → 获取 Bearer Token
 */

use serde_json::Value;
use rand::Rng;
use crate::types::{Channel, EmailInfo, Email};
use crate::normalize::normalize_email;
use crate::config::http_client;

const BASE_URL: &str = "https://api.mail.tm";

fn random_string(len: usize) -> String {
    let chars: Vec<char> = "abcdefghijklmnopqrstuvwxyz0123456789".chars().collect();
    let mut rng = rand::thread_rng();
    (0..len).map(|_| chars[rng.gen_range(0..chars.len())]).collect()
}

fn get_domains() -> Result<Vec<String>, String> {
    let resp = http_client()
        .get(format!("{}/domains", BASE_URL))
        .header("Accept", "application/json")
        .send().map_err(|e| format!("mail-tm domains failed: {}", e))?;

    let data: Value = resp.json().map_err(|e| format!("parse failed: {}", e))?;
    let members = if data.is_array() { data } else {
        data.get("hydra:member").cloned().unwrap_or(Value::Array(vec![]))
    };

    Ok(members.as_array().unwrap_or(&vec![]).iter()
        .filter(|d| d["isActive"].as_bool() == Some(true))
        .filter_map(|d| d["domain"].as_str().map(|s| s.to_string()))
        .collect())
}

fn flatten_message(msg: &Value, recipient: &str) -> Value {
    let from = msg.get("from")
        .and_then(|f| if f.is_object() { f["address"].as_str() } else { f.as_str() })
        .unwrap_or("");

    let to = msg.get("to")
        .and_then(|t| t.as_array())
        .and_then(|arr| arr.first())
        .and_then(|t| if t.is_object() { t["address"].as_str() } else { t.as_str() })
        .unwrap_or(recipient);

    let html = match msg.get("html") {
        Some(Value::Array(arr)) => arr.iter().filter_map(|v| v.as_str()).collect::<Vec<_>>().join(""),
        Some(Value::String(s)) => s.clone(),
        _ => String::new(),
    };

    serde_json::json!({
        "id": msg["id"].as_str().unwrap_or(""),
        "from": from,
        "to": to,
        "subject": msg["subject"].as_str().unwrap_or(""),
        "text": msg["text"].as_str().unwrap_or(""),
        "html": html,
        "createdAt": msg["createdAt"].as_str().unwrap_or(""),
        "seen": msg["seen"].as_bool().unwrap_or(false),
    })
}

pub fn generate_email() -> Result<EmailInfo, String> {
    let domains = get_domains()?;
    if domains.is_empty() {
        return Err("No available domains".into());
    }

    let mut rng = rand::thread_rng();
    let domain = &domains[rng.gen_range(0..domains.len())];
    let username = random_string(12);
    let address = format!("{}@{}", username, domain);
    let password = random_string(16);

    // 创建账号
    let resp = http_client()
        .post(format!("{}/accounts", BASE_URL))
        .header("Content-Type", "application/ld+json")
        .json(&serde_json::json!({"address": &address, "password": &password}))
        .send().map_err(|e| format!("mail-tm create account failed: {}", e))?;

    if !resp.status().is_success() {
        let text = resp.text().unwrap_or_default();
        return Err(format!("mail-tm create account failed: {}", text));
    }

    let account: Value = resp.json().map_err(|e| format!("parse failed: {}", e))?;

    // 获取 token
    let resp = http_client()
        .post(format!("{}/token", BASE_URL))
        .header("Content-Type", "application/json")
        .json(&serde_json::json!({"address": &address, "password": &password}))
        .send().map_err(|e| format!("mail-tm get token failed: {}", e))?;

    if !resp.status().is_success() {
        return Err(format!("mail-tm get token failed: {}", resp.status()));
    }

    let token_data: Value = resp.json().map_err(|e| format!("parse failed: {}", e))?;

    Ok(EmailInfo {
        channel: Channel::MailTm,
        email: address,
        token: token_data["token"].as_str().map(|s| s.to_string()),
        expires_at: None,
        created_at: account["createdAt"].as_str().map(|s| s.to_string()),
    })
}

pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let resp = http_client()
        .get(format!("{}/messages", BASE_URL))
        .header("Accept", "application/json")
        .header("Authorization", format!("Bearer {}", token))
        .send().map_err(|e| format!("mail-tm request failed: {}", e))?;

    if !resp.status().is_success() {
        return Err(format!("mail-tm get emails failed: {}", resp.status()));
    }

    let data: Value = resp.json().map_err(|e| format!("parse failed: {}", e))?;
    let messages = if data.is_array() { data.as_array().cloned().unwrap_or_default() }
    else { data.get("hydra:member").and_then(|v| v.as_array()).cloned().unwrap_or_default() };

    let mut result = Vec::new();
    for msg in &messages {
        let detail = http_client()
            .get(format!("{}/messages/{}", BASE_URL, msg["id"].as_str().unwrap_or("")))
            .header("Accept", "application/json")
            .header("Authorization", format!("Bearer {}", token))
            .send().ok()
            .and_then(|r| if r.status().is_success() { r.json::<Value>().ok() } else { None });

        let flat = flatten_message(detail.as_ref().unwrap_or(msg), email);
        result.push(normalize_email(&flat, email));
    }

    Ok(result)
}
