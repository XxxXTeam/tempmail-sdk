/*!
 * DevMail UK — https://devmail.uk
 */

use crate::config::{block_on, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::Value;

const API_BASE: &str = "https://www.devmail.uk/api";

fn headers(b: wreq::RequestBuilder) -> wreq::RequestBuilder {
    b.header("Accept", "application/json")
}

fn mailbox_from_email(email: &str) -> String {
    let address = email.trim();
    if address.is_empty() {
        return String::new();
    }
    match address.split_once('@') {
        Some((local, _)) if !local.trim().is_empty() => local.trim().to_string(),
        _ => address.to_string(),
    }
}

fn fetch_json(url: String) -> Result<Value, String> {
    block_on(async {
        let resp = headers(http_client().get(url))
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("devmail-uk {}", resp.status()));
        }
        resp.json::<Value>().await.map_err(|e| e.to_string())
    })
}

pub fn generate_email() -> Result<EmailInfo, String> {
    let data = fetch_json(format!("{}/new", API_BASE))?;
    let mut email = data
        .get("email")
        .and_then(|v| v.as_str())
        .unwrap_or("")
        .trim()
        .to_string();
    if email.is_empty() {
        if let Some(mailbox) = data.get("mailbox").and_then(|v| v.as_str()) {
            let mailbox = mailbox.trim();
            if !mailbox.is_empty() {
                email = format!("{}@devmail.uk", mailbox);
            }
        }
    }
    if email.is_empty() || !email.contains('@') {
        return Err("devmail-uk: invalid new email response".into());
    }
    Ok(EmailInfo {
        channel: Channel::DevmailUk,
        email,
        token: None,
        expires_at: None,
        created_at: None,
    })
}

pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let mailbox = mailbox_from_email(email);
    if mailbox.is_empty() {
        return Err("devmail-uk: empty email".into());
    }
    let data = fetch_json(format!(
        "{}/inbox/{}?detail=true",
        API_BASE,
        urlencoding::encode(&mailbox)
    ))?;
    let rows = data
        .get("emails")
        .and_then(|v| v.as_array())
        .cloned()
        .unwrap_or_default();
    let mut out = Vec::with_capacity(rows.len());
    for item in rows {
        out.push(normalize_email(&item, email));
    }
    Ok(out)
}
