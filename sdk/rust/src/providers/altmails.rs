/*!
 * altmails 渠道实现
 * 网站: https://tempmail.altmails.com
 * 域名: @altmails.com
 *
 * 流程（Cookie Session + CSRF + REST）:
 *   1. GET https://tempmail.altmails.com/
 *      保存 Cookie，从 HTML 的 inline script 中提取 CSRF token:
 *      '_token': 'xxx'
 *
 *   2. GET https://tempmail.altmails.com/random-email-address
 *      需要带 Cookie
 *      返回: 纯文本邮箱地址，如 "username@altmails.com"
 *
 *   3. POST https://tempmail.altmails.com/fetch-emails/{email}
 *      Headers: X-Requested-With: XMLHttpRequest, Accept: application/json
 *      Body: _token={csrf}（form-urlencoded）
 *      返回: JSON 数组 [{id: 1, from: "sender@xxx", subject: "xxx"}, ...]
 *      空数组 [] 表示无邮件
 *
 *   4. GET https://tempmail.altmails.com/view/{id}
 *      返回 HTML 页面，包含邮件正文
 *
 * token 存储: JSON 字符串 {"csrf":"...","cookie":"..."}
 */

use std::collections::BTreeMap;
use std::sync::LazyLock;

use regex::Regex;
use serde::{Deserialize, Serialize};
use serde_json::Value;

use crate::config::{block_on, get_current_ua, http_client_no_cookie_jar};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};

const ORIGIN: &str = "https://tempmail.altmails.com";

/// 匹配 HTML inline script 中 '_token': 'xxx' 的 CSRF token
static CSRF_RE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r#"'_token'\s*:\s*'([^']+)'"#).expect("re"));

/// token 中序列化的会话信息
#[derive(Serialize, Deserialize)]
struct AltmailsSess {
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
        // 只取第一段 key=value，忽略 Path、Expires 等属性
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
fn encode_sess(s: &AltmailsSess) -> Result<String, String> {
    serde_json::to_string(s).map_err(|e| format!("altmails: token 序列化失败: {}", e))
}

/// 从 token 字符串解码会话信息
fn decode_sess(tok: &str) -> Result<AltmailsSess, String> {
    let s: AltmailsSess =
        serde_json::from_str(tok).map_err(|_| "altmails: token 反序列化失败".to_string())?;
    if s.csrf.is_empty() {
        return Err("altmails: token 中 csrf 为空".into());
    }
    if s.cookie.is_empty() {
        return Err("altmails: token 中 cookie 为空".into());
    }
    Ok(s)
}

/// 构建浏览器请求头（GET HTML 页面）
fn browser_headers() -> Vec<(&'static str, String)> {
    vec![
        ("User-Agent", get_current_ua().to_string()),
        (
            "Accept",
            "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,*/*;q=0.8"
                .to_string(),
        ),
        ("Accept-Language", "en-US,en;q=0.9".to_string()),
    ]
}

/// 构建 AJAX 请求头（POST API）
fn ajax_headers() -> Vec<(&'static str, String)> {
    vec![
        ("User-Agent", get_current_ua().to_string()),
        ("Accept", "application/json".to_string()),
        ("Accept-Language", "en-US,en;q=0.9".to_string()),
        ("X-Requested-With", "XMLHttpRequest".to_string()),
        ("Referer", format!("{}/", ORIGIN)),
    ]
}

/// 创建临时邮箱
/// 1. GET / → 获取 Cookie 和 CSRF token
/// 2. GET /random-email-address → 纯文本邮箱地址
pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let client = http_client_no_cookie_jar();

