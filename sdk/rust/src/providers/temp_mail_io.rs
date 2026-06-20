/*!
 * temp-mail.io -- https://temp-mail.io
 * REST API: https://api.internal.temp-mail.io/api/v3
 */

use crate::config::{block_on, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use regex::Regex;
use serde_json::Value;
use std::sync::{LazyLock, Mutex};

const BASE_URL: &str = "https://api.internal.temp-mail.io/api/v3";
const PAGE_URL: &str = "https://temp-mail.io/en";

static CORS_HEADER_RE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r#"mobileTestingHeader\s*:\s*"([^"]+)""#).unwrap());
static CACHED_CORS_HEADER: Mutex<Option<String>> = Mutex::new(None);

async fn fetch_cors_header() -> String {
    if let Some(value) = CACHED_CORS_HEADER.lock().unwrap().clone() {
        return value;
    }

    let value = match http_client()
        .get(PAGE_URL)
        .header("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36")
        .send()
        .await
    {
        Ok(resp) => match resp.text().await {
            Ok(html) => CORS_HEADER_RE
                .captures(&html)
                .and_then(|cap| cap.get(1).map(|m| m.as_str().to_string()))
                .unwrap_or_else(|| "1".to_string()),
            Err(_) => "1".to_string(),
        },
        Err(_) => "1".to_string(),
    };

    *CACHED_CORS_HEADER.lock().unwrap() = Some(value.clone());
    value
}

fn apply_api_headers(req: wreq::RequestBuilder, cors_header: &str) -> wreq::RequestBuilder {
    req.header("Content-Type", "application/json")
        .header("Application-Name", "web")
        .header("Application-Version", "4.0.0")
        .header("X-CORS-Header", cors_header)
        .header("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36 Edg/144.0.0.0")
        .header("Origin", "https://temp-mail.io")
        .header("Referer", "https://temp-mail.io/")
}

pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let cors_header = fetch_cors_header().await;
        let resp = apply_api_headers(
            http_client().post(format!("{}/email/new", BASE_URL)),
            &cors_header,
        )
        .json(&serde_json::json!({
            "min_name_length": 10,
            "max_name_length": 10,
        }))
        .send()
        .await
        .map_err(|e| e.to_string())?;

        if !resp.status().is_success() {
            return Err(format!("temp-mail-io generate: {}", resp.status()));
        }

        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let email = data
            .get("email")
            .and_then(|x| x.as_str())
            .unwrap_or("")
            .to_string();
        let token = data
            .get("token")
            .and_then(|x| x.as_str())
            .unwrap_or("")
            .to_string();
        if email.is_empty() || token.is_empty() {
            return Err("temp-mail-io: invalid generate response".into());
        }

        Ok(EmailInfo {
            channel: Channel::TempMailIo,
            email,
            token: Some(token),
            expires_at: None,
            created_at: None,
        })
    })
}

pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    block_on(async {
        let cors_header = fetch_cors_header().await;
        let resp = apply_api_headers(
            http_client().get(format!("{}/email/{}/messages", BASE_URL, email)),
            &cors_header,
        )
        .send()
        .await
        .map_err(|e| e.to_string())?;

        if !resp.status().is_success() {
            return Err(format!("temp-mail-io messages: {}", resp.status()));
        }

        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let rows = data.as_array().cloned().unwrap_or_default();
        Ok(rows.iter().map(|raw| normalize_email(raw, email)).collect())
    })
}
