/*!
 * eyepaste.com — RSS XML API，无认证
 * 创建邮箱: 直接生成随机用户名 + "@eyepaste.com"
 * 获取邮件: GET https://www.eyepaste.com/inbox/{email}.rss → RSS 2.0 XML
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use rand::Rng;
use regex_lite::Regex;

/// 随机生成 10 位字母数字字符串作为用户名
fn random_username() -> String {
    const CHARS: &[u8] = b"abcdefghijklmnopqrstuvwxyz0123456789";
    let mut rng = rand::thread_rng();
    (0..10)
        .map(|_| CHARS[rng.gen_range(0..CHARS.len())] as char)
        .collect()
}

/// 创建邮箱，无需调用 API，直接生成随机地址
pub fn generate_email() -> Result<EmailInfo, String> {
    let username = random_username();
    let email = format!("{}@eyepaste.com", username);
    Ok(EmailInfo {
        channel: Channel::Eyepaste,
        email,
        token: None,
        expires_at: None,
        created_at: None,
    })
}

/// 提取 XML 标签内的文本内容
fn extract_tag<'a>(xml: &'a str, tag: &str) -> Option<&'a str> {
    let open = format!("<{}", tag);
    let close = format!("</{}>", tag);
    let start = xml.find(&open)?;
    // 跳过开始标签（可能有属性）
    let after_open = &xml[start + open.len()..];
    let content_start = after_open.find('>')? + 1;
    let content = &after_open[content_start..];
    let end = content.find(&close)?;
    Some(&content[..end])
}

/// 解码 XML 实体
fn decode_xml_entities(s: &str) -> String {
    s.replace("&amp;", "&")
        .replace("&lt;", "<")
        .replace("&gt;", ">")
        .replace("&quot;", "\"")
        .replace("&#39;", "'")
        .replace("&apos;", "'")
}

/// 解码 CDATA 内容
fn unwrap_cdata(s: &str) -> &str {
    let t = s.trim();
    if t.starts_with("<![CDATA[") && t.ends_with("]]>") {
        return &t[9..t.len() - 3];
    }
    t
}

/// 从 RSS description 中解析邮件信息
/// description 格式: <p> From: {from} To: {to} Subject: {subject} Date: {date}</p> 后续是邮件正文
fn parse_description(desc_raw: &str) -> (String, String, String, String) {
    let desc = unwrap_cdata(desc_raw);
    let decoded = decode_xml_entities(desc);

    let from_re = Regex::new(r"(?i)From:\s*([^\n]*?)(?:\s+To:|\s*$)").unwrap();
    let subject_re = Regex::new(r"(?i)Subject:\s*([^\n]*?)(?:\s+Date:|\s*$)").unwrap();
    let date_re = Regex::new(r"(?i)Date:\s*([^\n<]*)").unwrap();

    let from = from_re
        .captures(&decoded)
        .and_then(|c| c.get(1))
        .map(|m| m.as_str().trim().to_string())
        .unwrap_or_default();
    let subject = subject_re
        .captures(&decoded)
        .and_then(|c| c.get(1))
        .map(|m| m.as_str().trim().to_string())
        .unwrap_or_default();
    let date = date_re
        .captures(&decoded)
        .and_then(|c| c.get(1))
        .map(|m| m.as_str().trim().to_string())
        .unwrap_or_default();

    // 邮件正文: 第一个 </p> 之后的所有内容
    let body = if let Some(idx) = decoded.find("</p>") {
        decoded[idx + 4..].trim().to_string()
    } else {
        String::new()
    };

    (from, subject, date, body)
}

/// 获取邮件列表
pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let em = email.trim();
    if em.is_empty() {
        return Err("eyepaste: 邮箱地址为空".into());
    }
    let url = format!(
        "https://www.eyepaste.com/inbox/{}.rss",
        urlencoding::encode(em)
    );

    block_on(async {
        let resp = http_client()
            .get(&url)
            .header(
                "Accept",
                "application/rss+xml, application/xml, text/xml, */*",
            )
            .header("User-Agent", get_current_ua())
            .send()
            .await
            .map_err(|e| format!("eyepaste: {}", e))?;
        if resp.status().as_u16() == 404 {
            return Ok(vec![]);
        }
        if !resp.status().is_success() {
            return Err(format!("eyepaste: 获取邮件失败 {}", resp.status()));
        }
        let xml = resp.text().await.map_err(|e| format!("eyepaste: {}", e))?;

        // 解析 RSS XML，提取所有 <item> 元素
        let mut out = Vec::new();
        let mut search_pos = 0;
        let mut idx = 0;
        while let Some(item_start) = xml[search_pos..].find("<item>") {
            let abs_start = search_pos + item_start;
            let Some(item_end) = xml[abs_start..].find("</item>") else {
                break;
            };
            let item = &xml[abs_start..abs_start + item_end + 7];

            let title = extract_tag(item, "title")
                .map(|s| decode_xml_entities(unwrap_cdata(s)))
                .unwrap_or_default();
            let description = extract_tag(item, "description").unwrap_or("");
            let pub_date = extract_tag(item, "pubDate")
                .map(|s| decode_xml_entities(s))
                .unwrap_or_default();

            let (from, subject, date, body) = parse_description(description);
            // 优先使用 description 中解析的值，fallback 到 title/pubDate
            let final_subject = if subject.is_empty() { title } else { subject };
            let final_date = if date.is_empty() { pub_date } else { date };

            let flat = serde_json::json!({
                "id": format!("eyepaste-{}", idx),
                "from": from,
                "to": em,
                "subject": final_subject,
                "html": body,
                "date": final_date,
                "isRead": false,
            });
            out.push(normalize_email(&flat, em));
            idx += 1;

            search_pos = abs_start + item_end + 7;
        }
        Ok(out)
    })
}
