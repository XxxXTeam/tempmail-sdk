/*!
 * MFFAC — https://www.mffac.com/api
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::json;
use serde_json::Value;

const BASE: &str = "https://www.mffac.com/api";

fn mffac_headers(b: wreq::RequestBuilder, with_json_ct: bool) -> wreq::RequestBuilder {
    let mut b = b
        .header("User-Agent", get_current_ua())
        .header("Accept", "*/*")
        .header("Origin", "https://www.mffac.com")
        .header("Referer", "https://www.mffac.com/")
        .header(
            "sec-ch-ua",
            r#""Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146""#,
        )
        .header("sec-ch-ua-mobile", "?0")
        .header("sec-ch-ua-platform", r#""Windows""#)
        .header("sec-fetch-dest", "empty")
        .header("sec-fetch-mode", "cors")
        .header("sec-fetch-site", "same-origin");
    if with_json_ct {
        b = b.header("Content-Type", "application/json");
    }
    b
}

pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let resp = mffac_headers(
            http_client()
                .post(format!("{}/mailboxes", BASE))
                .body(r#"{"expiresInHours":24}"#.to_string()),
            true,
        )
        .send()
        .await
        .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("mffac generate {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        if data.get("success").and_then(|x| x.as_bool()) != Some(true) {
            return Err("mffac: create failed".into());
        }
        let mb = data
            .get("mailbox")
            .ok_or_else(|| "mffac: no mailbox".to_string())?;
        let addr = mb
            .get("address")
            .and_then(|x| x.as_str())
            .unwrap_or("")
            .trim();
        let id = mb.get("id").and_then(|x| x.as_str()).unwrap_or("").trim();
        if addr.is_empty() || id.is_empty() {
            return Err("mffac: bad mailbox".into());
        }
        let email = format!("{}@mffac.com", addr);
        let expires_at = mb
            .get("expiresAt")
            .and_then(|v| v.as_i64().or_else(|| v.as_f64().map(|f| f as i64)));
        let created_at = mb
            .get("createdAt")
            .and_then(|x| x.as_str())
            .map(|s| s.to_string());
        Ok(EmailInfo {
            channel: Channel::Mffac,
            email,
            token: Some(id.to_string()),
            expires_at,
            created_at,
        })
    })
}

fn received_at_to_iso(raw: &Value) -> String {
    let Some(value) = raw.get("receivedAt") else {
        return String::new();
    };
    let seconds = value
        .as_i64()
        .or_else(|| value.as_f64().map(|n| n as i64))
        .or_else(|| {
            value
                .as_str()
                .and_then(|s| s.trim().parse::<f64>().ok())
                .map(|n| n as i64)
        })
        .unwrap_or(0);
    if seconds <= 0 {
        return String::new();
    }
    chrono::DateTime::from_timestamp(seconds, 0)
        .map(|dt| dt.to_rfc3339())
        .unwrap_or_default()
}

fn flatten_email(raw: &Value, recipient: &str) -> Value {
    json!({
        "id": raw.get("id").cloned().unwrap_or(Value::String(String::new())),
        "from": raw.get("fromAddress").cloned().unwrap_or(Value::String(String::new())),
        "to": raw.get("toAddress").cloned().unwrap_or(Value::String(recipient.to_string())),
        "subject": raw.get("subject").cloned().unwrap_or(Value::String(String::new())),
        "text": raw.get("textContent").cloned().unwrap_or(Value::String(String::new())),
        "html": raw.get("htmlContent").cloned().unwrap_or(Value::String(String::new())),
        "date": received_at_to_iso(raw),
        "isRead": raw.get("isRead").cloned().unwrap_or(Value::Bool(false)),
        "attachments": [],
    })
}

async fn fetch_email_detail(id: &str) -> Result<Option<Value>, String> {
    let url = format!("{}/emails/{}", BASE, urlencoding::encode(id));
    let resp = mffac_headers(http_client().get(&url), false)
        .send()
        .await
        .map_err(|e| e.to_string())?;
    if !resp.status().is_success() {
        return Ok(None);
    }
    let data: Value = resp.json().await.map_err(|e| e.to_string())?;
    if data.get("success").and_then(|x| x.as_bool()) != Some(true) {
        return Ok(None);
    }
    Ok(data.get("email").cloned().filter(|x| x.is_object()))
}

pub fn get_emails(email: &str, _token: Option<&str>) -> Result<Vec<Email>, String> {
    let local = email.split('@').next().unwrap_or(email).trim();
    let enc = urlencoding::encode(local);
    let url = format!("{}/mailboxes/{}/emails", BASE, enc);
    block_on(async {
        let resp = mffac_headers(http_client().get(&url), false)
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("mffac emails {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        if data.get("success").and_then(|x| x.as_bool()) != Some(true) {
            return Err("mffac: list failed".into());
        }
        let rows = data
            .get("emails")
            .and_then(|x| x.as_array())
            .cloned()
            .unwrap_or_default();
        let mut out = Vec::with_capacity(rows.len());
        for raw in rows {
            let mut source = raw.clone();
            if let Some(id) = raw
                .get("id")
                .and_then(|x| x.as_str())
                .map(|s| s.trim())
                .filter(|s| !s.is_empty())
            {
                if let Ok(Some(detail)) = fetch_email_detail(id).await {
                    source = detail;
                }
            }
            out.push(normalize_email(&flatten_email(&source, email), email));
        }
        Ok(out)
    })
}
