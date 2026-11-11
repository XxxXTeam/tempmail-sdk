/*!
 * TenminApp 渠道实现（tenmin.app，真实 API 域 api.tenmin.app）
 *
 * 建箱与读信共用 GET /api/inbox/{localpart}（Accept: application/json）。
 * localpart 为随机 6 位小写十六进制串（首访即建箱，平台无显式创建接口）；
 * 响应 {"inboxId":..,"address":"..@tenmin.app","ttl":600,"count":0,"messages":[]}，
 * messages[] 元素字段 id/from/subject/text/html/receivedAt（from 为 {name,address} 对象）。
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use rand::RngExt;
use serde_json::{json, Value};

const BASE_URL: &str = "https://api.tenmin.app";

/// 生成 6 位小写十六进制随机 localpart
fn random_local() -> String {
    const HEX: &[u8] = b"0123456789abcdef";
    let mut rng = rand::rng();
    (0..6)
        .map(|_| HEX[rng.random_range(0..HEX.len())] as char)
        .collect()
}

/// 请求 /api/inbox/{localpart}
async fn fetch_inbox(localpart: &str) -> Result<Value, String> {
    let resp = http_client()
        .get(format!("{BASE_URL}/api/inbox/{localpart}"))
        .header("User-Agent", get_current_ua())
        .header("Accept", "application/json")
        .send()
        .await
        .map_err(|e| format!("tenmin-app: 请求收件箱失败: {e}"))?;

    if !resp.status().is_success() {
        return Err(format!("tenmin-app inbox: http {}", resp.status()));
    }

    resp.json()
        .await
        .map_err(|e| format!("tenmin-app: 解析收件箱响应失败: {e}"))
}

/// 创建 tenmin.app 临时邮箱
/// 首次 GET 随机 localpart 即自动建箱（10 分钟 TTL），token 存储 localpart
pub fn generate_email() -> Result<EmailInfo, String> {
    let local = random_local();
    let data =
        block_on(fetch_inbox(&local)).map_err(|e| format!("tenmin-app: 创建邮箱失败: {e}"))?;

    let address = data
        .get("address")
        .and_then(|v| v.as_str())
        .filter(|s| !s.is_empty())
        .map(|s| s.to_string())
        .unwrap_or_else(|| format!("{local}@tenmin.app"));

    let expires_at = data
        .get("ttl")
        .and_then(|v| v.as_f64())
        .filter(|n| *n > 0.0)
        .and_then(|n| {
            chrono::Utc::now()
                .timestamp_millis()
                .checked_add((n as i64) * 1000)
        });

    Ok(EmailInfo {
        channel: Channel::TenminApp,
        email: address,
        token: Some(local),
        expires_at,
        created_at: None,
    })
}

/// 读取 tenmin.app 收件箱
/// 复用建箱同一 localpart 轮询；from 为 {name,address} 对象时拆出地址字段
pub fn get_emails(email: &str, token: &str) -> Result<Vec<Email>, String> {
    let em = email.trim();
    let local = token.trim();
    if local.is_empty() {
        return Err("tenmin-app: token 为空".into());
    }

    let data = block_on(fetch_inbox(local))?;
    let messages = data
        .get("messages")
        .and_then(|v| v.as_array())
        .cloned()
        .unwrap_or_default();

    let mut out = Vec::with_capacity(messages.len());
    for m in &messages {
        /* from 为对象（{name,address}）时拆出地址字段 */
        let from = match m.get("from") {
            Some(Value::Object(obj)) => {
                let addr = obj
                    .get("address")
                    .and_then(|v| v.as_str())
                    .unwrap_or("")
                    .trim()
                    .to_string();
                let name = obj
                    .get("name")
                    .and_then(|v| v.as_str())
                    .unwrap_or("")
                    .trim()
                    .to_string();
                match (addr.is_empty(), name.is_empty()) {
                    (false, false) => format!("{name} <{addr}>"),
                    (false, true) => addr,
                    (true, false) => name,
                    _ => String::new(),
                }
            }
            _ => m
                .get("from")
                .and_then(|v| v.as_str())
                .unwrap_or("")
                .to_string(),
        };

        let flat = json!({
            "id": m.get("id").and_then(|v| v.as_str()).unwrap_or(""),
            "from": from,
            "to": em,
            "subject": m.get("subject").and_then(|v| v.as_str()).unwrap_or(""),
            "text": m.get("text").and_then(|v| v.as_str()).unwrap_or(""),
            "html": m.get("html").and_then(|v| v.as_str()).unwrap_or(""),
            "date": m.get("receivedAt").and_then(|v| v.as_str()).unwrap_or(""),
        });
        out.push(normalize_email(&flat, em));
    }
    Ok(out)
}
