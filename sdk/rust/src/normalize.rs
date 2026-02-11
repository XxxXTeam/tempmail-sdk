/*!
 * 邮件数据标准化模块
 * 将各提供商返回的原始邮件数据标准化为统一的 Email 格式
 */

use serde_json::Value;
use crate::types::{Email, EmailAttachment};

/// 将原始 JSON 数据标准化为统一的 Email 格式
pub fn normalize_email(raw: &Value, recipient_email: &str) -> Email {
    Email {
        id: get_str(raw, &["id", "eid", "_id", "mailboxId", "messageId", "mail_id"]),
        from_addr: get_str(raw, &["from_address", "address_from", "from", "messageFrom", "sender"]),
        to: {
            let t = get_str(raw, &["to", "to_address", "name_to", "email_address", "address"]);
            if t.is_empty() { recipient_email.to_string() } else { t }
        },
        subject: get_str(raw, &["subject", "e_subject"]),
        text: get_str(raw, &["text", "body", "content", "body_text", "text_content"]),
        html: get_str(raw, &["html", "html_content", "body_html"]),
        date: normalize_date(raw),
        is_read: normalize_is_read(raw),
        attachments: normalize_attachments(raw),
    }
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
    for key in &["received_at", "created_at", "createdAt", "date"] {
        if let Some(v) = raw.get(key) {
            if let Some(s) = v.as_str() {
                if !s.is_empty() {
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
        }
    }
    if let Some(v) = raw.get("is_read") {
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
