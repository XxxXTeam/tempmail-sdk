/*!
 * 10minutemail.net 渠道实现
 * 网站: https://10minutemail.net
 *
 * 流程:
 *   1. GET / 获取 session cookie（PHPSESSID）+ 从 HTML 提取随机分配的邮箱地址
 *      （邮箱输入框 value="xxx@xxx.com"）
 *   2. GET /mailbox.ajax.php?_={毫秒时间戳} 获取邮件列表（返回 HTML 表格，非 JSON）
 *   3. GET /readmail.html?mid={id} 获取单封邮件整页 HTML，从中提取正文片段
 *
 * token 存储: 会话 Cookie 串。
 * 注意: Go 端依赖全局 cookie jar 跨调用维持 session；Rust 端共享客户端会串话，
 *       故改用无 Cookie 罐客户端 + 手动 Cookie，将 session 存入 token。
 */

use std::collections::BTreeMap;
use std::sync::LazyLock;
use std::time::{SystemTime, UNIX_EPOCH};

use regex::Regex;

use crate::config::{block_on, get_current_ua, http_client_no_cookie_jar};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};

const BASE_URL: &str = "https://10minutemail.net";

/// 从首页 HTML 的邮箱输入框提取地址（value="xxx@xxx.com"）
static EMAIL_RE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r#"value="([^"]+@[^"]+)""#).expect("re"));
/// 从邮件列表表格逐行匹配 <tr>...</tr>
static ROW_RE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r#"(?is)<tr[^>]*>(.*?)</tr>"#).expect("re"));
/// 从行内链接提取邮件 ID（readmail.html?mid=xxx）
static MID_RE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r#"(?i)readmail\.html\?mid=([^'"&]+)"#).expect("re"));
/// 逐个匹配行内 <td>...</td> 单元格
static CELL_RE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r#"(?is)<td[^>]*>(.*?)</td>"#).expect("re"));
/// 从单元格提取 span 的 title 属性（收件时间）
static TITLE_RE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r#"(?i)title="([^"]+)""#).expect("re"));
/// 提取 Cloudflare 邮箱保护混淆数据（data-cfemail="十六进制"）
static CF_RE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r#"(?i)data-cfemail="([0-9a-fA-F]+)""#).expect("re"));
/// 去除 HTML 标签用于提取纯文本
static TAG_RE: LazyLock<Regex> = LazyLock::new(|| Regex::new(r#"(?s)<[^>]+>"#).expect("re"));

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
/// 1. GET / 获取 session cookie（PHPSESSID）
/// 2. 从首页 HTML 输入框正则提取随机分配的邮箱地址
///
/// token 存储会话 Cookie 串（供 GetEmails 维持 session）
pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let client = http_client_no_cookie_jar();

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
            .map_err(|e| format!("10minutemail-net: 获取首页失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("10minutemail-net: 首页返回 HTTP {}", resp.status()));
        }

        let cookie_hdr = merge_set_cookies("", resp.headers());
        let html = resp
            .text()
            .await
            .map_err(|e| format!("10minutemail-net: 读取首页响应失败: {}", e))?;

        let email = EMAIL_RE
            .captures(&html)
            .and_then(|c| c.get(1))
            .map(|m| m.as_str().trim().to_string())
            .filter(|s| !s.is_empty() && s.contains('@'))
            .ok_or_else(|| "10minutemail-net: 未能从首页提取邮箱地址".to_string())?;

        Ok(EmailInfo {
            channel: Channel::TenminutemailNet,
            email,
            token: Some(cookie_hdr),
            expires_at: None,
            created_at: None,
        })
    })
}

/// 获取邮件列表
/// 1. GET /mailbox.ajax.php?_={毫秒时间戳} 获取邮件列表（HTML 表格）
/// 2. 逐行解析 <tr>，提取邮件 ID、发件人、主题、收件时间、已读状态
/// 3. 对每封邮件 GET /readmail.html?mid={id} 提取正文 HTML 片段
///
/// 表格列顺序: 寄件人 | 主题 | 收件日期；未读行的 <tr> 带 font-weight: bold 样式
pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let em = email.trim();
    if em.is_empty() {
        return Err("10minutemail-net: 邮箱地址为空".into());
    }
    let cookie_hdr = token.trim();

    block_on(async {
        let client = http_client_no_cookie_jar();

        let ms = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .map(|d| d.as_millis())
            .unwrap_or(0);
        let list_url = format!("{BASE_URL}/mailbox.ajax.php?_={ms}");

        let resp = client
            .get(&list_url)
            .header("User-Agent", get_current_ua())
            .header("Accept", "*/*")
            .header("Accept-Language", "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7")
            .header("X-Requested-With", "XMLHttpRequest")
            .header("Referer", format!("{BASE_URL}/"))
            .header("Cookie", cookie_hdr)
            .send()
            .await
            .map_err(|e| format!("10minutemail-net: 获取邮件列表失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!(
                "10minutemail-net: 获取邮件列表返回 HTTP {}",
                resp.status()
            ));
        }

        let body = resp
            .text()
            .await
            .map_err(|e| format!("10minutemail-net: 读取邮件列表响应失败: {}", e))?;

        let mut result = Vec::new();
        for cap in ROW_RE.captures_iter(&body) {
            let row_full = cap.get(0).map(|m| m.as_str()).unwrap_or("");
            let row_inner = cap.get(1).map(|m| m.as_str()).unwrap_or("");

            // 跳过表头行（含 <th>）
            if row_inner.to_lowercase().contains("<th") {
                continue;
            }

            // 提取邮件 ID
            let mail_id = match MID_RE.captures(row_inner).and_then(|c| c.get(1)) {
                Some(m) => m.as_str().trim().to_string(),
                None => continue,
            };
            if mail_id.is_empty() {
                continue;
            }

            // 提取三个单元格：发件人 | 主题 | 收件日期
            let cells: Vec<&str> = CELL_RE
                .captures_iter(row_inner)
                .filter_map(|c| c.get(1).map(|m| m.as_str()))
                .collect();
            let from_cell = cells.first().copied().unwrap_or("");
            let subject_cell = cells.get(1).copied().unwrap_or("");
            let date_cell = cells.get(2).copied().unwrap_or("");

            let from_addr = extract_text(from_cell);
            let subject = extract_text(subject_cell);

            // 收件时间优先取 span 的 title 属性（UTC 绝对时间），否则取单元格文本
            let date = match TITLE_RE.captures(date_cell).and_then(|c| c.get(1)) {
                Some(m) => m.as_str().trim().to_string(),
                None => extract_text(date_cell),
            };

            // 未读状态：行 <tr> 样式含 font-weight: bold
            let is_read = !row_full.to_lowercase().contains("font-weight: bold");

            // 获取邮件正文 HTML
            let html_body = fetch_body(&client, cookie_hdr, &mail_id).await;

            let raw = serde_json::json!({
                "id": mail_id,
                "from": from_addr,
                "to": em,
                "subject": subject,
                "html": html_body,
                "date": date,
                "is_read": is_read,
            });
            result.push(normalize_email(&raw, em));
        }

        Ok(result)
    })
}

