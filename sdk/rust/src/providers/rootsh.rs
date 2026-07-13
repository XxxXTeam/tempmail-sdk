/*!
 * Rootsh(BccTo) 渠道实现
 * 网站: https://rootsh.com
 * 默认域名: bccto.cc
 * 可用域名: bccto.cc, mail.wvwvw.tech, x866.cc, mxl001.win, 51788.top, kya2.com 等
 *
 * 流程:
 *   1. GET https://rootsh.com/ 获取 session cookie ("mail" cookie)
 *   2. POST https://rootsh.com/applymail  body: mail={user}@{domain} (form-urlencoded)
 *      需要 X-Requested-With: XMLHttpRequest, Referer: https://rootsh.com/
 *      响应: {"delay":"10:00","tips":"","user":"xxx@bccto.cc","success":"true","time":1782762525}
 *   3. POST https://rootsh.com/getmail   body: mail={email}&time={lastCheckTime}&_={timestamp}ms
 *      响应: {"to":"xxx@bccto.cc","mail":[["sender_name","sender_email","subject","date","fid","time"]],"success":"true","time":1782762526}
 *   4. POST https://rootsh.com/viewmail  body: mail={fid}&to={email}
 *      响应: {"mail":"html_content","attachment":"","success":"true"}
 *
 * token 存储: "cookies\n{time}" 格式，time 用于增量获取邮件
 */

use crate::config::{block_on, http_client_no_cookie_jar};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use rand::Rng;
use serde_json::Value;
use std::time::{SystemTime, UNIX_EPOCH};

const BASE_URL: &str = "https://rootsh.com";
const DEFAULT_DOMAIN: &str = "bccto.cc";

/// 浏览器 User-Agent
fn browser_ua() -> &'static str {
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36"
}

/// 获取当前毫秒时间戳
fn now_ms() -> u64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_millis() as u64
}

/// 从响应头中提取 set-cookie 并拼接为 Cookie 字符串
fn extract_cookies(headers: &wreq::header::HeaderMap) -> String {
    let mut parts: Vec<String> = Vec::new();
    for value in headers.get_all("set-cookie") {
        let cookie_str = value.to_str().unwrap_or("");
        if let Some(name_value) = cookie_str.split(';').next() {
            let trimmed = name_value.trim();
            if !trimmed.is_empty() {
                parts.push(trimmed.to_string());
            }
        }
    }
    parts.join("; ")
}

/// 随机生成 10 位字母数字字符串作为邮箱本地部分
fn random_local() -> String {
    const CHARS: &[u8] = b"abcdefghijklmnopqrstuvwxyz0123456789";
    let mut rng = rand::thread_rng();
    (0..10)
        .map(|_| CHARS[rng.gen_range(0..CHARS.len())] as char)
        .collect()
}

