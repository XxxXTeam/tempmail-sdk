/*!
 * tempmailten 渠道实现
 * 网站: https://tempmailten.com
 *
 * Laravel + CSRF 后端。
 * 流程:
 *   1. GET /en 获取 session cookie + 从 HTML meta 提取 CSRF token
 *      （<meta name="csrf-token" content="xxx">）
 *   2. POST /messages（body: _token={csrf}&captcha=）获取 {mailbox, messages}
 *   3. GET /view/{id} 获取单封邮件 HTML 正文
 *
 * token 存储: JSON 字符串 {"csrf":"...","cookie":"..."}
 * 该站点 reCAPTCHA 已禁用，captcha 参数传空即可。
 */

use std::collections::BTreeMap;
use std::sync::LazyLock;

use regex::Regex;
use serde::{Deserialize, Serialize};
use serde_json::Value;

use crate::config::{block_on, get_current_ua, http_client_no_cookie_jar};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};

const BASE_URL: &str = "https://tempmailten.com";

/// 从 meta 标签提取 CSRF token（<meta name="csrf-token" content="xxx">）
static CSRF_RE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r#"<meta\s+name="csrf-token"\s+content="([^"]+)""#).expect("re"));

/// token 中序列化的会话信息
#[derive(Serialize, Deserialize)]
struct TempmailtenSess {
    /// CSRF token
    csrf: String,
    /// Cookie 字符串
    cookie: String,
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
fn encode_sess(s: &TempmailtenSess) -> Result<String, String> {
    serde_json::to_string(s).map_err(|e| format!("tempmailten: token 序列化失败: {}", e))
}

/// 从 token 字符串解码会话信息
fn decode_sess(tok: &str) -> Result<TempmailtenSess, String> {
    let s: TempmailtenSess =
        serde_json::from_str(tok).map_err(|_| "tempmailten: token 反序列化失败".to_string())?;
    if s.csrf.is_empty() {
        return Err("tempmailten: token 中 csrf 为空".into());
    }
    Ok(s)
}

/// 构造 POST /messages 的表单 body（_token={csrf}&captcha=）
fn messages_form(csrf: &str) -> String {
    format!("_token={}&captcha=", urlencoding::encode(csrf))
}

/// POST /messages 获取 {mailbox, messages} JSON
/// 返回解析后的 JSON 与合并后的 Cookie 串
async fn post_messages(
    client: &wreq::Client,
    csrf: &str,
    cookie_hdr: &str,
) -> Result<(Value, String), String> {
    let resp = client
        .post(format!("{BASE_URL}/messages"))
        .header("User-Agent", get_current_ua())
        .header("Accept", "application/json, text/javascript, */*; q=0.01")
        .header("Accept-Language", "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7")
        .header("X-Requested-With", "XMLHttpRequest")
        .header("Referer", format!("{BASE_URL}/en"))
        .header("Content-Type", "application/x-www-form-urlencoded")
        .header("X-CSRF-TOKEN", csrf)
        .header("Cookie", cookie_hdr)
        .body(messages_form(csrf))
        .send()
        .await
        .map_err(|e| format!("tempmailten: 请求 /messages 失败: {}", e))?;

    if !resp.status().is_success() {
        return Err(format!("tempmailten messages: {}", resp.status()));
    }

    let merged = merge_set_cookies(cookie_hdr, resp.headers());
    let text = resp
        .text()
        .await
        .map_err(|e| format!("tempmailten: 读取 /messages 响应失败: {}", e))?;
    let data: Value = serde_json::from_str(text.trim())
        .map_err(|e| format!("tempmailten: 解析 /messages 响应失败: {}", e))?;
    Ok((data, merged))
}

/// 创建临时邮箱
/// 1. GET /en 获取 session cookie 和 CSRF token
/// 2. POST /messages 获取分配的邮箱地址
pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let client = http_client_no_cookie_jar();

