/*!
 * Tsukaisute Mail — https://tsukaisutemail.com
 *
 * 无需密钥；创建随机地址后，地址即读取凭据。
 * 创建: POST /api/v1/address
 * 列表: GET /api/v1/inbox/{address}
 * 详情: GET /api/v1/message/{id}
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::{json, Value};

const BASE_URL: &str = "https://tsukaisutemail.com/api/v1";

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
        .map_err(|e| format!("tsukaisutemail: 请求失败: {e}"))?;
    if !resp.status().is_success() {
        return Err(format!("tsukaisutemail: HTTP {}", resp.status()));
    }
    resp.json().await.map_err(|e| e.to_string())
}

/// 创建 Tsukaisute Mail 临时地址
pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let resp = http_client()
            .post(format!("{BASE_URL}/address"))
            .header("Accept", "application/json")
            .header("User-Agent", get_current_ua())
            .send()
            .await
            .map_err(|e| format!("tsukaisutemail: 创建地址请求失败: {e}"))?;
        if !resp.status().is_success() {
            return Err(format!("tsukaisutemail: 创建地址 HTTP {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let address = data.get("address").and_then(Value::as_str).unwrap_or("");
        if address.is_empty() {
            return Err("tsukaisutemail: 创建响应缺少地址".into());
        }
        Ok(EmailInfo {
            channel: Channel::TsukaisuteMail,
            email: address.to_string(),
            token: None,
            expires_at: None,
            created_at: None,
        })
    })
}

/// 获取 Tsukaisute Mail 邮件，逐条补全正文
pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let address = email.trim();
    if address.is_empty() {
        return Err("tsukaisutemail: 邮箱地址为空".into());
    }
    block_on(async {
        let list = get_json(&format!("{BASE_URL}/inbox/{}", encode_path(address))).await?;
        let rows = list
            .get("messages")
            .and_then(Value::as_array)
            .cloned()
            .unwrap_or_default();
        let mut out = Vec::new();
        for item in rows {
            let id = item.get("id").and_then(Value::as_str).unwrap_or("");
            if id.is_empty() {
                continue;
            }
            if let Ok(detail) = get_json(&format!("{BASE_URL}/message/{}", encode_path(id))).await {
                if let Some(message) = detail.get("message") {
                    out.push(normalize_email(
                        &json!({
                            "id": message.get("id").cloned().unwrap_or(Value::Null),
                            "from": message.get("from_address").cloned().unwrap_or(Value::Null),
                            "to": address,
                            "subject": message.get("subject").cloned().unwrap_or(Value::Null),
                            "text": message.get("text_body").cloned().unwrap_or(Value::Null),
                            "html": message.get("html_body").cloned().unwrap_or(Value::Null),
                            "date": message.get("received_at").cloned().unwrap_or(Value::Null),
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
                    "text": item.get("snippet").cloned().unwrap_or(Value::Null),
                    "date": item.get("received_at").cloned().unwrap_or(Value::Null),
                }),
                address,
            ));
        }
        Ok(out)
    })
}
