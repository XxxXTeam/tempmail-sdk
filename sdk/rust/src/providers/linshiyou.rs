/*!
 * linshiyou.com 临时邮：Cookie（NEXUS_TOKEN + tmail-emails）+ HTML 分片响应
 */

use std::sync::LazyLock;

use chrono::{NaiveDateTime, TimeZone, Utc};
use regex::Regex;

use crate::types::{Channel, Email, EmailAttachment, EmailInfo};
use crate::config::{block_on, get_current_ua, http_client};

const ORIGIN: &str = "https://linshiyou.com";

static RE_LIST_ID: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r#"id="tmail-email-list-([a-f0-9]+)""#).unwrap());
static RE_DIV_CLASS: LazyLock<Regex> = LazyLock::new(|| {
    Regex::new(r#"(?i)<div class="([^"]+)"[^>]*>([\s\S]*?)</div>"#).unwrap()
});
static RE_SRCDOC: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r#"(?i)\ssrcdoc="([^"]*)""#).unwrap());
static RE_DOWNLOAD: LazyLock<Regex> = LazyLock::new(|| {
    Regex::new(r#"href="(/api/download\?id=[^"]+)""#).unwrap()
});
static RE_TAGS: LazyLock<Regex> = LazyLock::new(|| Regex::new(r#"<[^>]+>"#).unwrap());

fn decode_html_entities(s: &str) -> String {
    let mut out = s.to_string();
    out = regex::Regex::new(r#"&#x([0-9a-fA-F]+);"#)
        .unwrap()
        .replace_all(&out, |caps: &regex::Captures| {
            u32::from_str_radix(&caps[1], 16)
                .ok()
                .and_then(char::from_u32)
                .map(|c| c.to_string())
                .unwrap_or_default()
        })
        .into_owned();
    out = regex::Regex::new(r#"&#(\d+);"#)
        .unwrap()
        .replace_all(&out, |caps: &regex::Captures| {
            caps[1]
                .parse::<u32>()
                .ok()
                .and_then(char::from_u32)
                .map(|c| c.to_string())
                .unwrap_or_default()
        })
        .into_owned();
    out = out.replace("&quot;", "\"").replace("&lt;", "<").replace("&gt;", ">");
    out.replace("&amp;", "&")
}

fn strip_tags_to_text(html: &str) -> String {
    let t = RE_TAGS.replace_all(html, " ");
    decode_html_entities(&t)
        .split_whitespace()
        .collect::<Vec<_>>()
        .join(" ")
}

fn pick_div(html: &str, class_name: &str) -> String {
    for cap in RE_DIV_CLASS.captures_iter(html) {
        if cap.get(1).map(|m| m.as_str()) == Some(class_name) {
            let inner = cap.get(2).map(|m| m.as_str()).unwrap_or("");
            return strip_tags_to_text(inner).trim().to_string();
        }
    }
    String::new()
}

fn parse_date(s: &str) -> String {
    let s = s.trim();
    if s.is_empty() {
        return String::new();
    }
    let isoish = if s.contains('T') {
        s.to_string()
    } else {
        s.replacen(' ', "T", 1)
    };
    if let Ok(dt) = chrono::DateTime::parse_from_rfc3339(&isoish) {
        return dt.with_timezone(&chrono::Utc).to_rfc3339();
    }
    if let Ok(naive) = NaiveDateTime::parse_from_str(s, "%Y-%m-%d %H:%M:%S") {
        return Utc.from_utc_datetime(&naive).to_rfc3339();
    }
    if let Ok(naive) = NaiveDateTime::parse_from_str(&isoish, "%Y-%m-%dT%H:%M:%S") {
        return Utc.from_utc_datetime(&naive).to_rfc3339();
    }
    String::new()
}

fn extract_srcdoc(body_part: &str) -> String {
    RE_SRCDOC
        .captures(body_part)
        .and_then(|c| c.get(1))
        .map(|m| decode_html_entities(m.as_str()))
        .unwrap_or_default()
}

fn parse_segments(raw: &str, recipient: &str) -> Vec<Email> {
    let raw = raw.trim();
    if raw.is_empty() {
        return Vec::new();
    }
    let mut out = Vec::new();
    for seg in raw.split("<-----TMAILNEXTMAIL----->") {
        let seg = seg.trim();
        if seg.is_empty() {
            continue;
        }
        let mut parts = seg.splitn(2, "<-----TMAILCHOPPER----->");
        let list_part = parts.next().unwrap_or("");
        let body_part = parts.next().unwrap_or("");

        let Some(id_cap) = RE_LIST_ID.captures(list_part) else {
            continue;
        };
        let id = id_cap.get(1).map(|m| m.as_str()).unwrap_or("").to_string();
        if id.is_empty() {
            continue;
        }

        let from_list = pick_div(list_part, "name");
        let subject_list = pick_div(list_part, "subject");
        let preview_list = pick_div(list_part, "body");

        let from_body = pick_div(body_part, "tmail-email-sender");
        let time_body = pick_div(body_part, "tmail-email-time");
        let title_body = pick_div(body_part, "tmail-email-title");
        let html = extract_srcdoc(body_part);

        let from = if from_body.is_empty() {
            from_list
        } else {
            from_body
        };
        let subject = if title_body.is_empty() {
            subject_list
        } else {
            title_body
        };
        let text = if preview_list.is_empty() {
            strip_tags_to_text(&html)
        } else {
            preview_list
        };

        let mut attachments: Vec<EmailAttachment> = Vec::new();
        if let Some(cap) = RE_DOWNLOAD.captures(body_part) {
            if let Some(m) = cap.get(1) {
                attachments.push(EmailAttachment {
                    filename: String::new(),
                    size: None,
                    content_type: None,
                    url: Some(format!("{}{}", ORIGIN, m.as_str())),
                });
            }
        }

        out.push(Email {
            id,
            from_addr: from,
            to: recipient.to_string(),
            subject,
            text,
            html,
            date: parse_date(&time_body),
            is_read: false,
            attachments,
        });
    }
    out
}

fn extract_nexus_token(headers: &wreq::header::HeaderMap) -> Option<String> {
    for val in headers.get_all("set-cookie") {
        let Ok(s) = val.to_str() else {
            continue;
        };
        let needle = "NEXUS_TOKEN=";
        if let Some(i) = s.find(needle) {
            let rest = &s[i + needle.len()..];
            let token = rest.split(';').next().unwrap_or(rest).trim();
            if !token.is_empty() {
                return Some(token.to_string());
            }
        }
    }
    None
}

pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let resp = http_client()
            .get(format!("{ORIGIN}/api/user?user"))
            .header("User-Agent", get_current_ua())
            .header("accept-language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6")
            .header("cache-control", "no-cache")
            .header("dnt", "1")
            .header("pragma", "no-cache")
            .header("priority", "u=1, i")
            .header("referer", format!("{ORIGIN}/"))
            .header(
                "sec-ch-ua",
                r#""Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"#,
            )
            .header("sec-ch-ua-mobile", "?0")
            .header("sec-ch-ua-platform", r#""Windows""#)
            .header("sec-fetch-dest", "empty")
            .header("sec-fetch-mode", "cors")
            .header("sec-fetch-site", "same-origin")
            .send()
            .await
            .map_err(|e| format!("linshiyou request failed: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("linshiyou generate failed: {}", resp.status()));
        }

        let nexus = extract_nexus_token(resp.headers()).ok_or_else(|| {
            "linshiyou: NEXUS_TOKEN not found in Set-Cookie".to_string()
        })?;

        let email = resp.text().await.map_err(|e| e.to_string())?;
        let email = email.trim().to_string();
        if email.is_empty() || !email.contains('@') {
            return Err("linshiyou: invalid email in response body".into());
        }

        let token = format!("NEXUS_TOKEN={}; tmail-emails={}", nexus, email);

        Ok(EmailInfo {
            channel: Channel::Linshiyou,
            email,
            token: Some(token),
            expires_at: None,
            created_at: None,
        })
    })
}

pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let token = token.to_string();
    let email = email.to_string();
    block_on(async {
        let resp = http_client()
            .get(format!("{ORIGIN}/api/mail?unseen=1"))
            .header("User-Agent", get_current_ua())
            .header("Cookie", &token)
            .header("x-requested-with", "XMLHttpRequest")
            .header("accept-language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6")
            .header("cache-control", "no-cache")
            .header("dnt", "1")
            .header("pragma", "no-cache")
            .header("priority", "u=1, i")
            .header("referer", format!("{ORIGIN}/"))
            .header(
                "sec-ch-ua",
                r#""Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"#,
            )
            .header("sec-ch-ua-mobile", "?0")
            .header("sec-ch-ua-platform", r#""Windows""#)
            .header("sec-fetch-dest", "empty")
            .header("sec-fetch-mode", "cors")
            .header("sec-fetch-site", "same-origin")
            .send()
            .await
            .map_err(|e| format!("linshiyou mail failed: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("linshiyou get emails failed: {}", resp.status()));
        }

        let raw = resp.text().await.map_err(|e| e.to_string())?;
        Ok(parse_segments(&raw, &email))
    })
}
