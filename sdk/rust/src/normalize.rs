/*!
 * 邮件数据标准化模块
 * 将各提供商返回的原始邮件数据标准化为统一的 Email 格式
 */

use chrono::{NaiveDateTime, TimeZone, Utc};
use serde_json::Value;
use crate::types::{Email, EmailAttachment};

/// 将原始 JSON 数据标准化为统一的 Email 格式
pub fn normalize_email(raw: &Value, recipient_email: &str) -> Email {
    let mut text = get_str(raw, &["text", "text_body", "preview_text", "mail_body_text", "body", "content", "body_text", "text_content", "description"]);
    let mut html = get_str(raw, &["html", "html_body", "html_content", "body_html", "mail_body_html"]);

    /* 修正 text/html 错配：部分渠道将 HTML 放在 body/text 字段中 */
    if !text.is_empty() && html.is_empty() && is_html_content(&text) {
        html = text;
        text = String::new();
    }

    Email {
        id: get_str(raw, &["id", "eid", "_id", "mailboxId", "messageId", "mail_id"]),
        from_addr: get_str(raw, &["from_addr", "from_address", "fromAddress", "mail_sender", "sender", "address_from", "from_email", "from", "messageFrom"]),
        to: {
            let t = get_str(raw, &["to", "to_address", "toAddress", "name_to", "email_address", "address"]);
            if t.is_empty() { recipient_email.to_string() } else { t }
        },
        subject: get_str(raw, &["subject", "e_subject", "mail_title"]),
        text,
        html,
        date: normalize_date(raw),
        is_read: normalize_is_read(raw),
        attachments: normalize_attachments(raw),
    }
}

/// 检测内容是否为 HTML
fn is_html_content(content: &str) -> bool {
    let trimmed = content.trim().to_lowercase();
    if trimmed.starts_with("<!doctype html") || trimmed.starts_with("<html") || trimmed.starts_with("<body") {
        return true;
    }
    if trimmed.contains("<div") && trimmed.contains("</div>") {
        return true;
    }
    if trimmed.contains("<table") && trimmed.contains("</table>") {
        return true;
    }
    if trimmed.contains("<p") && trimmed.contains("</p>") {
        return true;
    }
    false
}

/// 从 JSON 中按优先级提取字符串值
fn get_str(raw: &Value, keys: &[&str]) -> String {
    for key in keys {
        if let Some(v) = raw.get(key) {
            if let Some(s) = v.as_str() {
                return s.to_string();
            }
            if !v.is_null() {
                return v.to_string();
            }
        }
    }
    String::new()
}

/// 提取并统一日期格式
fn normalize_date(raw: &Value) -> String {
    for key in &["received_at", "receivedAt", "created_at", "createdAt", "date"] {
        if let Some(v) = raw.get(key) {
            if let Some(s) = v.as_str() {
                if !s.is_empty() {
                    if let Ok(dt) = NaiveDateTime::parse_from_str(s, "%Y-%m-%d %H:%M:%S") {
                        return Utc.from_utc_datetime(&dt).to_rfc3339();
                    }
                    return s.to_string();
                }
            }
            if let Some(n) = v.as_f64() {
                if n > 0.0 {
                    if n > 1e12 {
                        return format_timestamp_ms(n as i64);
                    }
                    return format_timestamp_s(n as i64);
                }
            }
        }
    }

    for key in &["timestamp", "e_date"] {
        if let Some(v) = raw.get(key) {
            if let Some(n) = v.as_f64() {
                if n > 0.0 {
                    if *key == "timestamp" && n < 1e12 {
                        return format_timestamp_s(n as i64);
                    }
                    return format_timestamp_ms(n as i64);
                }
            }
        }
    }

    String::new()
}

fn format_timestamp_s(ts: i64) -> String {
    chrono::DateTime::from_timestamp(ts, 0)
        .map(|dt| dt.to_rfc3339())
        .unwrap_or_default()
}

fn format_timestamp_ms(ts: i64) -> String {
    chrono::DateTime::from_timestamp_millis(ts)
        .map(|dt| dt.to_rfc3339())
        .unwrap_or_default()
}

/// 提取已读状态
fn normalize_is_read(raw: &Value) -> bool {
    for key in &["seen", "read", "isRead"] {
        if let Some(v) = raw.get(key) {
            if let Some(b) = v.as_bool() {
                return b;
            }
            if let Some(n) = v.as_f64() {
                return n as i64 != 0;
            }
        }
    }
    if let Some(v) = raw.get("is_read") {
        if let Some(b) = v.as_bool() { return b; }
        if let Some(n) = v.as_f64() { return n as i64 != 0; }
        if let Some(s) = v.as_str() { return s == "1"; }
    }
    if let Some(v) = raw.get("is_seen") {
        if let Some(b) = v.as_bool() { return b; }
        if let Some(n) = v.as_f64() { return n as i64 != 0; }
        if let Some(s) = v.as_str() { return s == "1"; }
    }
    false
}

/// 提取附件列表
fn normalize_attachments(raw: &Value) -> Vec<EmailAttachment> {
    let Some(arr) = raw.get("attachments").and_then(|v| v.as_array()) else {
        return vec![];
    };

    arr.iter().filter_map(|a| {
        let obj = a.as_object()?;
        Some(EmailAttachment {
            filename: obj.get("filename").or(obj.get("name"))
                .and_then(|v| v.as_str()).unwrap_or("").to_string(),
            size: obj.get("size").or(obj.get("filesize"))
                .and_then(|v| v.as_i64()),
            content_type: obj.get("contentType").or(obj.get("content_type"))
                .or(obj.get("mimeType")).or(obj.get("mime_type"))
                .and_then(|v| v.as_str()).map(|s| s.to_string()),
            url: obj.get("url").or(obj.get("download_url")).or(obj.get("downloadUrl"))
                .and_then(|v| v.as_str()).map(|s| s.to_string()),
        })
    }).collect()
}
