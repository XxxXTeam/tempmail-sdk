/*!
 * DuckMail — https://api.duckmail.sbs
 * 流程: 获取域名 → 创建账号 → 获取 Bearer Token → 拉取消息详情
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use rand::Rng;
use serde_json::{json, Value};

const BASE_URL: &str = "https://api.duckmail.sbs";

fn random_string(len: usize) -> String {
    const CHARS: &[u8] = b"abcdefghijklmnopqrstuvwxyz0123456789";
    let mut rng = rand::thread_rng();
    (0..len)
        .map(|_| CHARS[rng.gen_range(0..CHARS.len())] as char)
        .collect()
}

fn default_headers(b: wreq::RequestBuilder) -> wreq::RequestBuilder {
    b.header("Accept", "application/json")
        .header(
            "Accept-Language",
            "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
        )
        .header("Cache-Control", "no-cache")
        .header("Pragma", "no-cache")
        .header("Origin", "https://duckmail.sbs")
        .header("Referer", "https://duckmail.sbs/")
        .header("User-Agent", get_current_ua())
}

fn get_domains() -> Result<Vec<String>, String> {
    block_on(async {
        let resp = default_headers(http_client().get(format!("{}/domains?page=1", BASE_URL)))
            .send()
            .await
            .map_err(|e| format!("duckmail domains failed: {}", e))?;
        if !resp.status().is_success() {
            return Err(format!("duckmail domains failed: {}", resp.status()));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("parse failed: {}", e))?;
        let members = if let Some(arr) = data.as_array() {
            arr.clone()
        } else {
            data.get("hydra:member")
                .and_then(|v| v.as_array())
                .cloned()
                .unwrap_or_default()
        };

        Ok(members
            .iter()
            .filter(|d| d.get("isActive").and_then(|v| v.as_bool()) == Some(true))
            .filter_map(|d| d.get("domain").and_then(|v| v.as_str()))
            .map(|s| s.to_string())
            .collect())
    })
}

fn create_account(address: &str, password: &str) -> Result<Value, String> {
    let address = address.to_string();
    let password = password.to_string();
    block_on(async move {
        let resp = default_headers(http_client().post(format!("{}/accounts", BASE_URL)))
            .header("Content-Type", "application/ld+json")
            .json(&json!({"address": address, "password": password}))
            .send()
            .await
            .map_err(|e| format!("duckmail create account failed: {}", e))?;

        let status = resp.status();
        if !status.is_success() {
            let text = resp.text().await.unwrap_or_default();
            return Err(format!(
                "duckmail create account failed: {} {}",
                status, text
            ));
        }

        resp.json::<Value>()
            .await
            .map_err(|e| format!("parse failed: {}", e))
    })
}

fn get_token(address: &str, password: &str) -> Result<String, String> {
    let address = address.to_string();
    let password = password.to_string();
    block_on(async move {
        let resp = default_headers(http_client().post(format!("{}/token", BASE_URL)))
            .header("Content-Type", "application/json")
            .json(&json!({"address": address, "password": password}))
            .send()
            .await
            .map_err(|e| format!("duckmail get token failed: {}", e))?;

        let status = resp.status();
        if !status.is_success() {
            let text = resp.text().await.unwrap_or_default();
            return Err(format!("duckmail get token failed: {} {}", status, text));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("parse failed: {}", e))?;
        data.get("token")
            .and_then(|v| v.as_str())
            .map(|s| s.to_string())
            .ok_or_else(|| "duckmail token missing".to_string())
    })
}

fn flatten_message(msg: &Value, recipient_email: &str) -> Value {
    let from = msg
        .get("from")
        .and_then(|f| {
            if f.is_object() {
                f.get("address").and_then(|v| v.as_str())
            } else {
                f.as_str()
            }
        })
        .unwrap_or("");

    let to = msg
        .get("to")
        .and_then(|v| v.as_array())
        .and_then(|arr| arr.first())
        .and_then(|item| {
            if item.is_object() {
                item.get("address").and_then(|v| v.as_str())
            } else {
                item.as_str()
            }
        })
        .unwrap_or(recipient_email);

    let html = match msg.get("html") {
        Some(Value::Array(arr)) => arr
            .iter()
            .filter_map(|v| v.as_str())
            .collect::<Vec<_>>()
            .join(""),
        Some(Value::String(s)) => s.clone(),
        _ => String::new(),
    };

    let attachments = msg
        .get("attachments")
        .and_then(|v| v.as_array())
        .map(|arr| {
            arr.iter()
                .filter_map(|item| {
                    let obj = item.as_object()?;
                    let download_url = obj
                        .get("downloadUrl")
                        .and_then(|v| v.as_str())
                        .filter(|s| !s.trim().is_empty())
                        .map(|s| {
                            if s.starts_with("http://") || s.starts_with("https://") {
                                s.to_string()
                            } else {
                                format!("{}{}", BASE_URL, s)
                            }
                        });
                    Some(json!({
                        "filename": obj.get("filename").cloned().unwrap_or_else(|| Value::String(String::new())),
                        "size": obj.get("size").cloned().unwrap_or(Value::Null),
                        "contentType": obj.get("contentType").cloned().unwrap_or(Value::Null),
                        "downloadUrl": download_url,
                    }))
                })
                .collect::<Vec<_>>()
        })
        .unwrap_or_default();

    json!({
        "id": msg.get("id").cloned().unwrap_or_else(|| Value::String(String::new())),
        "from": from,
        "to": to,
        "subject": msg.get("subject").cloned().unwrap_or_else(|| Value::String(String::new())),
        "text": msg.get("text").cloned().unwrap_or_else(|| Value::String(String::new())),
        "html": html,
        "createdAt": msg.get("createdAt").cloned().unwrap_or_else(|| Value::String(String::new())),
        "seen": msg.get("seen").cloned().unwrap_or(Value::Bool(false)),
        "attachments": attachments,
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

    let account = create_account(&address, &password)?;
    let token = get_token(&address, &password)?;

    Ok(EmailInfo {
        channel: Channel::Duckmail,
        email: address,
        token: Some(token),
        expires_at: None,
        created_at: account
            .get("createdAt")
            .and_then(|v| v.as_str())
            .map(|s| s.to_string()),
    })
}

pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let token = token.to_string();
    let email = email.to_string();
    block_on(async move {
        let resp = default_headers(http_client().get(format!("{}/messages?page=1", BASE_URL)))
            .header("Authorization", format!("Bearer {}", token))
            .send()
            .await
            .map_err(|e| format!("duckmail request failed: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("duckmail get emails failed: {}", resp.status()));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("parse failed: {}", e))?;
        let messages = if let Some(arr) = data.as_array() {
            arr.clone()
        } else {
            data.get("hydra:member")
                .and_then(|v| v.as_array())
                .cloned()
                .unwrap_or_default()
        };

        if messages.is_empty() {
            return Ok(vec![]);
        }

        let mut result = Vec::new();
        for msg in &messages {
            let message_id = msg.get("id").and_then(|v| v.as_str()).unwrap_or("");
            let detail = if message_id.is_empty() {
                None
            } else {
                let detail_resp = default_headers(
                    http_client().get(format!("{}/messages/{}", BASE_URL, message_id)),
                )
                .header("Authorization", format!("Bearer {}", token))
                .send()
                .await
                .ok()
                .and_then(|r| {
                    if r.status().is_success() {
                        Some(r)
                    } else {
                        None
                    }
                });

                if let Some(resp) = detail_resp {
                    resp.json::<Value>().await.ok()
                } else {
                    None
                }
            };

            let flat = flatten_message(detail.as_ref().unwrap_or(msg), &email);
            result.push(normalize_email(&flat, &email));
        }

        Ok(result)
    })
}
