/*!
 * TempGBox 渠道实现
 * API: https://tempgbox.net/api/proxy
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use base64::{engine::general_purpose, Engine as _};
use rand::Rng;
use serde_json::Value;

const API_URL: &str = "https://tempgbox.net/api/proxy";

fn random_device_id() -> String {
    // 上游按 X-Device-ID 限制连续生成；每个新邮箱使用新的设备 ID，后续收件复用该邮箱 token。
    let mut bytes = [0u8; 32];
    rand::thread_rng().fill(&mut bytes);
    hex::encode(bytes)
}

fn random_ip() -> String {
    let mut rng = rand::thread_rng();
    format!(
        "{}.{}.{}.{}",
        rng.gen_range(1..=254),
        rng.gen_range(1..=254),
        rng.gen_range(1..=254),
        rng.gen_range(1..=254)
    )
}

fn decode_payload(html: &str) -> Result<Value, String> {
    let start_marker = if html.contains("data-x=\"") {
        "data-x=\""
    } else {
        "data-x='"
    };
    let quote = if start_marker.ends_with('"') {
        '"'
    } else {
        '\''
    };
    let start = html
        .find(start_marker)
        .ok_or_else(|| "tempgbox: missing encoded response payload".to_string())?
        + start_marker.len();
    let tail = &html[start..];
    let end = tail
        .find(quote)
        .ok_or_else(|| "tempgbox: malformed encoded response payload".to_string())?;
    let raw = general_purpose::STANDARD
        .decode(&tail[..end])
        .map_err(|e| format!("tempgbox: decode payload failed: {}", e))?;
    serde_json::from_slice(&raw).map_err(|e| format!("tempgbox: parse payload failed: {}", e))
}

fn post_proxy(route: &str, device_id: &str, body: Value) -> Result<Value, String> {
    let device_id = device_id.to_string();
    let route = route.to_string();
    block_on(async move {
        let ip = random_ip();
        let resp = http_client()
            .post(format!("{}?route={}", API_URL, route))
            .header("Accept", "text/html,application/json")
            .header("Content-Type", "application/json")
            .header("Origin", "https://tempgbox.net")
            .header("Referer", "https://tempgbox.net/")
            .header("User-Agent", get_current_ua())
            .header("X-Device-ID", &device_id)
            .header("X-Forwarded-For", &ip)
            .header("X-Real-IP", &ip)
            .header("X-Originating-IP", &ip)
            .json(&body)
            .send()
            .await
            .map_err(|e| format!("tempgbox {} request failed: {}", route, e))?;

        let status = resp.status();
        let text = resp
            .text()
            .await
            .map_err(|e| format!("tempgbox {} read response failed: {}", route, e))?;
        let payload = decode_payload(&text)?;
        if !status.is_success() {
            let reason = payload["detail"]
                .as_str()
                .or_else(|| payload["error"].as_str())
                .or_else(|| payload["message"].as_str())
                .unwrap_or("");
            return Err(format!("tempgbox {} failed: {} {}", route, status, reason));
        }
        Ok(payload)
    })
}

pub fn generate_email() -> Result<EmailInfo, String> {
    let device_id = random_device_id();
    let data = post_proxy(
        "generate",
        &device_id,
        serde_json::json!({ "variant": "googlemail" }),
    )?;
    let alias = data
        .get("alias")
        .ok_or_else(|| "tempgbox: missing alias".to_string())?;
    let email = alias["email"]
        .as_str()
        .or_else(|| alias["alias"].as_str())
        .unwrap_or("");
    if email.is_empty() {
        return Err("tempgbox: missing email".into());
    }

    Ok(EmailInfo {
        channel: Channel::Tempgbox,
        email: email.to_string(),
        token: Some(device_id),
        expires_at: None,
        created_at: alias["created_at"].as_str().map(|s| s.to_string()),
    })
}

pub fn get_emails(device_id: &str, email: &str) -> Result<Vec<Email>, String> {
    if device_id.is_empty() {
        return Err("tempgbox: missing device id".into());
    }
    let data = post_proxy("inbox", device_id, serde_json::json!({ "email": email }))?;
    let messages = data["messages"].as_array().cloned().unwrap_or_default();
    let mut out = Vec::with_capacity(messages.len());
    for raw in messages {
        let flat = serde_json::json!({
            "id": raw["id"].as_str().or_else(|| raw["message_id"].as_str()).unwrap_or(""),
            "from": raw["from"].as_str().or_else(|| raw["sender"].as_str()).unwrap_or(""),
            "to": email,
            "subject": raw["subject"].as_str().unwrap_or(""),
            "text": raw["text"].as_str().or_else(|| raw["body_text"].as_str()).unwrap_or(""),
            "html": raw["html"].as_str().or_else(|| raw["body_html"].as_str()).unwrap_or(""),
            "date": raw["date"].as_str().or_else(|| raw["received_at"].as_str()).unwrap_or(""),
            "attachments": raw["attachments"].as_array().cloned().or_else(|| raw["attachments_info"].as_array().cloned()).unwrap_or_default(),
        });
        out.push(normalize_email(&flat, email));
    }
    Ok(out)
}
