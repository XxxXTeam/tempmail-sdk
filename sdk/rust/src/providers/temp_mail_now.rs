/*!
 * temp-mail.now 渠道实现
 * 网站: https://temp-mail.now
 *
 * 流程:
 *   1. GET https://temp-mail.now/en/ 获取会话 cookie
 *   2. POST https://temp-mail.now/change_email 使用会话 cookie
 *      body: {"new_email":"xxx@dpmurt.my"}
 *      获取新邮箱地址
 *   3. GET https://temp-mail.now/fetch_emails 使用会话 cookie
 *      返回: {"emails":[...],"remaining_time":n}
 *
 * token 存储: 会话 cookie 字符串
 */

use std::collections::BTreeMap;

use crate::config::{block_on, get_current_ua, http_client_no_cookie_jar};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use rand::Rng;
use serde::{Deserialize, Serialize};
use serde_json::{json, Value};

const ORIGIN: &str = "https://temp-mail.now";
/// temp-mail.now 默认使用的邮箱域名
const DOMAIN: &str = "dpmurt.my";
/// token 前缀
const TOK_PREFIX: &str = "tmn1:";

/// 序列化到 token 中的会话信息
#[derive(Serialize, Deserialize)]
struct TmnSess {
    /// Cookie 字符串
    c: String,
}

/// 生成随机用户名（小写字母，12 位）
fn random_name() -> String {
    let charset: &[u8] = b"abcdefghijklmnopqrstuvwxyz0123456789";
    let mut rng = rand::thread_rng();
    (0..12)
        .map(|_| charset[rng.gen_range(0..charset.len())] as char)
        .collect()
}

/// 解析 Cookie 头为 key=value 映射
fn parse_cookie_header(hdr: &str) -> BTreeMap<String, String> {
    let mut m = BTreeMap::new();
    for part in hdr.split(';') {
        let part = part.trim();
        if part.is_empty() {
            continue;
        }
        if let Some(i) = part.find('=') {
            let k = part[..i].trim();
            let v = part[i + 1..].trim();
            if !k.is_empty() {
                m.insert(k.to_string(), v.to_string());
            }
        }
    }
    m
}

/// 将 Cookie 映射转换回 Cookie 头字符串
fn cookie_header_from_map(m: &BTreeMap<String, String>) -> String {
    m.iter()
        .map(|(k, v)| format!("{}={}", k, v))
        .collect::<Vec<_>>()
        .join("; ")
}

/// 合并 Set-Cookie 响应头到已有 Cookie 中
fn merge_set_cookies(hdr: &str, headers: &wreq::header::HeaderMap) -> String {
    let mut m = parse_cookie_header(hdr);
    for val in headers.get_all("set-cookie") {
        let Ok(s) = val.to_str() else {
            continue;
        };
        let first = s.split(';').next().unwrap_or("");
        if let Some(i) = first.find('=') {
            let k = first[..i].trim();
            let v = first[i + 1..].trim();
            if !k.is_empty() {
                m.insert(k.to_string(), v.to_string());
            }
        }
    }
    cookie_header_from_map(&m)
}

/// 编码会话信息为 token 字符串
fn encode_sess(s: &TmnSess) -> Result<String, String> {
    let b = serde_json::to_vec(s).map_err(|e| e.to_string())?;
    Ok(format!(
        "{}{}",
        TOK_PREFIX,
        base64::Engine::encode(&base64::engine::general_purpose::STANDARD, b)
    ))
}

/// 从 token 字符串解码会话信息
fn decode_sess(tok: &str) -> Result<TmnSess, String> {
    if !tok.starts_with(TOK_PREFIX) {
        return Err("temp-mail-now: token 格式无效".into());
    }
    let raw = base64::Engine::decode(
        &base64::engine::general_purpose::STANDARD,
        tok.trim_start_matches(TOK_PREFIX),
    )
    .map_err(|_| "temp-mail-now: token 解码失败".to_string())?;
    let s: TmnSess = serde_json::from_slice(&raw)
        .map_err(|_| "temp-mail-now: token 反序列化失败".to_string())?;
    if s.c.is_empty() {
        return Err("temp-mail-now: token 中 cookie 为空".into());
    }
    Ok(s)
}

/// 构建浏览器请求头
fn browser_headers() -> Vec<(&'static str, String)> {
    vec![
        ("User-Agent", get_current_ua().to_string()),
        (
            "Accept",
            "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8".to_string(),
        ),
        ("Accept-Language", "en-US,en;q=0.9".to_string()),
    ]
}