        // 第一步：GET / 获取 session Cookie 和 CSRF token
        let mut req = client.get(format!("{}/", ORIGIN));
        for (k, v) in browser_headers() {
            req = req.header(k, v);
        }
        let resp = req
            .send()
            .await
            .map_err(|e| format!("altmails: 获取首页请求失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("altmails: 获取首页返回 HTTP {}", resp.status()));
        }

        // 收集 Set-Cookie
        let cookie_hdr = merge_set_cookies("", resp.headers());

        let html = resp
            .text()
            .await
            .map_err(|e| format!("altmails: 读取首页响应失败: {}", e))?;

        // 从 HTML inline script 中提取 CSRF token: '_token': 'xxx'
        let csrf = CSRF_RE
            .captures(&html)
            .and_then(|cap| cap.get(1).map(|m| m.as_str().to_string()))
            .ok_or_else(|| "altmails: 未找到 CSRF token".to_string())?;

        // 第二步：GET /random-email-address 获取随机邮箱地址
        let mut req = client.get(format!("{}/random-email-address", ORIGIN));
        for (k, v) in browser_headers() {
            req = req.header(k, v);
        }
        req = req.header("Cookie", &cookie_hdr);
        let resp = req
            .send()
            .await
            .map_err(|e| format!("altmails: 获取邮箱地址请求失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("altmails: 获取邮箱地址返回 HTTP {}", resp.status()));
        }

        // 合并可能的新 Cookie
        let cookie_hdr = merge_set_cookies(&cookie_hdr, resp.headers());

        let address = resp
            .text()
            .await
            .map_err(|e| format!("altmails: 读取邮箱地址响应失败: {}", e))?
            .trim()
            .to_string();

        if address.is_empty() || !address.contains('@') {
            return Err(format!("altmails: 返回的邮箱地址无效: {}", address));
        }

        let tok = encode_sess(&AltmailsSess {
            csrf,
            cookie: cookie_hdr,
        })?;

        Ok(EmailInfo {
            channel: Channel::Altmails,
            email: address,
            token: Some(tok),
            expires_at: None,
            created_at: None,
        })
    })
}

/// 获取邮件列表
/// 1. POST /fetch-emails/{email} → JSON 数组
/// 2. 对每封邮件调用 GET /view/{id} → 获取 HTML 正文
pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let sess = decode_sess(token)?;
    let em = email.trim();
    if em.is_empty() {
        return Err("altmails: 邮箱地址为空".into());
    }

    block_on(async {
        let client = http_client_no_cookie_jar();

        // POST /fetch-emails/{email} 获取邮件列表
        let fetch_url = format!("{}/fetch-emails/{}", ORIGIN, em);
        let mut req = client.post(&fetch_url);
        for (k, v) in ajax_headers() {
            req = req.header(k, v);
        }
        req = req.header("Cookie", &sess.cookie);
        req = req.header("Content-Type", "application/x-www-form-urlencoded");
        req = req.body(format!("_token={}", sess.csrf));

        let resp = req
            .send()
            .await
            .map_err(|e| format!("altmails: 获取邮件列表请求失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("altmails: 获取邮件列表返回 HTTP {}", resp.status()));
        }

        let body: Value = resp
            .json()
            .await
            .map_err(|e| format!("altmails: 解析邮件列表响应失败: {}", e))?;

        // 响应为 JSON 数组
        let messages = match body.as_array() {
            Some(arr) if !arr.is_empty() => arr.clone(),
            _ => return Ok(vec![]),
        };

        // 逐封邮件获取正文
        let mut result = Vec::with_capacity(messages.len());
        for msg in &messages {
            let id = msg
                .get("id")
                .map(|v| {
                    if let Some(n) = v.as_u64() {
                        n.to_string()
                    } else if let Some(s) = v.as_str() {
                        s.to_string()
                    } else {
                        v.to_string()
                    }
                })
                .unwrap_or_default();

            let from = msg.get("from").and_then(|v| v.as_str()).unwrap_or("");
            let subject = msg.get("subject").and_then(|v| v.as_str()).unwrap_or("");
            let date = msg
                .get("created_at")
                .or_else(|| msg.get("date"))
                .and_then(|v| v.as_str())
                .unwrap_or("");

            // GET /view/{id} 获取邮件 HTML 正文
            let mut html_body = String::new();
            if !id.is_empty() {
                let view_url = format!("{}/view/{}", ORIGIN, id);
                let mut reqv = client.get(&view_url);
                for (k, v) in browser_headers() {
                    reqv = reqv.header(k, v);
                }
                reqv = reqv.header("Cookie", &sess.cookie);

                if let Ok(view_resp) = reqv.send().await {
                    if view_resp.status().is_success() {
                        html_body = view_resp.text().await.unwrap_or_default();
                    }
                }
            }

            let raw = serde_json::json!({
                "id": id,
                "from": from,
                "to": em,
                "subject": subject,
                "html": html_body,
                "text": "",
                "date": date,
                "isRead": false,
                "attachments": [],
            });
            result.push(normalize_email(&raw, em));
        }

        Ok(result)
    })
}
