/*!
 * 1SecMail -- https://tmaily.com
 */

use crate::config::{block_on, http_client_no_cookie_jar};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde::Deserialize;
use serde_json::Value;

const BASE_URL: &str = "https://tmaily.com/";

#[derive(Debug, Deserialize)]
struct GenerateResponse {
    address: Option<String>,
}

fn extract_cookie(headers: &wreq::header::HeaderMap) -> String {
    for value in headers.get_all("set-cookie").iter() {
        let Ok(line) = value.to_str() else { continue };
        let first = line.split(';').next().unwrap_or("").trim();
        if first.starts_with("TMaily_sid=") {
            return first.to_string();
        }
    }
    String::new()
}

pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let resp = http_client_no_cookie_jar()
            .get(format!("{}generate", BASE_URL))
            .header("Accept", "application/json")
            .header("User-Agent", "Mozilla/5.0")
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("1sec-mail generate: {}", resp.status()));
        }
        let cookie = extract_cookie(resp.headers());
        if cookie.is_empty() {
            return Err("1sec-mail: 未获取到会话 Cookie".into());
        }
        let data: GenerateResponse = resp.json().await.map_err(|e| e.to_string())?;
        let email = data.address.unwrap_or_default().trim().to_string();
        if email.is_empty() || !email.contains('@') {
            return Err("1sec-mail: 无效的邮箱响应".into());
        }
        Ok(EmailInfo {
            channel: Channel::OneSecMail,
            email,
            token: Some(cookie),
            expires_at: None,
            created_at: None,
        })
    })
}

pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let address = email.trim();
    if address.is_empty() {
        return Err("1sec-mail: 邮箱地址为空".into());
    }
    block_on(async {
        let resp = http_client_no_cookie_jar()
            .get(format!("{}emails?address={}", BASE_URL, address))
            .header("Accept", "application/json")
            .header("Cookie", token)
            .header("User-Agent", "Mozilla/5.0")
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("1sec-mail emails: {}", resp.status()));
        }
        let rows: Vec<Value> = resp.json().await.map_err(|e| e.to_string())?;
        Ok(rows
            .iter()
            .map(|row| normalize_email(row, address))
            .collect())
    })
}