/// 创建临时邮箱
/// 1. GET /en/ 获取会话 cookie
/// 2. POST /change_email 设置邮箱地址
pub fn generate_email() -> Result<EmailInfo, String> {
    let name = random_name();
    let new_email = format!("{}@{}", name, DOMAIN);

    block_on(async {
        let client = http_client_no_cookie_jar();

        // 第一步: GET /en/ 获取会话 cookie
        let mut req = client.get(format!("{}/en/", ORIGIN));
        for (k, v) in browser_headers() {
            req = req.header(k, v);
        }
        let resp = req
            .send()
            .await
            .map_err(|e| format!("temp-mail-now: 获取会话请求失败: {}", e))?;

        let mut cookie_hdr = merge_set_cookies("", resp.headers());

        if cookie_hdr.is_empty() {
            // 即使没有显式的 Set-Cookie，也继续尝试
            log::warn!("temp-mail-now: 首次请求未获取到 cookie，继续尝试");
        }

        // 第二步: POST /change_email 设置邮箱地址
        let mut req2 = client.post(format!("{}/change_email", ORIGIN));
        for (k, v) in browser_headers() {
            req2 = req2.header(k, v);
        }
        req2 = req2
            .header("Content-Type", "application/json")
            .header("Accept", "application/json")
            .header("Referer", format!("{}/en/", ORIGIN))
            .header("Origin", ORIGIN);

        if !cookie_hdr.is_empty() {
            req2 = req2.header("Cookie", &cookie_hdr);
        }

        let body = json!({ "new_email": new_email });
        let resp2 = req2
            .body(body.to_string())
            .send()
            .await
            .map_err(|e| format!("temp-mail-now: 设置邮箱请求失败: {}", e))?;

        if !resp2.status().is_success() {
            return Err(format!(
                "temp-mail-now: 设置邮箱返回 HTTP {}",
                resp2.status()
            ));
        }

        // 合并新的 cookie
        cookie_hdr = merge_set_cookies(&cookie_hdr, resp2.headers());

        // 尝试从响应中读取确认的邮箱地址
        let resp_data: Value = resp2.json().await.unwrap_or(Value::Null);
        let confirmed_email = resp_data
            .get("email")
            .or_else(|| resp_data.get("new_email"))
            .or_else(|| resp_data.get("address"))
            .and_then(|v| v.as_str())
            .filter(|s| !s.is_empty() && s.contains('@'))
            .unwrap_or(&new_email)
            .to_string();

        // 编码 cookie 为 token
        let tok = encode_sess(&TmnSess { c: cookie_hdr })?;

        Ok(EmailInfo {
            channel: Channel::TempMailNow,
            email: confirmed_email,
            token: Some(tok),
            expires_at: None,
            created_at: None,
        })
    })
}

/// 获取邮件列表
/// GET /fetch_emails 使用会话 cookie
pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let em = email.trim();
    if em.is_empty() {
        return Err("temp-mail-now: 邮箱地址为空".into());
    }

    let sess = decode_sess(token)?;
    let cookie_hdr = &sess.c;

    block_on(async {
        let client = http_client_no_cookie_jar();

        let mut req = client.get(format!("{}/fetch_emails", ORIGIN));
        for (k, v) in browser_headers() {
            req = req.header(k, v);
        }
        req = req
            .header("Accept", "application/json")
            .header("Referer", format!("{}/en/", ORIGIN))
            .header("Cookie", cookie_hdr);

        let resp = req
            .send()
            .await
            .map_err(|e| format!("temp-mail-now: 获取邮件列表请求失败: {}", e))?;

        if resp.status().as_u16() == 404 {
            return Ok(Vec::new());
        }

        if !resp.status().is_success() {
            return Err(format!(
                "temp-mail-now: 获取邮件列表返回 HTTP {}",
                resp.status()
            ));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("temp-mail-now: 解析邮件列表响应失败: {}", e))?;

        // 响应格式: {"emails":[...],"remaining_time":n}
        let items = if let Some(arr) = data.get("emails").and_then(|v| v.as_array()) {
            arr.clone()
        } else if let Some(arr) = data.as_array() {
            arr.clone()
        } else {
            return Ok(Vec::new());
        };

        let mut result = Vec::with_capacity(items.len());
        for msg in &items {
            let raw = json!({
                "id": msg.get("id")
                    .or_else(|| msg.get("_id"))
                    .unwrap_or(&Value::Null),
                "from": msg.get("from")
                    .or_else(|| msg.get("sender"))
                    .or_else(|| msg.get("from_address"))
                    .or_else(|| msg.get("mail_sender"))
                    .unwrap_or(&Value::Null),
                "to": msg.get("to")
                    .or_else(|| msg.get("recipient"))
                    .unwrap_or(&Value::String(em.to_string())),
                "subject": msg.get("subject")
                    .unwrap_or(&Value::Null),
                "text": msg.get("text")
                    .or_else(|| msg.get("body_text"))
                    .or_else(|| msg.get("body"))
                    .unwrap_or(&Value::Null),
                "html": msg.get("html")
                    .or_else(|| msg.get("body_html"))
                    .or_else(|| msg.get("htmlBody"))
                    .unwrap_or(&Value::Null),
                "date": msg.get("date")
                    .or_else(|| msg.get("received_at"))
                    .or_else(|| msg.get("created_at"))
                    .or_else(|| msg.get("timestamp"))
                    .unwrap_or(&Value::Null),
                "isRead": msg.get("is_read")
                    .or_else(|| msg.get("read"))
                    .unwrap_or(&Value::Bool(false)),
                "attachments": msg.get("attachments")
                    .unwrap_or(&Value::Array(vec![])),
            });
            result.push(normalize_email(&raw, em));
        }

        Ok(result)
    })
}
