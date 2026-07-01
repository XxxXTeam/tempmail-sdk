/*!
 * FakeEmailSite — https://fake-email.site
 */

use crate::config::{block_on, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::{json, Value};

const API_BASE: &str = "https://api.fake-email.site/api";

fn headers(b: wreq::RequestBuilder) -> wreq::RequestBuilder {
    b.header("Accept", "application/json")
        .header("Content-Type", "application/json")
}

/// 创建临时邮箱
pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let resp = headers(http_client().post(format!("{}/temporary-address", API_BASE)))
            .body(json!({}).to_string())
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("fake-email-site create {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let email = data
            .get("temp_email_addr")
            .and_then(|x| x.as_str())
            .unwrap_or("")
            .trim()
            .to_string();
        if email.is_empty() || !email.contains('@') {
            return Err("fake-email-site: invalid new email response".into());
        }
        Ok(EmailInfo {
            channel: Channel::FakeEmailSite,
            email,
            token: None,
            expires_at: None,
            created_at: None,
        })
    })
}

/// 获取邮件列表
pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let em = email.trim();
    if em.is_empty() {
        return Err("fake-email-site: empty email".into());
    }
    block_on(async {
        let url = format!(
            "{}/inbox/poll?address={}",
            API_BASE,
            urlencoding::encode(em)
        );
        let resp = headers(http_client().get(url))
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if resp.status().as_u16() == 404 {
            return Ok(Vec::new());
        }
        if !resp.status().is_success() {
            return Err(format!("fake-email-site poll {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let rows = data
            .get("messages")
            .and_then(|x| x.as_array())
            .cloned()
            .unwrap_or_default();
        let mut out = Vec::new();
        for msg in rows {
            let normalized = json!({
                "id": msg.get("id").cloned().unwrap_or(Value::String(String::new())),
                "from": msg.get("from_addr").cloned().unwrap_or(Value::String(String::new())),
                "to": msg.get("to_addr").cloned().unwrap_or(Value::String(String::new())),
                "subject": msg.get("subject").cloned().unwrap_or(Value::String(String::new())),
                "text": msg.get("body_text").cloned().unwrap_or(Value::String(String::new())),
                "timestamp": msg.get("received_at").cloned().unwrap_or(Value::Null),
                "read": Value::Bool(false),
            });
            out.push(normalize_email(&normalized, em));
        }
        Ok(out)
    })
}
