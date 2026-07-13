/*!
 * Mail Sunls -- https://mail.sunls.de
 * 无需认证、无需 Token 的简单 REST API 渠道
 * 域名列表通过 API 动态获取，邮箱地址本地生成
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use rand::Rng;
use serde_json::Value;

const BASE: &str = "https://mail.sunls.de";

/// 生成指定长度的随机小写字母数字字符串
fn random_local(len: usize) -> String {
    const CHARS: &[u8] = b"abcdefghijklmnopqrstuvwxyz0123456789";
    let mut rng = rand::thread_rng();
    (0..len)
        .map(|_| CHARS[rng.gen_range(0..CHARS.len())] as char)
        .collect()
}

/// 从 API 获取可用域名列表
fn fetch_domains() -> Result<Vec<String>, String> {
    block_on(async {
        let resp = http_client()
            .get(format!("{}/api/domain", BASE))
            .header("Accept", "application/json")
            .header("User-Agent", get_current_ua())
            .send()
            .await
            .map_err(|e| format!("mail-sunls: 获取域名列表失败: {}", e))?;
        if !resp.status().is_success() {
            return Err(format!("mail-sunls: 获取域名列表 HTTP {}", resp.status()));
        }
        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("mail-sunls: 解析域名列表失败: {}", e))?;
        let domains = data
            .as_array()
            .ok_or_else(|| "mail-sunls: 域名列表格式错误".to_string())?
            .iter()
            .filter_map(|v| v.as_str())
            .map(|s| s.to_string())
            .filter(|s| !s.is_empty())
            .collect::<Vec<_>>();
        if domains.is_empty() {
            return Err("mail-sunls: 域名列表为空".into());
        }
        Ok(domains)
    })
}

/// 创建临时邮箱
/// 通过 API 获取域名列表后，本地生成随机邮箱地址
pub fn generate_email() -> Result<EmailInfo, String> {
    let domains = fetch_domains()?;
    let mut rng = rand::thread_rng();
    let domain = &domains[rng.gen_range(0..domains.len())];
    let local = random_local(10);
    let email = format!("{}@{}", local, domain);

    Ok(EmailInfo {
        channel: Channel::MailSunls,
        email,
        token: None,
        expires_at: None,
        created_at: None,
    })
}

/// 获取邮件列表
pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let em = email.trim();
    if em.is_empty() {
        return Err("mail-sunls: 邮箱地址为空".into());
    }

    block_on(async {
        let url = format!("{}/api/fetch?to={}", BASE, urlencoding::encode(em));
        let resp = http_client()
            .get(&url)
            .header("Accept", "application/json")
            .header("User-Agent", get_current_ua())
            .send()
            .await
            .map_err(|e| format!("mail-sunls: 获取邮件列表失败: {}", e))?;
        if !resp.status().is_success() {
            return Err(format!("mail-sunls: 获取邮件列表 HTTP {}", resp.status()));
        }
        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("mail-sunls: 解析邮件列表失败: {}", e))?;
        let rows = data.as_array().cloned().unwrap_or_default();

        let mut out = Vec::with_capacity(rows.len());
        for raw in &rows {
            let id = raw
                .get("id")
                .or_else(|| raw.get("_id"))
                .and_then(|v| {
                    v.as_str()
                        .map(|s| s.to_string())
                        .or_else(|| v.as_i64().map(|n| n.to_string()))
                })
                .unwrap_or_default();

            // 尝试获取单封邮件详情
            let detail = fetch_detail(&id);
            let merged = if let Some(d) = &detail { d } else { raw };

            let flat = serde_json::json!({
                "id": id,
                "from": merged.get("from")
                    .or_else(|| merged.get("from_address"))
                    .or_else(|| merged.get("sender"))
                    .and_then(|v| v.as_str())
                    .unwrap_or(""),
                "to": em,
                "subject": merged.get("subject")
                    .and_then(|v| v.as_str())
                    .unwrap_or(""),
                "date": merged.get("date")
                    .or_else(|| merged.get("created_at"))
                    .and_then(|v| v.as_str())
                    .unwrap_or(""),
                "html": merged.get("html")
                    .or_else(|| merged.get("body_html"))
                    .and_then(|v| v.as_str())
                    .unwrap_or(""),
                "text": merged.get("text")
                    .or_else(|| merged.get("body_text"))
                    .or_else(|| merged.get("body"))
                    .and_then(|v| v.as_str())
                    .unwrap_or(""),
                "isRead": false,
                "attachments": merged.get("attachments").cloned().unwrap_or(Value::Array(vec![])),
            });
            out.push(normalize_email(&flat, em));
        }
        Ok(out)
    })
}

/// 获取单封邮件详情
fn fetch_detail(id: &str) -> Option<Value> {
    if id.is_empty() {
        return None;
    }
    let url = format!("{}/api/fetch/{}", BASE, urlencoding::encode(id));
    block_on(async {
        let resp = http_client()
            .get(&url)
            .header("Accept", "application/json")
            .header("User-Agent", get_current_ua())
            .send()
            .await
            .ok()?;
        if !resp.status().is_success() {
            return None;
        }
        resp.json::<Value>().await.ok()
    })
}