/// 创建临时邮箱
/// 1. GET / 获取 session cookie
/// 2. POST /applymail 申请邮箱地址
/// token 格式: "cookies\n{time}"，time 用于后续增量获取邮件
pub fn generate_email() -> Result<EmailInfo, String> {
    let local = random_local();
    let addr = format!("{}@{}", local, DEFAULT_DOMAIN);

    block_on(async {
        /* 第一步：访问首页获取 session cookie */
        let resp = http_client_no_cookie_jar()
            .get(format!("{}/", BASE_URL))
            .header(
                "Accept",
                "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
            )
            .header("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
            .header("User-Agent", browser_ua())
            .send()
            .await
            .map_err(|e| format!("rootsh: 获取首页失败: {}", e))?;

        let cookies = extract_cookies(resp.headers());
        if cookies.is_empty() {
            return Err("rootsh: 未获取到 session cookie".into());
        }

        /* 第二步：申请邮箱 */
        let apply_resp = http_client_no_cookie_jar()
            .post(format!("{}/applymail", BASE_URL))
            .header("Content-Type", "application/x-www-form-urlencoded")
            .header("X-Requested-With", "XMLHttpRequest")
            .header("Referer", format!("{}/", BASE_URL))
            .header("Accept", "application/json, text/javascript, */*; q=0.01")
            .header("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
            .header("User-Agent", browser_ua())
            .header("Cookie", &cookies)
            .body(format!("mail={}", urlencoding::encode(&addr)))
            .send()
            .await
            .map_err(|e| format!("rootsh: 申请邮箱失败: {}", e))?;

        if !apply_resp.status().is_success() {
            return Err(format!("rootsh: 申请邮箱返回 HTTP {}", apply_resp.status()));
        }

        let body: Value = apply_resp
            .json()
            .await
            .map_err(|e| format!("rootsh: 解析申请响应失败: {}", e))?;

        /* 检查 success 字段 */
        let success = body.get("success").and_then(|v| v.as_str()).unwrap_or("");
        if success != "true" {
            return Err(format!(
                "rootsh: 申请邮箱失败, 响应: {}",
                serde_json::to_string(&body).unwrap_or_default()
            ));
        }

        /* 从响应中获取实际分配的邮箱地址 */
        let email = body
            .get("user")
            .and_then(|v| v.as_str())
            .unwrap_or("")
            .to_string();
        if email.is_empty() || !email.contains('@') {
            return Err("rootsh: 响应中未包含有效邮箱地址".into());
        }

        /* 获取 time 字段，用于后续增量获取邮件 */
        let time = body.get("time").and_then(|v| v.as_i64()).unwrap_or(0);

        /* token 格式: "cookies\n{time}" */
        let token = format!("{}\n{}", cookies, time);

        Ok(EmailInfo {
            channel: Channel::Rootsh,
            email,
            token: Some(token),
            expires_at: None,
            created_at: None,
        })
    })
}

/// 获取邮件列表
/// 1. POST /getmail 获取邮件列表（增量，基于 lastCheckTime）
/// 2. 对每封邮件 POST /viewmail 获取完整内容
///
/// token 格式: "cookies\n{time}"
pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    if token.is_empty() {
        return Err("rootsh: token 不能为空".into());
    }
    let em = email.trim();
    if em.is_empty() {
        return Err("rootsh: 邮箱地址为空".into());
    }

    /* 解析 token: "cookies\n{time}" */
    let (cookies, last_time) = match token.split_once('\n') {
        Some((c, t)) => (c.to_string(), t.to_string()),
        None => (token.to_string(), "0".to_string()),
    };

    block_on(async {
        let ts = now_ms();
        let body = format!(
            "mail={}&time={}&_={}",
            urlencoding::encode(em),
            urlencoding::encode(&last_time),
            ts
        );

        let resp = http_client_no_cookie_jar()
            .post(format!("{}/getmail", BASE_URL))
            .header("Content-Type", "application/x-www-form-urlencoded")
            .header("X-Requested-With", "XMLHttpRequest")
            .header("Referer", format!("{}/", BASE_URL))
            .header("Accept", "application/json, text/javascript, */*; q=0.01")
            .header("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
            .header("User-Agent", browser_ua())
            .header("Cookie", &cookies)
            .body(body)
            .send()
            .await
            .map_err(|e| format!("rootsh: 获取邮件列表失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("rootsh: 获取邮件列表返回 HTTP {}", resp.status()));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("rootsh: 解析邮件列表响应失败: {}", e))?;

        let success = data.get("success").and_then(|v| v.as_str()).unwrap_or("");
        if success != "true" {
            return Err(format!(
                "rootsh: 获取邮件列表失败, 响应: {}",
                serde_json::to_string(&data).unwrap_or_default()
            ));
        }

        /* mail 数组中每个元素: [display_name, from_email, subject, date_str, mail_fid, received_time] */
        let mail_arr = data
            .get("mail")
            .and_then(|v| v.as_array())
            .cloned()
            .unwrap_or_default();

        let mut result = Vec::new();
        for item in &mail_arr {
            let arr = match item.as_array() {
                Some(a) if a.len() >= 5 => a,
                _ => continue,
            };

            let display_name = arr[0].as_str().unwrap_or("");
            let from_email = arr[1].as_str().unwrap_or("");
            let subject = arr[2].as_str().unwrap_or("");
            let date_str = arr[3].as_str().unwrap_or("");
            let fid = arr[4].as_str().unwrap_or("");

            /* 构建发件人地址：如果有 display_name 则拼接 "name <email>" */
            let from_addr = if display_name.is_empty() {
                from_email.to_string()
            } else {
                format!("{} <{}>", display_name, from_email)
            };

            /* 获取邮件正文 */
            let html_content = fetch_mail_body(&cookies, fid, em).await.unwrap_or_default();

            let raw = serde_json::json!({
                "id": fid,
                "from": from_addr,
                "to": em,
                "subject": subject,
                "date": date_str,
                "text": "",
                "html": html_content,
                "isRead": false,
                "attachments": [],
            });
            result.push(normalize_email(&raw, em));
        }

        Ok(result)
    })
}

/// 获取单封邮件正文
/// POST /viewmail  body: mail={fid}&to={email}
async fn fetch_mail_body(cookies: &str, fid: &str, email: &str) -> Result<String, String> {
    let body = format!(
        "mail={}&to={}",
        urlencoding::encode(fid),
        urlencoding::encode(email)
    );

    let resp = http_client_no_cookie_jar()
        .post(format!("{}/viewmail", BASE_URL))
        .header("Content-Type", "application/x-www-form-urlencoded")
        .header("X-Requested-With", "XMLHttpRequest")
        .header("Referer", format!("{}/", BASE_URL))
        .header("Accept", "application/json, text/javascript, */*; q=0.01")
        .header("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
        .header("User-Agent", browser_ua())
        .header("Cookie", cookies)
        .body(body)
        .send()
        .await
        .map_err(|e| format!("rootsh: 获取邮件正文失败: {}", e))?;

    if !resp.status().is_success() {
        return Err(format!("rootsh: 获取邮件正文返回 HTTP {}", resp.status()));
    }

    let data: Value = resp
        .json()
        .await
        .map_err(|e| format!("rootsh: 解析邮件正文响应失败: {}", e))?;

    Ok(data
        .get("mail")
        .and_then(|v| v.as_str())
        .unwrap_or("")
        .to_string())
}
