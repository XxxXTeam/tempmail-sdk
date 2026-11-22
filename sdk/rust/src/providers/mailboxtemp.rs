/*!
 * MailboxTemp — https://mailboxtemp.com
 *
 * 无需密钥；创建免费邮箱后，地址即读取凭据。
 * 创建: POST /api/inbox/generate
 * 列表: GET /api/inbox/{address}/emails
 * 详情: GET /api/email/{address}/{id}
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::{json, Value};

const BASE_URL: &str = "https://mailboxtemp.com";

fn encode_path(value: &str) -> String {
    value
        .bytes()
        .map(|b| match b {
            b'A'..=b'Z' | b'a'..=b'z' | b'0'..=b'9' | b'@' | b'.' | b'-' | b'_' | b'~' => {
                (b as char).to_string()
            }
            _ => format!("%{b:02X}"),
        })
        .collect()
}

async fn get_json(url: &str) -> Result<Value, String> {
    let resp = http_client()
        .get(url)
        .header("Accept", "application/json")
        .header("User-Agent", get_current_ua())
        .send()
        .await
        .map_err(|e| format!("mailboxtemp: 请求失败: {e}"))?;
    let status = resp.status();
    let data: Value = resp.json().await.map_err(|e| e.to_string())?;
    if !status.is_success() || data.get("ok").and_then(Value::as_bool) == Some(false) {
        return Err(format!("mailboxtemp: HTTP {status}"));
    }
    Ok(data)
}

/// 创建 MailboxTemp 临时邮箱
pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let resp = http_client()
            .post(format!("{BASE_URL}/api/inbox/generate"))
            .header("Accept", "application/json")
            .header("User-Agent", get_current_ua())
            .send()
            .await
            .map_err(|e| format!("mailboxtemp: 创建邮箱请求失败: {e}"))?;
        let status = resp.status();
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let address = data.get("address").and_then(Value::as_str).unwrap_or("");
        if !status.is_success() || address.is_empty() {
            return Err(format!("mailboxtemp: 创建邮箱 HTTP {status}"));
        }
        Ok(EmailInfo {
            channel: Channel::MailboxTemp,
            email: address.to_string(),
            token: None,
            expires_at: None,
            created_at: None,
        })
    })
}

/// 获取 MailboxTemp 邮件，逐条补全正文
pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let address = email.trim();
    if address.is_empty() {
        return Err("mailboxtemp: 邮箱地址为空".into());
    }
    let path_addr = encode_path(address);
    block_on(async {
        let list = get_json(&format!("{BASE_URL}/api/inbox/{path_addr}/emails")).await?;
        let rows = list
            .get("emails")
            .and_then(Value::as_array)
            .cloned()
            .unwrap_or_default();
        let mut out = Vec::new();
        for item in rows {
            let id = item.get("id").and_then(Value::as_str).unwrap_or("");
            if id.is_empty() {
                continue;
            }
            if let Ok(detail) =
                get_json(&format!("{BASE_URL}/api/email/{path_addr}/{}", encode_path(id))).await
            {
                if let Some(message) = detail.get("email") {
                    out.push(normalize_email(
                        &json!({
                            "id": message.get("id").cloned().unwrap_or(Value::Null),
                            "from": message.get("from_address").cloned().unwrap_or(Value::Null),
                            "to": address,
                            "subject": message.get("subject").cloned().unwrap_or(Value::Null),
                            "text": message.get("body_text").cloned().unwrap_or(Value::Null),
                            "html": message.get("body_html").cloned().unwrap_or(Value::Null),
                            "date": message.get("received_at").cloned().unwrap_or(Value::Null),
                            "read": message.get("read").cloned().unwrap_or(Value::Null),
                            "attachments": detail.get("attachments").cloned().unwrap_or(json!([])),
                        }),
                        address,
                    ));
                    continue;
                }
            }
            out.push(normalize_email(
                &json!({
                    "id": id,
                    "from": item.get("from_address").cloned().unwrap_or(Value::Null),
                    "to": address,
                    "subject": item.get("subject").cloned().unwrap_or(Value::Null),
                    "date": item.get("received_at").cloned().unwrap_or(Value::Null),
                    "read": item.get("read").cloned().unwrap_or(Value::Null),
                }),
                address,
            ));
        }
        Ok(out)
    })
}
