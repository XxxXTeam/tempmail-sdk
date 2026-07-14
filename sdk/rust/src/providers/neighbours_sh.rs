/*!
 * Neighbours — https://neighbours.sh
 * 公共收件箱模式，无需注册，本地生成随机用户名即可收信
 * token 存储邮箱地址本身
 */

use crate::config::{block_on, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use rand::Rng;
use serde_json::{json, Value};

const API_BASE: &str = "https://neighbours.sh/api/v1";
const DOMAIN: &str = "neighbours.sh";

/// 生成随机用户名，前缀 sdk + 10 位小写字母数字
fn random_username() -> String {
    const CHARS: &[u8] = b"abcdefghijklmnopqrstuvwxyz0123456789";
    let mut rng = rand::thread_rng();
    let mut out = String::from("sdk");
    for _ in 0..10 {
        let idx = rng.gen_range(0..CHARS.len());
        out.push(CHARS[idx] as char);
    }
    out
}

/// 将 neighbours.sh 邮件详情映射为标准化中间格式
fn flatten_message(detail: &Value, recipient: &str) -> Value {
    let from_value = detail
        .get("from")
        .and_then(|f| f.get("value"))
        .and_then(Value::as_array)
        .and_then(|arr| arr.first());
    let from_addr = from_value
        .and_then(|v| v.get("address"))
        .and_then(Value::as_str)
        .filter(|s| !s.is_empty())
        .or_else(|| {
            detail
                .get("from")
                .and_then(|f| f.get("text"))
                .and_then(Value::as_str)
        })
        .unwrap_or("");
    let to = detail
        .get("to")
        .and_then(|t| t.get("text"))
        .and_then(Value::as_str)
        .filter(|s| !s.is_empty())
        .unwrap_or(recipient);
    let id = detail
        .get("uid")
        .map(|v| match v {
            Value::Number(n) => n.to_string(),
            Value::String(s) => s.clone(),
            _ => String::new(),
        })
        .unwrap_or_default();
    json!({
        "id": id,
        "from": from_addr,
        "to": to,
        "subject": detail.get("subject").and_then(Value::as_str).unwrap_or(""),
        "text": detail.get("text").and_then(Value::as_str).unwrap_or(""),
        "html": detail.get("html").and_then(Value::as_str).unwrap_or(""),
        "date": detail.get("date").and_then(Value::as_str).unwrap_or(""),
        "attachments": detail.get("attachments").cloned().unwrap_or(Value::Array(vec![])),
    })
}

/// 创建 neighbours.sh 临时邮箱
/// 公共收件箱，任意用户名即可收信，无需注册；token 存储邮箱地址
pub fn generate_email() -> Result<EmailInfo, String> {
    let email = format!("{}@{}", random_username(), DOMAIN);
    Ok(EmailInfo {
        channel: Channel::NeighboursSh,
        email: email.clone(),
        token: Some(email),
        expires_at: None,
        created_at: None,
    })
}

/// 获取 neighbours.sh 邮件列表
/// API: GET /inbox/{address} → 摘要列表；GET /inbox/{address}/{uid} → 单邮件详情
pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let address = if !token.trim().is_empty() {
        token.trim().to_string()
    } else {
        email.trim().to_string()
    };
    if address.is_empty() {
        return Err("neighbours-sh: 缺少邮箱地址".into());
    }

    block_on(async {
        /* 先请求列表拿到 uid 数组 */
        let list_url = format!("{}/inbox/{}", API_BASE, urlencoding::encode(&address));
        let list_resp = http_client()
            .get(list_url)
            .header("Accept", "application/json")
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !list_resp.status().is_success() {
            return Err(format!(
                "neighbours-sh: 获取邮件列表失败 {}",
                list_resp.status()
            ));
        }
        let list_data: Value = list_resp.json().await.map_err(|e| e.to_string())?;
        let rows = list_data
            .get("data")
            .and_then(Value::as_array)
            .cloned()
            .unwrap_or_default();

        let mut out = Vec::new();
        for row in rows {
            let Some(uid) = row.get("uid").and_then(Value::as_i64) else {
                continue;
            };
            /* 详情接口返回完整正文 */
            let detail_url = format!(
                "{}/inbox/{}/{}",
                API_BASE,
                urlencoding::encode(&address),
                uid
            );
            let detail_resp = match http_client()
                .get(detail_url)
                .header("Accept", "application/json")
                .send()
                .await
            {
                Ok(r) => r,
                Err(_) => continue,
            };
            if !detail_resp.status().is_success() {
                continue;
            }
            let detail_data: Value = match detail_resp.json().await {
                Ok(d) => d,
                Err(_) => continue,
            };
            if let Some(detail) = detail_data.get("data") {
                out.push(normalize_email(
                    &flatten_message(detail, &address),
                    &address,
                ));
            }
        }
        Ok(out)
    })
}
