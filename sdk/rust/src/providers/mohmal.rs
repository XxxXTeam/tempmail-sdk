/*!
 * mohmal.com: connect.sid session cookie + HTML 收件箱
 * 通过 GET /en/create/random 创建邮箱，从 HTML 中提取 data-email 属性获取邮箱地址
 * 通过 GET /en/inbox 获取邮件列表，解析 #inbox-table 表格行
 * 通过 GET /en/message/{id} 获取邮件详情，从 HTML 中提取邮件内容
 */

use std::collections::BTreeMap;
use std::sync::LazyLock;

use regex::Regex;
use serde::{Deserialize, Serialize};
use serde_json::{json, Value};

use crate::config::{block_on, get_current_ua, http_client_no_cookie_jar};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};

const ORIGIN: &str = "https://www.mohmal.com";
/// token 前缀，用于序列化 / 反序列化会话信息
const TOK_PREFIX: &str = "mohmal1:";

/// 序列化到 token 中的会话信息
#[derive(Serialize, Deserialize)]
struct MohmalSess {
    /// Cookie 字符串
    c: String,
}

/// 匹配 data-email 属性
static DATA_EMAIL_RE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r#"data-email="([^"]+)""#).expect("re"));

/// 匹配收件箱表格行中的邮件链接
static MSG_LINK_RE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r#"href="/en/message/([^"]+)""#).expect("re"));

/// 匹配表格行
static TR_RE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r#"(?is)<tr[^>]*>([\s\S]*?)</tr>"#).expect("re"));

/// 匹配表格单元格
static TD_RE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r#"(?is)<td[^>]*>([\s\S]*?)</td>"#).expect("re"));

