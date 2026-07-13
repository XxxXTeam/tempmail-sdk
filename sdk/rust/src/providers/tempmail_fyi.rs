/*!
 * temp-mail.fyi 渠道实现
 * 网站: https://temp-mail.fyi
 *
 * 流程:
 *   1. GET / 获取 PHPSESSID cookie + 从 HTML 正则提取 csrfToken
 *      （格式: csrfToken" value="xxx"）
 *   2. POST /api/generate_email.php 创建邮箱（需 X-CSRF-Token 头 + session cookie）
 *      返回 {"success":true,"email_address":"xxx@...","expires_at":"...","error":null}
 *   3. POST /api/get_emails.php 获取邮件列表（body 携带 email_address）
 *      返回 {"success":true,"emails":[...],"error":null}
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

const BASE_URL: &str = "https://temp-mail.fyi";

/// 从 HTML 中提取 CSRF token（格式: csrfToken" value="xxx"）
static CSRF_RE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r#"csrfToken"\s*value="([^"]+)""#).expect("re"));

/// token 中序列化的会话信息
#[derive(Serialize, Deserialize)]
struct TempmailFyiSess {
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
fn encode_sess(s: &TempmailFyiSess) -> Result<String, String> {
    serde_json::to_string(s).map_err(|e| format!("tempmail-fyi: token 序列化失败: {}", e))
}

/// 从 token 字符串解码会话信息
fn decode_sess(tok: &str) -> Result<TempmailFyiSess, String> {
    let s: TempmailFyiSess =
        serde_json::from_str(tok).map_err(|_| "tempmail-fyi: token 反序列化失败".to_string())?;
    if s.csrf.is_empty() {
        return Err("tempmail-fyi: token 中 csrf 为空".into());
    }
    if s.cookie.is_empty() {
        return Err("tempmail-fyi: token 中 cookie 为空".into());
    }
    Ok(s)
}

/// 创建临时邮箱
/// 1. GET / 获取 PHPSESSID cookie 和 CSRF token
/// 2. POST /api/generate_email.php 创建邮箱
pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let client = http_client_no_cookie_jar();

        // 第一步：GET / 获取 session cookie 和 CSRF token
        let resp = client
            .get(format!("{BASE_URL}/"))
            .header("User-Agent", get_current_ua())
            .header(
                "Accept",
                "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
            )
            .header("Accept-Language", "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7")
            .send()
            .await
            .map_err(|e| format!("tempmail-fyi: 获取首页失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("tempmail-fyi: 首页返回 HTTP {}", resp.status()));
        }

        let cookie_hdr = merge_set_cookies("", resp.headers());
        let html = resp
            .text()
            .await
            .map_err(|e| format!("tempmail-fyi: 读取首页响应失败: {}", e))?;

        let csrf = CSRF_RE
            .captures(&html)
            .and_then(|c| c.get(1))
            .map(|m| m.as_str().to_string())
            .filter(|s| !s.is_empty())
            .ok_or_else(|| "tempmail-fyi: 未能从首页提取 CSRF token".to_string())?;

        // 第二步：POST /api/generate_email.php 创建邮箱
        let resp2 = client
            .post(format!("{BASE_URL}/api/generate_email.php"))
            .header("User-Agent", get_current_ua())
            .header("Accept", "application/json, text/javascript, */*; q=0.01")
            .header("Accept-Language", "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7")
            .header("Content-Type", "application/json")
            .header("X-CSRF-Token", &csrf)
            .header("X-Requested-With", "XMLHttpRequest")
            .header("Referer", format!("{BASE_URL}/"))
            .header("Cookie", &cookie_hdr)
            .body("{}")
            .send()
            .await
            .map_err(|e| format!("tempmail-fyi: 创建邮箱失败: {}", e))?;

        if !resp2.status().is_success() {
            return Err(format!(
                "tempmail-fyi: 创建邮箱返回 HTTP {}",
                resp2.status()
            ));
        }

        let cookie_hdr = merge_set_cookies(&cookie_hdr, resp2.headers());
        let data: Value = resp2
            .json()
            .await
            .map_err(|e| format!("tempmail-fyi: 解析创建邮箱响应失败: {}", e))?;

        if !data
            .get("success")
            .and_then(|v| v.as_bool())
            .unwrap_or(false)
        {
            let err = data.get("error").and_then(|v| v.as_str()).unwrap_or("");
            if !err.is_empty() {
                return Err(format!("tempmail-fyi: 创建邮箱失败: {}", err));
            }
            return Err("tempmail-fyi: 创建邮箱失败（success=false）".into());
        }

        let email = data
            .get("email_address")
            .and_then(|v| v.as_str())
            .map(|s| s.trim().to_string())
            .unwrap_or_default();
        if email.is_empty() || !email.contains('@') {
            return Err(format!("tempmail-fyi: 获取到的邮箱地址无效: {}", email));
        }

        let expires_at = data
            .get("expires_at")
            .and_then(|v| v.as_str())
            .and_then(|s| chrono::DateTime::parse_from_rfc3339(s).ok())
            .map(|dt| dt.timestamp_millis());

        let tok = encode_sess(&TempmailFyiSess {
            csrf,
            cookie: cookie_hdr,
        })?;

        Ok(EmailInfo {
            channel: Channel::TempmailFyi,
            email,
            token: Some(tok),
            expires_at,
            created_at: None,
        })
    })
}

/// 获取邮件列表
/// POST /api/get_emails.php，body 携带 {"email_address":"xxx@..."}
pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let em = email.trim();
    if em.is_empty() {
        return Err("tempmail-fyi: 邮箱地址为空".into());
    }
    let sess = decode_sess(token)?;

    block_on(async {
        let client = http_client_no_cookie_jar();
        let req_body = serde_json::json!({ "email_address": em }).to_string();

        let resp = client
            .post(format!("{BASE_URL}/api/get_emails.php"))
            .header("User-Agent", get_current_ua())
            .header("Accept", "application/json, text/javascript, */*; q=0.01")
            .header("Accept-Language", "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7")
            .header("Content-Type", "application/json")
            .header("X-CSRF-Token", &sess.csrf)
            .header("X-Requested-With", "XMLHttpRequest")
            .header("Referer", format!("{BASE_URL}/"))
            .header("Cookie", &sess.cookie)
            .body(req_body)
            .send()
            .await
            .map_err(|e| format!("tempmail-fyi: 获取邮件列表失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!(
                "tempmail-fyi: 获取邮件列表返回 HTTP {}",
                resp.status()
            ));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("tempmail-fyi: 解析邮件列表失败: {}", e))?;

        if !data
            .get("success")
            .and_then(|v| v.as_bool())
            .unwrap_or(false)
        {
            let err = data.get("error").and_then(|v| v.as_str()).unwrap_or("");
            if !err.is_empty() {
                return Err(format!("tempmail-fyi: 获取邮件列表失败: {}", err));
            }
            return Err("tempmail-fyi: 获取邮件列表失败（success=false）".into());
        }

        let emails = match data.get("emails").and_then(|v| v.as_array()) {
            Some(arr) if !arr.is_empty() => arr,
            _ => return Ok(vec![]),
        };

        let mut result = Vec::with_capacity(emails.len());
        for item in emails {
            result.push(normalize_email(item, em));
        }

        Ok(result)
    })
}
