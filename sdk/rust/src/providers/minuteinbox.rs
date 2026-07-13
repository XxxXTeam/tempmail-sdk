/*!
 * minuteinbox.com 渠道实现
 * 网站: https://www.minuteinbox.com
 *
 * 流程（Cookie Session + CSRF + REST JSON）:
 *   1. GET https://www.minuteinbox.com/
 *      返回 HTML 页面，保存 Cookie，正则提取 CSRF="xxx" 值
 *
 *   2. GET https://www.minuteinbox.com/index/index?csrf_token={CSRF}
 *      带上步骤1获取的 Cookie
 *      返回: {"email":"user@minafter.com"}
 *
 *   3. GET https://www.minuteinbox.com/index/refresh
 *      带上 Cookie
 *      返回: JSON数组 [{"predmet":"subject","od":"from","id":1,"kdy":"time","precteno":"new|precteno"}]
 *      空收件箱返回数字 0
 *
 *   4. GET https://www.minuteinbox.com/email/id/{id}
 *      带上 Cookie，返回完整 HTML 邮件正文
 *
 * token 存储: JSON 字符串 {"csrf":"...","cookie":"..."}
 * 字段说明（捷克语）: predmet=subject, od=from, id=邮件ID, kdy=when, precteno=read status
 */

use std::collections::BTreeMap;
use std::sync::LazyLock;

use regex::Regex;
use serde::{Deserialize, Serialize};
use serde_json::Value;

use crate::config::{block_on, get_current_ua, http_client_no_cookie_jar};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};

const ORIGIN: &str = "https://www.minuteinbox.com";

/// 匹配页面中的 CSRF 令牌
static CSRF_RE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r#"CSRF\s*=\s*"([^"]+)""#).expect("re"));

/// token 中序列化的会话信息
#[derive(Serialize, Deserialize)]
struct MinuteinboxSess {
    /// CSRF 令牌
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
fn encode_sess(s: &MinuteinboxSess) -> Result<String, String> {
    serde_json::to_string(s).map_err(|e| format!("minuteinbox: token 序列化失败: {}", e))
}

/// 从 token 字符串解码会话信息
fn decode_sess(tok: &str) -> Result<MinuteinboxSess, String> {
    let s: MinuteinboxSess =
        serde_json::from_str(tok).map_err(|_| "minuteinbox: token 反序列化失败".to_string())?;
    if s.csrf.is_empty() {
        return Err("minuteinbox: token 中 csrf 为空".into());
    }
    if s.cookie.is_empty() {
        return Err("minuteinbox: token 中 cookie 为空".into());
    }
    Ok(s)
}

/// 构建浏览器请求头
fn browser_headers(referer: &str) -> Vec<(&'static str, String)> {
    vec![
        ("User-Agent", get_current_ua().to_string()),
        (
            "Accept",
            "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,*/*;q=0.8"
                .to_string(),
        ),
        ("Accept-Language", "en-US,en;q=0.9".to_string()),
        ("Cache-Control", "no-cache".to_string()),
        ("Pragma", "no-cache".to_string()),
        ("Referer", referer.to_string()),
    ]
}

/// 构建 JSON 请求头
fn json_headers(referer: &str) -> Vec<(&'static str, String)> {
    vec![
        ("User-Agent", get_current_ua().to_string()),
        (
            "Accept",
            "application/json, text/javascript, */*; q=0.01".to_string(),
        ),
        ("Accept-Language", "en-US,en;q=0.9".to_string()),
        ("X-Requested-With", "XMLHttpRequest".to_string()),
        ("Referer", referer.to_string()),
    ]
}

/// 创建临时邮箱
/// 1. GET / → 获取 Cookie 和 CSRF 令牌
/// 2. GET /index/index?csrf_token={csrf} → 获取分配的邮箱地址
pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let client = http_client_no_cookie_jar();