/// 匹配 HTML 标签
static TAG_RE: LazyLock<Regex> = LazyLock::new(|| Regex::new(r#"<[^>]+>"#).expect("re"));

/// 匹配邮件详情页内容区域（开标签，用于栈式解析）
static MSG_BODY_OPEN_RE: LazyLock<Regex> = LazyLock::new(|| {
    Regex::new(r#"(?is)<div[^>]*class="[^"]*mail-content[^"]*"[^>]*>"#).expect("re")
});

/// 匹配邮件详情页的主题
static MSG_SUBJECT_RE: LazyLock<Regex> = LazyLock::new(|| {
    Regex::new(r#"(?is)<h4[^>]*class="[^"]*subject[^"]*"[^>]*>([\s\S]*?)</h4>"#).expect("re")
});

/// 匹配邮件详情页的发件人
static MSG_FROM_RE: LazyLock<Regex> = LazyLock::new(|| {
    Regex::new(r#"(?is)<span[^>]*class="[^"]*from[^"]*"[^>]*>([\s\S]*?)</span>"#).expect("re")
});

/// 匹配邮件详情页的日期
static MSG_DATE_RE: LazyLock<Regex> = LazyLock::new(|| {
    Regex::new(r#"(?is)<span[^>]*class="[^"]*date[^"]*"[^>]*>([\s\S]*?)</span>"#).expect("re")
});

/// 匹配 iframe src 中的 HTML 内容地址
static IFRAME_SRC_RE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r#"(?is)<iframe[^>]*src="([^"]+)"[^>]*>"#).expect("re"));

/// 使用栈式深度匹配提取 mail-content div 的完整内部 HTML，
/// 避免非贪婪正则在嵌套 div 时截断正文。
fn extract_mail_body(page: &str) -> String {
    let Some(m) = MSG_BODY_OPEN_RE.find(page) else {
        return String::new();
    };
    let start = m.end();
    let mut pos = start;
    let mut depth = 1usize;
    while pos < page.len() && depth > 0 {
        let rest = &page[pos..];
        let next_open = rest.find("<div").map(|i| pos + i);
        let next_close = rest.find("</div>").map(|i| pos + i);
        match (next_open, next_close) {
            (_, None) => break,
            (Some(no), Some(nc)) if no < nc => {
                depth += 1;
                pos = no + 4;
            }
            (_, Some(nc)) => {
                depth -= 1;
                if depth == 0 {
                    return page[start..nc].trim().to_string();
                }
                pos = nc + 6;
            }
        }
    }
    String::new()
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
fn encode_sess(s: &MohmalSess) -> Result<String, String> {
    let b = serde_json::to_vec(s).map_err(|e| e.to_string())?;
    Ok(format!(
        "{}{}",
        TOK_PREFIX,
        base64::Engine::encode(&base64::engine::general_purpose::STANDARD, b)
    ))
}

/// 从 token 字符串解码会话信息
fn decode_sess(tok: &str) -> Result<MohmalSess, String> {
    if !tok.starts_with(TOK_PREFIX) {
        return Err("mohmal: token 格式无效".into());
    }
    let raw = base64::Engine::decode(
        &base64::engine::general_purpose::STANDARD,
        tok.trim_start_matches(TOK_PREFIX),
    )
    .map_err(|_| "mohmal: token 解码失败".to_string())?;
    let s: MohmalSess =
        serde_json::from_slice(&raw).map_err(|_| "mohmal: token 反序列化失败".to_string())?;
    if s.c.is_empty() {
        return Err("mohmal: token 中 cookie 为空".into());
    }
    Ok(s)
}

/// 去除 HTML 标签
fn strip_tags(s: &str) -> String {
    TAG_RE.replace_all(s, " ").to_string()
}

/// 简单 HTML 实体反转义
fn light_unescape(s: &str) -> String {
    let t = s.replace("&amp;", "\u{fffd}");
    t.replace("&quot;", "\"")
        .replace("&#34;", "\"")
        .replace("&#39;", "'")
        .replace("&lt;", "<")
        .replace("&gt;", ">")
        .replace("&nbsp;", " ")
        .replace('\u{fffd}', "&")
}

/// 构建浏览器请求头
fn page_headers(referer: &str) -> Vec<(&'static str, String)> {
    vec![
        ("User-Agent", get_current_ua().to_string()),
        (
            "Accept",
            "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8"
                .to_string(),
        ),
        (
            "Accept-Language",
            "en-US,en;q=0.9".to_string(),
        ),
        ("Cache-Control", "no-cache".to_string()),
        ("DNT", "1".to_string()),
        ("Pragma", "no-cache".to_string()),
        ("Referer", referer.to_string()),
        ("Upgrade-Insecure-Requests", "1".to_string()),
    ]
}

/// 创建临时邮箱
/// 1. GET /en/create/random，跟随重定向到 /en/inbox，获取 connect.sid
/// 2. 从 /en/inbox 页面 HTML 中提取 data-email 属性获取邮箱地址
pub fn generate_email() -> Result<EmailInfo, String> {
    let create_url = format!("{}/en/create/random", ORIGIN);
    let inbox_url = format!("{}/en/inbox", ORIGIN);

    block_on(async {
        let client = http_client_no_cookie_jar();

        // 构建不跟随重定向的客户端，手动跟踪 cookie
        let no_redirect_client = wreq::Client::builder()
            .redirect(wreq::redirect::Policy::none())
            .build()
            .map_err(|e| format!("mohmal: 创建客户端失败: {}", e))?;

        // 第一步：GET /en/create/random（不跟随重定向，获取 connect.sid cookie）
        let mut req = no_redirect_client.get(&create_url);
        for (k, v) in page_headers(ORIGIN) {
            req = req.header(k, v);
        }
        let resp = req
            .send()
            .await
            .map_err(|e| format!("mohmal create: {}", e))?;
        let mut cookie_hdr = merge_set_cookies("", resp.headers());

        // 检查是否存在 connect.sid cookie
        if !parse_cookie_header(&cookie_hdr).contains_key("connect.sid") {
            // 可能服务端直接返回了 200 而非重定向，尝试从响应体提取
            let status = resp.status();
            if status.is_redirection() {
                // 手动跟随重定向到 /en/inbox
                let location = resp
                    .headers()
                    .get("location")
                    .and_then(|v| v.to_str().ok())
                    .unwrap_or("/en/inbox");
                let redirect_url = if location.starts_with("http") {
                    location.to_string()
                } else {
                    format!("{}{}", ORIGIN, location)
                };
                let mut req2 = no_redirect_client.get(&redirect_url);
                for (k, v) in page_headers(&create_url) {
                    req2 = req2.header(k, v);
                }
                req2 = req2.header("Cookie", &cookie_hdr);
                let resp2 = req2
                    .send()
                    .await
                    .map_err(|e| format!("mohmal redirect: {}", e))?;
                cookie_hdr = merge_set_cookies(&cookie_hdr, resp2.headers());
            }
        }

        if !parse_cookie_header(&cookie_hdr).contains_key("connect.sid") {
            return Err("mohmal: 未获取到 connect.sid cookie".into());
        }

        // 第二步：GET /en/inbox 获取邮箱地址
        let mut req3 = client.get(&inbox_url);
        for (k, v) in page_headers(&create_url) {
            req3 = req3.header(k, v);
        }
        req3 = req3.header("Cookie", &cookie_hdr);
        let resp3 = req3
            .send()
            .await
            .map_err(|e| format!("mohmal inbox: {}", e))?;
        if !resp3.status().is_success() {
            return Err(format!("mohmal inbox: {}", resp3.status()));
        }
        cookie_hdr = merge_set_cookies(&cookie_hdr, resp3.headers());
        let html = resp3.text().await.map_err(|e| e.to_string())?;

        // 从 HTML 中提取 data-email 属性
        let email = DATA_EMAIL_RE
            .captures(&html)
            .and_then(|c| c.get(1))
            .map(|m| light_unescape(m.as_str().trim()))
            .ok_or_else(|| "mohmal: 未找到 data-email 属性".to_string())?;

        if email.is_empty() || !email.contains('@') {
            return Err("mohmal: 提取的邮箱地址无效".into());
        }

        let tok = encode_sess(&MohmalSess { c: cookie_hdr })?;

        Ok(EmailInfo {
            channel: Channel::Mohmal,
            email,
            token: Some(tok),
            expires_at: None,
            created_at: None,
        })
    })
}

/// 获取邮件列表
/// 1. GET /en/inbox 获取收件箱 HTML
/// 2. 解析 #inbox-table 中的邮件链接和基本信息
/// 3. 对每封邮件 GET /en/message/{id} 获取详细内容
pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let sess = decode_sess(token)?;
    let inbox_url = format!("{}/en/inbox", ORIGIN);
    let cookie_hdr = &sess.c;

    block_on(async {
        let client = http_client_no_cookie_jar();

        // 获取收件箱页面
        let mut req = client.get(&inbox_url);
        for (k, v) in page_headers(ORIGIN) {
            req = req.header(k, v);
        }
        req = req.header("Cookie", cookie_hdr);
        let resp = req
            .send()
            .await
            .map_err(|e| format!("mohmal inbox: {}", e))?;
        if !resp.status().is_success() {
            return Err(format!("mohmal inbox: {}", resp.status()));
        }
        let html = resp.text().await.map_err(|e| e.to_string())?;

        // 解析邮件列表：提取每行的消息 ID 和基本信息
        let mail_entries = parse_inbox_table(&html);
        let mut out = Vec::with_capacity(mail_entries.len());

        for entry in &mail_entries {
            // 获取每封邮件的详情页
            let detail_url = format!("{}/en/message/{}", ORIGIN, entry.id);
            let mut reqd = client.get(&detail_url);
            for (k, v) in page_headers(&inbox_url) {
                reqd = reqd.header(k, v);
            }
            reqd = reqd.header("Cookie", cookie_hdr);
            let respd = match reqd.send().await {
                Ok(r) => r,
                Err(_) => continue,
            };
            if !respd.status().is_success() {
                continue;
            }
            let page = match respd.text().await {
                Ok(t) => t,
                Err(_) => continue,
            };

            let raw = parse_detail_page(&page, entry, email);
            out.push(normalize_email(&raw, email));
        }

        Ok(out)
    })
}

/// 收件箱中解析出的邮件条目
struct InboxEntry {
    /// 邮件 ID
    id: String,
    /// 发件人
    from: String,
    /// 主题
    subject: String,
    /// 时间
    date: String,
}

/// 从收件箱 HTML 解析邮件列表条目
fn parse_inbox_table(html: &str) -> Vec<InboxEntry> {
    let mut entries = Vec::new();
    let mut seen = std::collections::HashSet::new();

    for tr_cap in TR_RE.captures_iter(html) {
        let tr_content = tr_cap.get(1).unwrap().as_str();

        // 提取消息 ID
        let msg_id = match MSG_LINK_RE.captures(tr_content) {
            Some(cap) => cap.get(1).unwrap().as_str().to_string(),
            None => continue,
        };

        if !seen.insert(msg_id.clone()) {
            continue;
        }

        // 提取所有 td 单元格
        let tds: Vec<String> = TD_RE
            .captures_iter(tr_content)
            .map(|c| {
                let raw = c.get(1).unwrap().as_str();
                light_unescape(strip_tags(raw).trim())
            })
            .collect();

        // 表格行结构：发件人, 主题, 时间
        let from = tds.first().cloned().unwrap_or_default();
        let subject = tds.get(1).cloned().unwrap_or_default();
        let date = tds.get(2).cloned().unwrap_or_default();

        entries.push(InboxEntry {
            id: msg_id,
            from,
            subject,
            date,
        });
    }

    entries
}

/// 从邮件详情页 HTML 解析邮件内容
fn parse_detail_page(page: &str, entry: &InboxEntry, recipient: &str) -> Value {
    // 尝试从详情页提取更精确的字段，回退到列表中的基本信息
    let from = MSG_FROM_RE
        .captures(page)
        .and_then(|c| c.get(1))
        .map(|m| light_unescape(strip_tags(m.as_str()).trim()))
        .filter(|s| !s.is_empty())
        .unwrap_or_else(|| entry.from.clone());

    let subject = MSG_SUBJECT_RE
        .captures(page)
        .and_then(|c| c.get(1))
        .map(|m| light_unescape(strip_tags(m.as_str()).trim()))
        .filter(|s| !s.is_empty())
        .unwrap_or_else(|| entry.subject.clone());

    let date = MSG_DATE_RE
        .captures(page)
        .and_then(|c| c.get(1))
        .map(|m| light_unescape(strip_tags(m.as_str()).trim()))
        .filter(|s| !s.is_empty())
        .unwrap_or_else(|| entry.date.clone());

    // 提取邮件正文 HTML：使用栈式解析避免嵌套 div 截断
    let html_body = extract_mail_body(page);
    let html_body = if html_body.is_empty() {
        // 回退：检查是否有 iframe src
        if let Some(iframe_cap) = IFRAME_SRC_RE.captures(page) {
            let src = iframe_cap.get(1).unwrap().as_str();
            format!("<p>Content available at: {}</p>", src)
        } else {
            String::new()
        }
    } else {
        html_body
    };

    json!({
        "id": entry.id,
        "to": recipient,
        "from": from,
        "subject": subject,
        "date": date,
        "html": html_body,
    })
}
