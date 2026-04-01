/*!
 * MFFAC — https://www.mffac.com/api
 */

use serde_json::Value;
use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};

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
        let mb = data.get("mailbox").ok_or_else(|| "mffac: no mailbox".to_string())?;
        let addr = mb.get("address").and_then(|x| x.as_str()).unwrap_or("").trim();
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
        Ok(rows
            .iter()
            .map(|raw| normalize_email(raw, email))
            .collect())
    })
}
