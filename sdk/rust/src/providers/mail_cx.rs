/*!
 * mail.cx -- https://mail.cx
 * Anonymous Web API: https://mail.cx/v1
 */

use crate::config::{block_on, get_config as sdk_config, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use chrono::Utc;
use rand::{distributions::Alphanumeric, Rng};
use serde_json::{json, Value};
use std::time::Duration;

const BASE_URL: &str = "https://mail.cx";

fn random_string(len: usize) -> String {
    rand::thread_rng()
        .sample_iter(&Alphanumeric)
        .take(len)
        .map(char::from)
        .map(|c| c.to_ascii_lowercase())
        .collect()
}

fn client_id() -> String {
    format!("tempmail-sdk-{}", random_string(16))
}

fn apply_headers(req: wreq::RequestBuilder, client_id: &str) -> wreq::RequestBuilder {
    req.header("Accept", "application/json")
        .header("Origin", BASE_URL)
        .header("Referer", format!("{}/", BASE_URL))
        .header("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0")
        .header("X-Client-ID", client_id)
}

fn long_poll_client() -> Result<wreq::Client, String> {
    let cfg = sdk_config();
    let mut builder = wreq::Client::builder()
        .timeout(Duration::from_secs(cfg.timeout_secs.max(35)))
        .cookie_store(true);

    if let Some(ref proxy_url) = cfg.proxy {
        if let Ok(proxy) = wreq::Proxy::all(proxy_url) {
            builder = builder.proxy(proxy);
        }
    }
    if cfg.insecure {
        builder = builder.cert_verification(false);
    }

    builder.build().map_err(|e| e.to_string())
}

async fn get_mail_cx_config(client_id: &str) -> Result<Value, String> {
    let resp = apply_headers(
        http_client().get(format!("{}/v1/config", BASE_URL)),
        client_id,
    )
    .send()
    .await
    .map_err(|e| e.to_string())?;
    if !resp.status().is_success() {
        return Err(format!("mail-cx config: {}", resp.status()));
    }
    resp.json::<Value>().await.map_err(|e| e.to_string())
}

fn pick_domain(config: &Value, preferred_domain: Option<&str>) -> Result<String, String> {
    let items = config
        .get("system_domains")
        .and_then(|v| v.as_array())
        .cloned()
        .unwrap_or_default();
    let domains: Vec<String> = items
        .iter()
        .filter_map(|item| item.get("domain").and_then(|v| v.as_str()))
        .map(str::trim)
        .filter(|s| !s.is_empty())
        .map(ToOwned::to_owned)
        .collect();
    if domains.is_empty() {
        return Err("mail-cx: no system domains".into());
    }

    if let Some(preferred) = preferred_domain {
        let want = preferred.trim().trim_start_matches('@').to_lowercase();
        if !want.is_empty() {
            if let Some(found) = domains.iter().find(|d| d.to_lowercase() == want) {
                return Ok(found.clone());
            }
        }
    }

    if let Some(default_domain) = items.iter().find_map(|item| {
        let is_default = item
            .get("default")
            .and_then(|v| v.as_bool())
            .unwrap_or(false);
        if !is_default {
            return None;
        }
        item.get("domain")
            .and_then(|v| v.as_str())
            .map(str::trim)
            .filter(|s| !s.is_empty())
    }) {
        return Ok(default_domain.to_string());
    }

    Ok(domains[0].clone())
}

fn first_non_empty(values: &[Option<&Value>]) -> String {
    for value in values {
        if let Some(v) = value {
            if let Some(s) = v.as_str() {
                if !s.trim().is_empty() {
                    return s.trim().to_string();
                }
            } else if !v.is_null() {
                return v.to_string();
            }
        }
    }
    String::new()
}

fn flatten_list_message(raw: &Value, recipient: &str) -> Value {
    json!({
        "id": raw.get("id").cloned().unwrap_or(Value::Null),
        "from": first_non_empty(&[raw.get("from_email"), raw.get("from_name")]),
        "to": recipient,
        "subject": raw.get("subject").cloned().unwrap_or(Value::String(String::new())),
        "text": raw.get("preview_text").cloned().unwrap_or(Value::String(String::new())),
        "created_at": raw.get("created_at").cloned().unwrap_or(Value::Null),
        "attachments": raw.get("attachments").cloned().unwrap_or(Value::Null),
    })
}

fn flatten_detail(raw: &Value, recipient: &str) -> Value {
    json!({
        "id": raw.get("id").cloned().unwrap_or(Value::Null),
        "from": first_non_empty(&[raw.get("from_email"), raw.get("from_name")]),
        "to": recipient,
        "subject": raw.get("subject").cloned().unwrap_or(Value::String(String::new())),
        "text_body": raw.get("text_body").cloned().unwrap_or(Value::Null),
        "html_body": raw.get("html_body").cloned().unwrap_or(Value::Null),
        "text": first_non_empty(&[raw.get("text_body"), raw.get("preview_text")]),
        "html": raw.get("html_body").cloned().unwrap_or(Value::String(String::new())),
        "created_at": raw.get("created_at").cloned().unwrap_or(Value::Null),
        "attachments": raw.get("attachments").cloned().unwrap_or(Value::Null),
    })
}

async fn get_detail(client_id: &str, id: &str) -> Result<Value, String> {
    let resp = apply_headers(
        http_client().get(format!("{}/v1/email/{}", BASE_URL, urlencoding::encode(id))),
        client_id,
    )
    .send()
    .await
    .map_err(|e| e.to_string())?;
    if !resp.status().is_success() {
        return Err(format!("mail-cx detail: {}", resp.status()));
    }
    resp.json::<Value>().await.map_err(|e| e.to_string())
}

pub fn generate_email(preferred_domain: Option<&str>) -> Result<EmailInfo, String> {
    block_on(async {
        let client_id = client_id();
        let config = get_mail_cx_config(&client_id).await?;
        let domain = pick_domain(&config, preferred_domain)?;
        let now = Utc::now();
        let ttl = config
            .get("ttl_seconds")
            .and_then(|v| v.as_i64())
            .filter(|v| *v > 0);

        Ok(EmailInfo {
            channel: Channel::MailCx,
            email: format!("{}@{}", random_string(12), domain),
            token: Some(client_id),
            expires_at: ttl
                .map(|seconds| (now + chrono::Duration::seconds(seconds)).timestamp_millis()),
            created_at: Some(now.to_rfc3339()),
        })
    })
}

pub fn get_emails(client_id: &str, email: &str) -> Result<Vec<Email>, String> {
    block_on(async {
        let resp = apply_headers(
            long_poll_client()?.get(format!(
                "{}/v1/inbox/{}",
                BASE_URL,
                urlencoding::encode(email)
            )),
            client_id,
        )
        .send()
        .await
        .map_err(|e| e.to_string())?;

        if resp.status().as_u16() == 204 {
            return Ok(vec![]);
        }
        if !resp.status().is_success() {
            return Err(format!("mail-cx inbox: {}", resp.status()));
        }

        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let rows = data
            .get("emails")
            .and_then(|v| v.as_array())
            .cloned()
            .unwrap_or_default();

        let mut emails = Vec::with_capacity(rows.len());
        for row in rows {
            let mut raw = flatten_list_message(&row, email);
            if let Some(id) = row
                .get("id")
                .and_then(|v| v.as_str())
                .filter(|s| !s.is_empty())
            {
                if let Ok(detail) = get_detail(client_id, id).await {
                    raw = flatten_detail(&detail, email);
                }
            }
            emails.push(normalize_email(&raw, email));
        }
        Ok(emails)
    })
}
