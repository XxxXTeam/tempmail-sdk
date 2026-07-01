/*!
 * tempemail.info 渠道实现
 * 网站: https://tempemail.info
 *
 * 流程:
 *   1. GET / 获取 PHPSESSID 会话 Cookie，并从 HTML 中正则提取
 *      var emailEncoded = "base64..."，base64 解码得到邮箱地址
 *   2. POST /template/checker.php（body: last_id=0）获取邮件列表（JSON 数组）
 *   3. GET /view/{date} 获取单封邮件 HTML 正文
 *
 * token 存储: 会话 Cookie 串，用于后续请求绑定收件箱
 */

use std::collections::BTreeMap;
use std::sync::LazyLock;

use base64::Engine;
use regex::Regex;
use serde_json::Value;

use crate::config::{block_on, get_current_ua, http_client_no_cookie_jar};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};

const BASE_URL: &str = "https://tempemail.info";

/// 匹配首页 HTML 中的 var emailEncoded = "base64..."
static EMAIL_RE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r#"var\s+emailEncoded\s*=\s*"([^"]+)""#).expect("re"));

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

/// 创建临时邮箱
/// 1. GET / 获取 PHPSESSID 会话 Cookie
/// 2. 从 HTML 中正则提取 emailEncoded 并 base64 解码得到邮箱地址
///
/// token 存储会话 Cookie 串，供后续 checker.php / view 请求使用
pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let client = http_client_no_cookie_jar();

        let resp = client
            .get(format!("{BASE_URL}/"))
            .header("User-Agent", get_current_ua())
            .header(
                "Accept",
                "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
            )
            .header("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
            .header("Referer", format!("{BASE_URL}/"))
            .header("Origin", BASE_URL)
            .send()
            .await
            .map_err(|e| format!("tempemail-info: 获取首页失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("tempemail-info: 首页返回 HTTP {}", resp.status()));
        }

        // 提取会话 Cookie，绑定后续请求的收件箱
        let cookie_hdr = merge_set_cookies("", resp.headers());
        if cookie_hdr.is_empty() {
            return Err("tempemail-info: 未获取到会话 Cookie".into());
        }

        let html = resp
            .text()
            .await
            .map_err(|e| format!("tempemail-info: 读取首页响应失败: {}", e))?;

        // 从 HTML 中正则提取 base64 编码的邮箱地址
        let encoded = EMAIL_RE
            .captures(&html)
            .and_then(|c| c.get(1))
            .map(|m| m.as_str().to_string())
            .ok_or_else(|| "tempemail-info: 未在页面中找到 emailEncoded".to_string())?;

        let decoded = base64::engine::general_purpose::STANDARD
            .decode(encoded.as_bytes())
            .map_err(|e| format!("tempemail-info: 邮箱地址 base64 解码失败: {}", e))?;
        let email = String::from_utf8_lossy(&decoded).trim().to_string();
        if email.is_empty() || !email.contains('@') {
            return Err("tempemail-info: 解码出的邮箱地址无效".into());
        }

        Ok(EmailInfo {
            channel: Channel::TempemailInfo,
            email,
            token: Some(cookie_hdr),
            expires_at: None,
            created_at: None,
        })
    })
}

/// 获取邮件列表
/// 1. POST /template/checker.php（body: last_id=0）获取邮件列表 JSON 数组
/// 2. 对每封邮件 GET /view/{date} 获取 HTML 正文
pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let cookie_hdr = token.trim();
    if cookie_hdr.is_empty() {
        return Err("tempemail-info: 缺少会话 Cookie".into());
    }

    block_on(async {
        let client = http_client_no_cookie_jar();

        let resp = client
            .post(format!("{BASE_URL}/template/checker.php"))
            .header("User-Agent", get_current_ua())
            .header("Accept", "application/json, text/javascript, */*; q=0.01")
            .header("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
            .header("X-Requested-With", "XMLHttpRequest")
            .header("Content-Type", "application/x-www-form-urlencoded")
            .header("Referer", format!("{BASE_URL}/"))
            .header("Origin", BASE_URL)
            .header("Cookie", cookie_hdr)
            .body("last_id=0")
            .send()
            .await
            .map_err(|e| format!("tempemail-info: 获取邮件列表失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!(
                "tempemail-info: 获取邮件列表返回 HTTP {}",
                resp.status()
            ));
        }

        let body_text = resp
            .text()
            .await
            .map_err(|e| format!("tempemail-info: 读取邮件列表响应失败: {}", e))?;

        // checker.php 返回邮件对象数组，字段：id / name / from / subject / date / read
        // 其中 date 既是显示日期，又是 /view/{date} 正文接口的路径键
        let data: Value = serde_json::from_str(body_text.trim())
            .map_err(|e| format!("tempemail-info: 解析邮件列表失败: {}", e))?;

        let rows = match data.as_array() {
            Some(arr) if !arr.is_empty() => arr,
            _ => return Ok(vec![]),
        };

        let mut result = Vec::with_capacity(rows.len());
        for row in rows {
            let id = match row.get("id") {
                Some(v) if v.is_string() => v.as_str().unwrap_or("").to_string(),
                Some(v) if v.is_number() => v.to_string(),
                _ => String::new(),
            };
            let from = row.get("from").and_then(|v| v.as_str()).unwrap_or("");
            let name = row.get("name").and_then(|v| v.as_str()).unwrap_or("");
            let subject = row.get("subject").and_then(|v| v.as_str()).unwrap_or("");
            let date = row.get("date").and_then(|v| v.as_str()).unwrap_or("");
            let is_read = row.get("read").and_then(|v| v.as_bool()).unwrap_or(false);

            // 构造发件人地址：有显示名则格式化为 "name <email>"
            let from_addr = if !name.is_empty() && name != from {
                format!("{} <{}>", name, from)
            } else {
                from.to_string()
            };

            // GET /view/{date} 获取邮件正文
            let html_body = fetch_body(&client, cookie_hdr, date).await;

            let raw = serde_json::json!({
                "id": id,
                "from": from_addr,
                "to": email,
                "subject": subject,
                "date": date,
                "html": html_body,
                "is_read": is_read,
            });
            result.push(normalize_email(&raw, email));
        }

        Ok(result)
    })
}

/// 获取单封邮件的 HTML 正文（GET /view/{date}，date 作为路径段需 URL 编码）
async fn fetch_body(client: &wreq::Client, cookie_hdr: &str, date: &str) -> String {
    if date.trim().is_empty() {
        return String::new();
    }

    let url = format!("{BASE_URL}/view/{}", urlencoding::encode(date));
    let resp = match client
        .get(&url)
        .header("User-Agent", get_current_ua())
        .header(
            "Accept",
            "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
        )
        .header("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
        .header("Referer", format!("{BASE_URL}/"))
        .header("Origin", BASE_URL)
        .header("Cookie", cookie_hdr)
        .send()
        .await
    {
        Ok(r) if r.status().is_success() => r,
        _ => return String::new(),
    };

    resp.text().await.unwrap_or_default()
}