/// 获取单封邮件的正文 HTML 片段
/// GET /readmail.html?mid={id} 返回整页 HTML，正文位于 <div class="mailinhtml">...</div>
async fn fetch_body(client: &wreq::Client, cookie_hdr: &str, mail_id: &str) -> String {
    if mail_id.is_empty() {
        return String::new();
    }

    let url = format!("{BASE_URL}/readmail.html?mid={mail_id}");
    let resp = match client
        .get(&url)
        .header("User-Agent", get_current_ua())
        .header(
            "Accept",
            "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
        )
        .header("Accept-Language", "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7")
        .header("Referer", format!("{BASE_URL}/"))
        .header("Cookie", cookie_hdr)
        .send()
        .await
    {
        Ok(r) if r.status().is_success() => r,
        _ => return String::new(),
    };

    let page = resp.text().await.unwrap_or_default();
    extract_body(&page)
}

/// 从整页 HTML 提取正文片段
/// 正文包裹于 <div class="mailinhtml"> ... </div>，其内部存在嵌套 div，
/// 故以紧随其后的 email-decode.min.js script 标签作为结束锚点，再回退裁掉尾部两个闭合 </div>。
fn extract_body(page: &str) -> String {
    const START_MARK: &str = r#"class="mailinhtml""#;
    let si = match page.find(START_MARK) {
        Some(i) => i,
        None => return String::new(),
    };
    // 跳到 mailinhtml 这个 div 的 '>' 之后
    let gt = match page[si..].find('>') {
        Some(i) => i,
        None => return String::new(),
    };
    let content_start = si + gt + 1;
    let rest = &page[content_start..];

    // 结束锚点：mailinhtml 区块后紧跟的 cloudflare email-decode 脚本
    let segment = match rest.find("email-decode.min.js") {
        Some(ei) => {
            let mut seg = &rest[..ei];
            // segment 末尾形如 "...正文</div></div><script ..."，裁掉结尾的 <script 起始
            if let Some(s_idx) = seg.rfind("<script") {
                seg = &seg[..s_idx];
            }
            let mut seg = seg.trim().to_string();
            // 去掉 mailinhtml 与其外层 tab1 的两个闭合 div
            for _ in 0..2 {
                seg = seg.trim().to_string();
                if let Some(stripped) = seg.strip_suffix("</div>") {
                    seg = stripped.to_string();
                }
            }
            seg.trim().to_string()
        }
        None => {
            // 兜底：退化为该 div 后第一个 </div>
            match rest.find("</div>") {
                Some(di) => rest[..di].trim().to_string(),
                None => rest.trim().to_string(),
            }
        }
    };
    segment
}

/// 从单元格 HTML 提取纯文本发件人/主题
/// 优先解码 Cloudflare 邮箱保护混淆（__cf_email__ + data-cfemail），否则去标签取文本。
fn extract_text(cell: &str) -> String {
    // 优先解码 Cloudflare 邮箱保护混淆
    if let Some(cf) = CF_RE.captures(cell).and_then(|c| c.get(1)) {
        let decoded = cf_decode(cf.as_str());
        if !decoded.is_empty() {
            return decoded;
        }
    }
    // 去除标签后反转义 HTML 实体
    let text = TAG_RE.replace_all(cell, "");
    text.replace("&nbsp;", " ")
        .replace("&#160;", " ")
        .replace("&amp;", "&")
        .replace("&lt;", "<")
        .replace("&gt;", ">")
        .replace("&quot;", "\"")
        .trim()
        .to_string()
}

/// 解码 Cloudflare 邮箱保护混淆字符串
/// 算法：首字节为异或密钥，其后每字节与密钥异或还原 ASCII。解码失败返回空字符串。
fn cf_decode(encoded: &str) -> String {
    let data = match hex::decode(encoded) {
        Ok(d) if d.len() >= 2 => d,
        _ => return String::new(),
    };
    let key = data[0];
    let out: Vec<u8> = data[1..].iter().map(|b| b ^ key).collect();
    let decoded = String::from_utf8_lossy(&out).to_string();
    // 解码结果应为可读邮箱地址
    if !decoded.contains('@') {
        return String::new();
    }
    decoded
}