        // 第一步：GET /en 获取 session cookie 和 CSRF token
        let resp = client
            .get(format!("{BASE_URL}/en"))
            .header("User-Agent", get_current_ua())
            .header(
                "Accept",
                "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
            )
            .header("Accept-Language", "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7")
            .send()
            .await
            .map_err(|e| format!("tempmailten: 获取首页失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("tempmailten: 首页返回 HTTP {}", resp.status()));
        }

        let cookie_hdr = merge_set_cookies("", resp.headers());
        let html = resp
            .text()
            .await
            .map_err(|e| format!("tempmailten: 读取首页响应失败: {}", e))?;

        let csrf = CSRF_RE
            .captures(&html)
            .and_then(|c| c.get(1))
            .map(|m| m.as_str().to_string())
            .filter(|s| !s.is_empty())
            .ok_or_else(|| "tempmailten: 未能从首页提取 CSRF token".to_string())?;

        // 第二步：POST /messages 获取邮箱地址
        let (data, cookie_hdr) = post_messages(&client, &csrf, &cookie_hdr).await?;

        let mailbox = data
            .get("mailbox")
            .and_then(|v| v.as_str())
            .map(|s| s.trim().to_string())
            .unwrap_or_default();

        if mailbox.is_empty() || !mailbox.contains('@') {
            return Err("tempmailten: 响应中未包含有效的 mailbox 字段".into());
        }

        let tok = encode_sess(&TempmailtenSess {
            csrf,
            cookie: cookie_hdr,
        })?;

        Ok(EmailInfo {
            channel: Channel::Tempmailten,
            email: mailbox,
            token: Some(tok),
            expires_at: None,
            created_at: None,
        })
    })
}

/// 获取邮件列表
/// 1. POST /messages 获取 {mailbox, messages} JSON
/// 2. 对每封邮件 GET /view/{id} 获取 HTML 正文
pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let em = email.trim();
    if em.is_empty() {
        return Err("tempmailten: 邮箱地址为空".into());
    }
    let sess = decode_sess(token)?;

    block_on(async {
        let client = http_client_no_cookie_jar();
        let (data, _) = post_messages(&client, &sess.csrf, &sess.cookie).await?;

        // messages 数组每个元素: {id, from, from_email, subject, is_seen}
        let msgs = match data.get("messages").and_then(|v| v.as_array()) {
            Some(arr) if !arr.is_empty() => arr,
            _ => return Ok(vec![]),
        };

        let mut result = Vec::with_capacity(msgs.len());
        for msg in msgs {
            let id = json_str(msg.get("id"));
            if id.is_empty() {
                continue;
            }

            let from_email = json_str(msg.get("from_email"));
            let from_name = json_str(msg.get("from"));
            let from_addr = if !from_name.is_empty() && from_name != from_email {
                format!("{} <{}>", from_name, from_email)
            } else {
                from_email
            };

            let is_seen = msg
                .get("is_seen")
                .and_then(|v| v.as_bool())
                .unwrap_or(false);
            let html_body = fetch_view(&client, &sess.cookie, &id).await;

            let raw = serde_json::json!({
                "id": id,
                "from": from_addr,
                "to": em,
                "subject": json_str(msg.get("subject")),
                "html": html_body,
                "is_read": is_seen,
            });
            result.push(normalize_email(&raw, em));
        }

        Ok(result)
    })
}

/// 获取单封邮件的 HTML 正文（GET /view/{id}）
async fn fetch_view(client: &wreq::Client, cookie_hdr: &str, id: &str) -> String {
    if id.is_empty() {
        return String::new();
    }

    let url = format!("{BASE_URL}/view/{}", urlencoding::encode(id));
    let resp = match client
        .get(&url)
        .header("User-Agent", get_current_ua())
        .header(
            "Accept",
            "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
        )
        .header("Accept-Language", "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7")
        .header("Referer", format!("{BASE_URL}/en"))
        .header("Cookie", cookie_hdr)
        .send()
        .await
    {
        Ok(r) if r.status().is_success() => r,
        _ => return String::new(),
    };

    resp.text().await.unwrap_or_default()
}

/// 安全提取 JSON 值为字符串（字符串原样、数字转整数文本）
fn json_str(v: Option<&Value>) -> String {
    match v {
        Some(v) if v.is_string() => v.as_str().unwrap_or("").trim().to_string(),
        Some(v) if v.is_number() => {
            if let Some(n) = v.as_i64() {
                n.to_string()
            } else if let Some(n) = v.as_u64() {
                n.to_string()
            } else {
                v.to_string()
            }
        }
        _ => String::new(),
    }
}
