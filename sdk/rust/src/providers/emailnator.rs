/*!
 * Emailnator 渠道实现
 * 网站: https://www.emailnator.com
 * 需要 XSRF-TOKEN Session 认证
 */

use crate::config::{block_on, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::Value;

const BASE_URL: &str = "https://www.emailnator.com";
const EMAIL_OPTIONS: &[&str] = &["plusGmail", "dotGmail", "googleMail"];

fn browser_ua() -> &'static str {
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36"
}

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
            .map_err(|e| format!("emailnator init session failed: {}", e))?;

        let mut xsrf_token = String::new();
        let mut cookie_parts: Vec<String> = Vec::new();

        for value in resp.headers().get_all("set-cookie") {
            let cookie_str = value.to_str().unwrap_or("");
            if let Some(name_value) = cookie_str.split(';').next() {
                cookie_parts.push(name_value.to_string());
                if name_value.starts_with("XSRF-TOKEN=") {
                    let raw = &name_value["XSRF-TOKEN=".len()..];
                    xsrf_token = urlencoding::decode(raw).unwrap_or_default().to_string();
                }
            }
        }

        if xsrf_token.is_empty() {
            return Err("Failed to extract XSRF-TOKEN".into());
        }

        Ok((xsrf_token, cookie_parts.join("; ")))
    })
}

fn post_json(xsrf_token: &str, cookies: &str, path: &str, body: Value) -> Result<String, String> {
    let xsrf_token = xsrf_token.to_string();
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
            .header("X-XSRF-TOKEN", xsrf_token)
            .header("Cookie", cookies)
            .json(&body)
            .send()
            .await
            .map_err(|e| format!("emailnator {} failed: {}", path, e))?;

        let status = resp.status();
        let text = resp
            .text()
            .await
            .map_err(|e| format!("emailnator {} read failed: {}", path, e))?;
        if !status.is_success() {
            return Err(format!("emailnator {} failed: {} {}", path, status, text));
        }
        Ok(text)
    })
}

pub fn generate_email() -> Result<EmailInfo, String> {
    let (xsrf_token, cookies) = init_session()?;
    let text = post_json(
        &xsrf_token,
        &cookies,
        "/generate-email",
        serde_json::json!({ "email": EMAIL_OPTIONS }),
    )?;
    let data: Value = serde_json::from_str(&text).map_err(|e| format!("parse failed: {}", e))?;
    let email_list = data["email"]
        .as_array()
        .ok_or("emailnator: empty email response")?;

    if email_list.is_empty() {
        return Err("emailnator: empty email list".into());
    }

    let email = email_list[0].as_str().unwrap_or("").to_string();
    let token_payload = serde_json::json!({
        "xsrfToken": xsrf_token,
        "cookies": cookies,
    })
    .to_string();

    Ok(EmailInfo {
        channel: Channel::Emailnator,
        email,
        token: Some(token_payload),
        expires_at: None,
        created_at: None,
    })
}

pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let session: Value =
        serde_json::from_str(token).map_err(|e| format!("parse session token failed: {}", e))?;

    let xsrf_token = session["xsrfToken"].as_str().unwrap_or("");
    let cookies = session["cookies"].as_str().unwrap_or("");

    let text = post_json(
        xsrf_token,
        cookies,
        "/message-list",
        serde_json::json!({ "email": email }),
    )?;
    let data: Value = serde_json::from_str(&text).map_err(|e| format!("parse failed: {}", e))?;
    let message_data = data["messageData"].as_array().cloned().unwrap_or_default();

    let mut result = Vec::new();
    for msg in &message_data {
        let msg_id = msg["messageID"].as_str().unwrap_or("");
        if msg_id.starts_with("ADS") {
            continue;
        }
        let html = if msg_id.is_empty() {
            String::new()
        } else {
            let detail = post_json(
                xsrf_token,
                cookies,
                "/message-list",
                serde_json::json!({ "email": email, "messageID": msg_id }),
            )
            .unwrap_or_default();
            serde_json::from_str::<String>(&detail).unwrap_or(detail)
        };
        let raw = serde_json::json!({
            "id": msg_id,
            "from": msg["from"].as_str().unwrap_or(""),
            "to": email,
            "subject": msg["subject"].as_str().unwrap_or(""),
            "text": "",
            "html": html,
            "date": msg["time"].as_str().unwrap_or(""),
            "isRead": false,
            "attachments": [],
        });
        result.push(normalize_email(&raw, email));
    }

    Ok(result)
}
