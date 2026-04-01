/*!
 * mail.cx 渠道实现（公开 REST API，见 https://docs.mail.cx）
 * GET /api/domains → POST /api/accounts（JWT）→ GET /api/messages
 */

use chrono::Utc;
use rand::Rng;
use serde_json::{json, Value};
use crate::types::{Channel, Email, EmailInfo};
use crate::normalize::normalize_email;
use crate::config::{http_client, block_on, get_current_ua};

const BASE: &str = "https://api.mail.cx";

fn random_string(len: usize) -> String {
    let chars: Vec<char> = "abcdefghijklmnopqrstuvwxyz0123456789".chars().collect();
    let mut rng = rand::thread_rng();
    (0..len).map(|_| chars[rng.gen_range(0..chars.len())]).collect()
}

fn get_domains() -> Result<Vec<String>, String> {
    block_on(async {
        let resp = http_client()
            .get(format!("{}/api/domains", BASE))
            .header("Accept", "application/json")
            .header("Origin", "https://mail.cx")
            .header("Referer", "https://mail.cx/")
            .header("User-Agent", get_current_ua())
            .send()
            .await
            .map_err(|e| format!("mail-cx domains: {}", e))?;
        if !resp.status().is_success() {
            return Err(format!("mail-cx domains: {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| format!("parse: {}", e))?;
        let arr = data["domains"].as_array().cloned().unwrap_or_default();
        Ok(arr
            .into_iter()
            .filter_map(|d| d["domain"].as_str().map(|s| s.to_string()))
            .collect())
    })
}

fn flatten_message(msg: &Value, recipient: &str) -> Value {
    let id = msg["id"].as_str().unwrap_or("");
    let attachments: Value = match msg["attachments"].as_array() {
        Some(arr) => {
            let mapped: Vec<Value> = arr
                .iter()
                .filter_map(|a| {
                    let idx = a["index"].as_u64().or_else(|| a["index"].as_i64().map(|i| i as u64))?;
                    Some(json!({
                        "filename": a["filename"].as_str().unwrap_or(""),
                        "size": a["size"],
                        "content_type": a["content_type"].as_str(),
                        "url": format!("{}/api/messages/{}/attachments/{}", BASE, id, idx),
                    }))
                })
                .collect();
            Value::Array(mapped)
        }
        None => Value::Array(vec![]),
    };
    json!({
        "id": id,
        "sender": msg["sender"].as_str().unwrap_or(""),
        "from": msg["from"].as_str().unwrap_or(""),
        "address": msg["address"].as_str().unwrap_or(recipient),
        "subject": msg["subject"].as_str().unwrap_or(""),
        "preview_text": msg["preview_text"],
        "text_body": msg["text_body"],
        "html_body": msg["html_body"],
        "created_at": msg["created_at"],
        "attachments": attachments,
    })
}

pub fn generate_email(preferred_domain: Option<&str>) -> Result<EmailInfo, String> {
    let mut domains = get_domains()?;
    if domains.is_empty() {
        return Err("No available domains".into());
    }
    if let Some(want) = preferred_domain {
        let w = want.trim_start_matches('@').to_lowercase();
        let filtered: Vec<String> = domains
            .iter()
            .filter(|d| d.to_lowercase() == w)
            .cloned()
            .collect();
        if !filtered.is_empty() {
            domains = filtered;
        }
    }

    let mut last_err: Option<String> = None;
    for _ in 0..8 {
        let mut rng = rand::thread_rng();
        let domain = &domains[rng.gen_range(0..domains.len())];
        let address = format!("{}@{}", random_string(12), domain);
        let password = random_string(16);

        let res = block_on(async {
            let resp = http_client()
                .post(format!("{}/api/accounts", BASE))
                .header("Content-Type", "application/json")
                .header("Accept", "application/json")
                .header("Origin", "https://mail.cx")
                .header("Referer", "https://mail.cx/")
                .header("User-Agent", get_current_ua())
                .json(&json!({ "address": &address, "password": &password }))
                .send()
                .await
                .map_err(|e| format!("mail-cx create: {}", e))?;

            let status = resp.status();
            let text = resp.text().await.unwrap_or_default();
            if status.as_u16() != 201 {
                return Err(format!("mail-cx create account: {} {}", status, text));
            }
            let data: Value = serde_json::from_str(&text).map_err(|e| format!("parse: {}", e))?;
            let token = data["token"].as_str().ok_or("mail-cx: no token")?.to_string();
            let em = data["address"].as_str().ok_or("mail-cx: no address")?.to_string();
            Ok(EmailInfo {
                channel: Channel::MailCx,
                email: em,
                token: Some(token),
                expires_at: None,
                created_at: Some(Utc::now().to_rfc3339()),
            })
        });

        match res {
            Ok(info) => return Ok(info),
            Err(e) => {
                if e.contains("409") {
                    last_err = Some(e);
                    continue;
                }
                return Err(e);
            }
        }
    }
    Err(last_err.unwrap_or_else(|| "mail-cx: could not create account".into()))
}

pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let token = token.to_string();
    let email = email.to_string();
    block_on(async {
        let resp = http_client()
            .get(format!("{}/api/messages?page=1", BASE))
            .header("Accept", "application/json")
            .header("Origin", "https://mail.cx")
            .header("Referer", "https://mail.cx/")
            .header("User-Agent", get_current_ua())
            .header("Authorization", format!("Bearer {}", token))
            .send()
            .await
            .map_err(|e| format!("mail-cx list: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("mail-cx list messages: {}", resp.status()));
        }

        let data: Value = resp.json().await.map_err(|e| format!("parse: {}", e))?;
        let messages = data["messages"].as_array().cloned().unwrap_or_default();
        let mut result = Vec::new();

        for msg in &messages {
            let id = msg["id"].as_str().unwrap_or("");
            let detail_json = if !id.is_empty() {
                match http_client()
                    .get(format!("{}/api/messages/{}", BASE, id))
                    .header("Accept", "application/json")
                    .header("Origin", "https://mail.cx")
                    .header("Referer", "https://mail.cx/")
                    .header("User-Agent", get_current_ua())
                    .header("Authorization", format!("Bearer {}", token))
                    .send()
                    .await
                {
                    Ok(r) if r.status().is_success() => r.json::<Value>().await.ok(),
                    _ => None,
                }
            } else {
                None
            };

            let src = detail_json.as_ref().unwrap_or(msg);
            let flat = flatten_message(src, &email);
            result.push(normalize_email(&flat, &email));
        }

        Ok(result)
    })
}
