/*!
 * ockito 渠道实现
 */

use crate::config::{block_on, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::Value;

const BASE_URL: &str = "https://ockito.com/web-api";

fn any_string(v: &Value) -> String {
    match v {
        Value::String(s) => s.trim().to_string(),
        Value::Number(n) => n.to_string(),
        Value::Bool(b) => b.to_string(),
        _ => String::new(),
    }
}

async fn request_json(
    method: &str,
    path: &str,
    headers: Option<Vec<(&str, String)>>,
    body: Option<Value>,
) -> Result<(u16, Value), String> {
    let builder = match method {
        "POST" => http_client().post(format!("{}{}", BASE_URL, path)),
        _ => http_client().get(format!("{}{}", BASE_URL, path)),
    };
    let mut req = builder.header("Accept", "application/json");
    if let Some(hs) = headers {
        for (k, v) in hs {
            req = req.header(k, v);
        }
    }
    if let Some(b) = body {
        req = req.header("Content-Type", "application/json").json(&b);
    }
    let resp = req
        .send()
        .await
        .map_err(|e| format!("ockito request failed: {}", e))?;
    let status = resp.status().as_u16();
    let text = resp
        .text()
        .await
        .map_err(|e| format!("ockito read response: {}", e))?;
    let json = if text.trim().is_empty() {
        Value::Object(Default::default())
    } else {
        serde_json::from_str(&text)
            .map_err(|_| format!("ockito invalid JSON: {} HTTP {}", path, status))?
    };
    Ok((status, json))
}

fn encode_token(access_token: &str, refresh_token: &str) -> String {
    serde_json::json!({
        "access_token": access_token,
        "refresh_token": refresh_token,
    })
    .to_string()
}

fn decode_token(token: &str) -> Result<(String, String), String> {
    let value = token.trim();
    if value.is_empty() || !value.starts_with('{') {
        return Err("ockito: invalid session token".into());
    }
    let data: Value =
        serde_json::from_str(value).map_err(|_| "ockito: invalid session token".to_string())?;
    let access_token = any_string(&data["access_token"]);
    let refresh_token = any_string(&data["refresh_token"]);
    if access_token.is_empty() || refresh_token.is_empty() {
        return Err("ockito: invalid session token".into());
    }
    Ok((access_token, refresh_token))
}

async fn refresh_access_token(refresh_token: &str) -> Result<String, String> {
    let (status, data) = request_json(
        "POST",
        "/grefresh",
        Some(vec![
            ("Authorization", format!("Bearer {}", refresh_token)),
            ("X-PASSTHROUGH", "Y".to_string()),
        ]),
        None,
    )
    .await?;
    if !(200..300).contains(&status) {
        return Err(format!("ockito grefresh http {}", status));
    }
    let access_token = any_string(&data["access_token"]);
    if access_token.is_empty() {
        return Err("ockito: invalid refresh response".into());
    }
    Ok(access_token)
}

async fn fetch_bearer_json(
    path: &str,
    access_token: &mut String,
    refresh_token: &str,
) -> Result<Value, String> {
    let (status, data) = request_json(
        "GET",
        path,
        Some(vec![("Authorization", format!("Bearer {}", access_token))]),
        None,
    )
    .await?;
    if status == 401 {
        *access_token = refresh_access_token(refresh_token).await?;
        let (_, retry) = request_json(
            "GET",
            path,
            Some(vec![("Authorization", format!("Bearer {}", access_token))]),
            None,
        )
        .await?;
        return Ok(retry);
    }
    if !(200..300).contains(&status) {
        return Err(format!("ockito http {}", status));
    }
    Ok(data)
}

fn flatten_inbox_row(raw: &Value, recipient: &str) -> Value {
    serde_json::json!({
        "id": any_string(&raw["uid"]),
        "from": any_string(&raw["sender"]),
        "to": recipient,
        "subject": any_string(&raw["subject"]),
        "text": any_string(&raw["snippet"]),
        "html": any_string(&raw["html"]),
        "date": raw.get("timestamp").cloned().unwrap_or(Value::Null),
        "is_read": false,
        "attachments": [],
    })
}

fn flatten_message(raw: &Value, recipient: &str) -> Value {
    let obj = raw
        .get("obj")
        .and_then(|v| v.as_object())
        .cloned()
        .map(Value::Object)
        .unwrap_or_else(|| raw.clone());
    let from = any_string(&obj["sender_email"]).chars().collect::<String>();
    let to = {
        let value = any_string(&obj["to"]);
        if value.is_empty() {
            recipient.to_string()
        } else {
            value
        }
    };
    let subject = any_string(&obj["subject"]);
    let text = any_string(&obj["text"]);
    let html = any_string(&obj["html"]);
    let date = obj.get("date").cloned().unwrap_or(Value::Null);
    let attachments: Vec<Value> = Vec::new();
    serde_json::json!({
        "id": any_string(&raw["uid"]),
        "from": from,
        "to": to,
        "subject": subject,
        "text": text,
        "html": html,
        "date": date,
        "is_read": false,
        "attachments": attachments,
    })
}

pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let (status, login) =
            request_json("POST", "/gtoken", None, Some(serde_json::json!({}))).await?;
        if !(200..300).contains(&status) {
            return Err(format!("ockito gtoken http {}", status));
        }
        let access_token = any_string(&login["access_token"]);
        let refresh_token = any_string(&login["refresh_token"]);
        if access_token.is_empty() || refresh_token.is_empty() {
            return Err("ockito: invalid token response".into());
        }

        let (status, email_json) = request_json(
            "GET",
            "/email",
            Some(vec![("Authorization", format!("Bearer {}", access_token))]),
            None,
        )
        .await?;
        if !(200..300).contains(&status) {
            return Err(format!("ockito email http {}", status));
        }
        let email_value = &email_json["email"];
        let mut email = if let Some(s) = email_value.as_str() {
            s.trim().to_string()
        } else if let Some(obj) = email_value.as_object() {
            any_string(&obj.get("email").cloned().unwrap_or(Value::Null))
        } else {
            any_string(email_value)
        };
        if email.is_empty() {
            email = any_string(&email_json["email"]);
        }
        if email.is_empty() || !email.contains('@') {
            return Err("ockito: invalid email response".into());
        }

        Ok(EmailInfo {
            channel: Channel::Ockito,
            email,
            token: Some(encode_token(&access_token, &refresh_token)),
            expires_at: None,
            created_at: None,
        })
    })
}

pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let (mut access_token, refresh_token) = decode_token(token)?;
    let address = email.trim();
    if address.is_empty() {
        return Err("ockito: empty email".into());
    }

    block_on(async {
        let data = fetch_bearer_json(
            &format!("/retrieve/{}/emails", urlencoding::encode(address)),
            &mut access_token,
            &refresh_token,
        )
        .await?;
        let rows = data
            .get("inbox")
            .and_then(|v| v.as_array())
            .cloned()
            .unwrap_or_default();
        let mut out = Vec::with_capacity(rows.len());
        for row in &rows {
            let uid = row.get("uid").and_then(|v| v.as_str()).unwrap_or("").trim();
            if uid.is_empty() {
                out.push(normalize_email(&flatten_inbox_row(row, address), address));
                continue;
            }
            let detail = fetch_bearer_json(
                &format!(
                    "/retrieve/{}/{}",
                    urlencoding::encode(address),
                    urlencoding::encode(uid)
                ),
                &mut access_token,
                &refresh_token,
            )
            .await
            .unwrap_or_else(|_| row.clone());
            out.push(normalize_email(&flatten_message(&detail, address), address));
        }
        Ok(out)
    })
}