        // 第一步：GET 首页，获取 Cookie 和 CSRF 令牌
        let mut req = client.get(format!("{}/", ORIGIN));
        for (k, v) in browser_headers(ORIGIN) {
            req = req.header(k, v);
        }
        let resp = req
            .send()
            .await
            .map_err(|e| format!("minuteinbox: 获取首页失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("minuteinbox: 首页返回 HTTP {}", resp.status()));
        }

        // 收集 Set-Cookie
        let cookie_hdr = merge_set_cookies("", resp.headers());
        let html = resp
            .text()
            .await
            .map_err(|e| format!("minuteinbox: 读取首页响应失败: {}", e))?;

        // 从 HTML 中提取 CSRF 令牌
        let csrf = CSRF_RE
            .captures(&html)
            .and_then(|c| c.get(1))
            .map(|m| m.as_str().to_string())
            .ok_or_else(|| "minuteinbox: 未找到 CSRF 令牌".to_string())?;

        if csrf.is_empty() {
            return Err("minuteinbox: CSRF 令牌为空".into());
        }

        // 第二步：GET /index/index?csrf_token={csrf} 获取邮箱地址
        let create_url = format!("{}/index/index?csrf_token={}", ORIGIN, csrf);
        let mut req2 = client.get(&create_url);
        for (k, v) in json_headers(&format!("{}/", ORIGIN)) {
            req2 = req2.header(k, v);
        }
        req2 = req2.header("Cookie", &cookie_hdr);
        let resp2 = req2
            .send()
            .await
            .map_err(|e| format!("minuteinbox: 创建邮箱请求失败: {}", e))?;

        if !resp2.status().is_success() {
            return Err(format!("minuteinbox: 创建邮箱返回 HTTP {}", resp2.status()));
        }

        // 更新 Cookie
        let cookie_hdr = merge_set_cookies(&cookie_hdr, resp2.headers());

        let body: Value = resp2
            .json()
            .await
            .map_err(|e| format!("minuteinbox: 解析创建邮箱响应失败: {}", e))?;

        let address = body
            .get("email")
            .and_then(|v| v.as_str())
            .map(|s| s.trim().to_string())
            .ok_or_else(|| "minuteinbox: 响应中未找到 email 字段".to_string())?;

        if address.is_empty() || !address.contains('@') {
            return Err(format!("minuteinbox: 返回的邮箱地址无效: {}", address));
        }

        let tok = encode_sess(&MinuteinboxSess {
            csrf,
            cookie: cookie_hdr,
        })?;

        Ok(EmailInfo {
            channel: Channel::Minuteinbox,
            email: address,
            token: Some(tok),
            expires_at: None,
            created_at: None,
        })
    })
}

/// 获取邮件列表
/// 1. GET /index/refresh → 获取邮件摘要列表
/// 2. 对每封邮件 GET /email/id/{id} → 获取完整 HTML 正文
pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let sess = decode_sess(token)?;
    let em = email.trim();
    if em.is_empty() {
        return Err("minuteinbox: 邮箱地址为空".into());
    }

    block_on(async {
        let client = http_client_no_cookie_jar();
        let page_url = format!("{}/", ORIGIN);

        // 第一步：GET /index/refresh 获取邮件列表
        let refresh_url = format!("{}/index/refresh", ORIGIN);
        let mut req = client.get(&refresh_url);
        for (k, v) in json_headers(&page_url) {
            req = req.header(k, v);
        }
        req = req.header("Cookie", &sess.cookie);
        let resp = req
            .send()
            .await
            .map_err(|e| format!("minuteinbox: 获取邮件列表请求失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!(
                "minuteinbox: 获取邮件列表返回 HTTP {}",
                resp.status()
            ));
        }

        let body_text = resp
            .text()
            .await
            .map_err(|e| format!("minuteinbox: 读取邮件列表响应失败: {}", e))?;

        // 空收件箱返回数字 0
        let trimmed = body_text.trim();
        if trimmed == "0" || trimmed.is_empty() {
            return Ok(vec![]);
        }

        let data: Value = serde_json::from_str(trimmed)
            .map_err(|e| format!("minuteinbox: 解析邮件列表 JSON 失败: {}", e))?;

        let items = match data.as_array() {
            Some(arr) => arr,
            None => return Ok(vec![]),
        };

        if items.is_empty() {
            return Ok(vec![]);
        }

        // 第二步：逐封获取邮件详情
        let mut result = Vec::with_capacity(items.len());
        for item in items {
            let mail_id = match item.get("id") {
                Some(v) => {
                    if let Some(n) = v.as_i64() {
                        n.to_string()
                    } else if let Some(n) = v.as_u64() {
                        n.to_string()
                    } else if let Some(s) = v.as_str() {
                        s.to_string()
                    } else {
                        continue;
                    }
                }
                None => continue,
            };

            // 从列表项提取基本信息（捷克语字段名）
            let subject = item.get("predmet").and_then(|v| v.as_str()).unwrap_or("");
            let from = item.get("od").and_then(|v| v.as_str()).unwrap_or("");
            let date = item.get("kdy").and_then(|v| v.as_str()).unwrap_or("");
            let is_read = item
                .get("precteno")
                .and_then(|v| v.as_str())
                .map(|s| s == "precteno")
                .unwrap_or(false);

            // GET /email/id/{id} 获取完整 HTML 正文
            let detail_url = format!("{}/email/id/{}", ORIGIN, mail_id);
            let mut reqd = client.get(&detail_url);
            for (k, v) in json_headers(&page_url) {
                reqd = reqd.header(k, v);
            }
            reqd = reqd.header("Cookie", &sess.cookie);

            let html_body = match reqd.send().await {
                Ok(r) if r.status().is_success() => r.text().await.unwrap_or_default(),
                _ => String::new(),
            };

            let raw = serde_json::json!({
                "id": mail_id,
                "from": from,
                "to": em,
                "subject": subject,
                "html": html_body,
                "date": date,
                "is_read": is_read,
                "attachments": [],
            });
            result.push(normalize_email(&raw, em));
        }

        Ok(result)
    })
}
