/*!
 * tmail.link 渠道实现
 * 网站: https://tmail.link
 *
 * 流程（HTML 提取邮箱 + Django CSRF Cookie）:
 *   1. GET https://tmail.link/
 *      返回 HTML 页面，正则提取 xxx@tmail.link 邮箱地址
 *   2. GET https://tmail.link/inbox/{email}/
 *      从响应头 Set-Cookie 中提取 csrftoken 值，作为 token
 *   3. POST https://tmail.link/inbox/{email}/
 *      form data: format=json + csrfmiddlewaretoken={token}
 *      Cookie: csrftoken={token}，Referer: inbox URL
 *      返回 JSON {"messages":[{key,sender,subject,date}]}
 *
 * token 存储: csrftoken 值（字符串）
 */

use std::sync::LazyLock;

use regex::Regex;

use crate::config::{block_on, get_current_ua, http_client_no_cookie_jar};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};

const ORIGIN: &str = "https://tmail.link";

/// 匹配页面中的邮箱地址（格式: xxx@tmail.link）
static EMAIL_RE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r"([a-zA-Z0-9._%+-]+@tmail\.link)").expect("re"));

/// 从响应头的 Set-Cookie 中提取指定 cookie 的值
fn extract_cookie(headers: &wreq::header::HeaderMap, name: &str) -> Option<String> {
    for val in headers.get_all("set-cookie") {
        let Ok(s) = val.to_str() else {
            continue;
        };
        let first = s.split(';').next().unwrap_or("");
        if let Some(i) = first.find('=') {
            let k = first[..i].trim();
            let v = first[i + 1..].trim();
            if k == name && !v.is_empty() {
                return Some(v.to_string());
            }
        }
    }
    None
}

/// 构建浏览器请求头（页面 GET）
fn browser_headers() -> Vec<(&'static str, String)> {
    vec![
        ("User-Agent", get_current_ua().to_string()),
        (
            "Accept",
            "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8"
                .to_string(),
        ),
        (
            "Accept-Language",
            "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7".to_string(),
        ),
    ]
}

/// 创建临时邮箱
/// 1. GET / → 正则提取邮箱地址
/// 2. GET /inbox/{email}/ → 从 Set-Cookie 提取 csrftoken 作为 token
pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let client = http_client_no_cookie_jar();

        // 第一步：GET 首页，正则提取邮箱地址
        let mut req = client.get(format!("{}/", ORIGIN));
        for (k, v) in browser_headers() {
            req = req.header(k, v);
        }
        let resp = req
            .send()
            .await
            .map_err(|e| format!("tmail-link: 获取首页失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("tmail-link: 首页返回 HTTP {}", resp.status()));
        }

        let html = resp
            .text()
            .await
            .map_err(|e| format!("tmail-link: 读取首页响应失败: {}", e))?;

        let address = EMAIL_RE
            .captures(&html)
            .and_then(|c| c.get(1))
            .map(|m| m.as_str().to_string())
            .filter(|s| !s.is_empty())
            .ok_or_else(|| "tmail-link: 未能从首页提取邮箱地址".to_string())?;

        // 第二步：GET /inbox/{email}/ 获取 csrftoken
        let inbox_url = format!("{}/inbox/{}/", ORIGIN, address);
        let mut req2 = client.get(&inbox_url);
        for (k, v) in browser_headers() {
            req2 = req2.header(k, v);
        }
        req2 = req2.header("Referer", format!("{}/", ORIGIN));
        let resp2 = req2
            .send()
            .await
            .map_err(|e| format!("tmail-link: 访问收件箱失败: {}", e))?;

        if !resp2.status().is_success() {
            return Err(format!(
                "tmail-link: 访问收件箱返回 HTTP {}",
                resp2.status()
            ));
        }

        let csrftoken = extract_cookie(resp2.headers(), "csrftoken")
            .ok_or_else(|| "tmail-link: 未能从响应头提取 csrftoken".to_string())?;

        Ok(EmailInfo {
            channel: Channel::TmailLink,
            email: address,
            token: Some(csrftoken),
            expires_at: None,
            created_at: None,
        })
    })
}

/// 获取邮件列表
/// POST /inbox/{email}/ 带 form: format=json + csrfmiddlewaretoken={token}
pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let em = email.trim();
    if em.is_empty() {
        return Err("tmail-link: 邮箱地址为空".into());
    }
    if token.is_empty() {
        return Err("tmail-link: token 为空".into());
    }

    block_on(async {
        let client = http_client_no_cookie_jar();
        let inbox_url = format!("{}/inbox/{}/", ORIGIN, em);
        let body = format!("format=json&csrfmiddlewaretoken={}", token);

        let mut req = client.post(&inbox_url);
        for (k, v) in browser_headers() {
            req = req.header(k, v);
        }
        req = req.header("Content-Type", "application/x-www-form-urlencoded");
        req = req.header("X-Requested-With", "XMLHttpRequest");
        req = req.header("Origin", ORIGIN);
        req = req.header("Referer", &inbox_url);
        req = req.header("Cookie", format!("csrftoken={}", token));

        let resp = req
            .body(body)
            .send()
            .await
            .map_err(|e| format!("tmail-link: 获取邮件请求失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("tmail-link: 获取邮件返回 HTTP {}", resp.status()));
        }

        let data: serde_json::Value = resp
            .json()
            .await
            .map_err(|e| format!("tmail-link: 解析邮件 JSON 失败: {}", e))?;

        let messages = match data["messages"].as_array() {
            Some(arr) if !arr.is_empty() => arr,
            _ => return Ok(vec![]),
        };

        let mut result = Vec::with_capacity(messages.len());
        for item in messages {
            let raw = serde_json::json!({
                "id": item["key"].as_str().unwrap_or(""),
                "from": item["sender"].as_str().unwrap_or(""),
                "to": em,
                "subject": item["subject"].as_str().unwrap_or(""),
                "date": item["date"].as_str().unwrap_or(""),
                "isRead": false,
            });
            result.push(normalize_email(&raw, em));
        }

        Ok(result)
    })
}
