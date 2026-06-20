/*!
 * MailForSpam — https://mailforspam.com
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use rand::Rng;
use serde_json::{json, Value};

const API_BASE: &str = "https://api.mailforspam.com/api";
const DEFAULT_DOMAIN: &str = "mailforspam.com";
const DOMAINS: &[&str] = &["mailforspam.com", "tempmail.io", "disposable.email"];

fn headers(b: wreq::RequestBuilder) -> wreq::RequestBuilder {
    b.header("Accept", "application/json")
        .header("Referer", "https://mailforspam.com/")
        .header("Origin", "https://mailforspam.com")
        .header("User-Agent", get_current_ua())
}

fn random_local() -> String {
    const CHARS: &[u8] = b"abcdefghijklmnopqrstuvwxyz0123456789";
    let mut rng = rand::thread_rng();
    let mut out = String::from("sdk");
    for _ in 0..16 {
        out.push(CHARS[rng.gen_range(0..CHARS.len())] as char);
    }
    out
}

fn pick_domain(preferred: Option<&str>) -> &'static str {
    if let Some(p) = preferred {
        let wanted = p.trim().trim_start_matches('@').to_lowercase();
        if let Some(hit) = DOMAINS.iter().find(|d| d.to_lowercase() == wanted) {
            return hit;
        }
    }
    DEFAULT_DOMAIN
}

fn mailbox_emails_url(email: &str, page_size: u32) -> String {
    format!(
        "{}/mailboxes/{}/emails?page=1&page_size={}&sort_by=date&sort_order=desc",
        API_BASE,
        urlencoding::encode(email),
        page_size
    )
}

fn string_field(raw: &Value, key: &str) -> String {
    raw.get(key)
        .and_then(|x| x.as_str())
        .unwrap_or("")
        .trim()
        .to_string()
}

fn flatten(raw: &Value, recipient: &str) -> Value {
    let to = {
        let value = string_field(raw, "receiver");
        if value.is_empty() {
            recipient.to_string()
        } else {
            value
        }
    };
    json!({
        "id": raw.get("id").cloned().unwrap_or(Value::String(String::new())),
        "from": string_field(raw, "sender"),
        "to": to,
        "subject": raw.get("subject").cloned().unwrap_or(Value::String(String::new())),
        "text": raw.get("body_text").cloned().unwrap_or(Value::String(String::new())),
        "html": raw.get("body_html").cloned().unwrap_or(Value::String(String::new())),
        "date": raw.get("date").cloned().unwrap_or(Value::String(String::new())),
        "isRead": raw.get("readAt").is_some() && !raw.get("readAt").unwrap().is_null(),
        "attachments": raw.get("attachments").cloned().unwrap_or(Value::Array(vec![])),
    })
}

fn fetch_message(message_id: &str, email: &str) -> Result<Value, String> {
    block_on(async {
        let url = format!(
            "{}/mailboxes/{}/emails/{}",
            API_BASE,
            urlencoding::encode(email),
            urlencoding::encode(message_id)
        );
        let resp = headers(http_client().get(url))
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("mailforspam message {}", resp.status()));
        }
        resp.json::<Value>().await.map_err(|e| e.to_string())
    })
}

pub fn generate_email(domain: Option<&str>) -> Result<EmailInfo, String> {
    let email = format!("{}@{}", random_local(), pick_domain(domain));
    block_on(async {
        let resp = headers(http_client().get(mailbox_emails_url(&email, 1)))
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("mailforspam validate mailbox {}", resp.status()));
        }
        Ok(EmailInfo {
            channel: Channel::Mailforspam,
            email,
            token: None,
            expires_at: None,
            created_at: None,
        })
    })
}

pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let em = email.trim();
    if em.is_empty() {
        return Err("mailforspam: empty email".into());
    }
    block_on(async {
        let resp = headers(http_client().get(mailbox_emails_url(em, 100)))
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("mailforspam mailbox {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let rows = data
            .get("emails")
            .and_then(|x| x.as_array())
            .cloned()
            .unwrap_or_default();
        let mut out = Vec::new();
        for item in rows {
            let id = item.get("id").and_then(|x| x.as_str()).unwrap_or("").trim();
            if id.is_empty() {
                continue;
            }
            let detail = fetch_message(id, em).unwrap_or(item);
            out.push(normalize_email(&flatten(&detail, em), em));
        }
        Ok(out)
    })
}
