/*!
 * Firetempmail 渠道实现（firetempmail.com）
 *
 * 无认证 REST：
 *   建箱: 无需请求，本地生成 随机词+0-999@<域>（域池与官网一致：
 *         offrework.click / service-today.click / jobsdeforyou.sa.com）
 *   读信: GET https://mail.firetempmail.com/mail/get?address=<完整邮箱 URL 编码>，
 *         必须携带 Header Origin: https://firetempmail.com（否则 403 'Origin not allowed'）。
 * 响应 {"status":"ok","code":200,"msg":"...","stats":{},"mails":[...]}，
 * 邮件字段以 sender/subject/date + content-html/content-text/content-plain 多候选归一化。
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use rand::RngExt;
use serde_json::{json, Value};

const API_BASE_URL: &str = "https://mail.firetempmail.com";
const ORIGIN: &str = "https://firetempmail.com";

/// 官网 JS 中的完整平台域池，顺序与官网一致
const DOMAINS: [&str; 3] = [
    "offrework.click",
    "service-today.click",
    "jobsdeforyou.sa.com",
];

/// 随机小写单词（3-6 位）+ 0-999，模拟官网 faker 词 + 1e3 取整
fn random_local() -> String {
    const CHARS: &[u8] = b"abcdefghijklmnopqrstuvwxyz";
    let mut rng = rand::rng();
    let word: String = (0..(3 + rng.random_range(0..4)))
        .map(|_| CHARS[rng.random_range(0..CHARS.len())] as char)
        .collect();
    format!("{word}{}", rng.random_range(0..1000))
}

/// 多候选字段取值：返回第一个存在且非空的字符串值
fn pick_str(m: &Value, keys: &[&str]) -> String {
    for key in keys {
        if let Some(v) = m.get(key) {
            let s = match v {
                Value::String(s) => s.clone(),
                other => other.to_string(),
            };
            if !s.is_empty() {
                return s;
            }
        }
    }
    String::new()
}

/// 创建临时邮箱
/// 建箱无需请求，本地生成 随机词+0-999@域名 的形式，token 复用完整地址
pub fn generate_email() -> Result<EmailInfo, String> {
    let dom = DOMAINS[rand::rng().random_range(0..DOMAINS.len())];
    let email = format!("{}@{dom}", random_local());
    Ok(EmailInfo {
        channel: Channel::Firetempmail,
        email: email.clone(),
        token: Some(email),
        expires_at: None,
        created_at: None,
    })
}

/// 读取收件箱
/// GET mail.firetempmail.com/mail/get?address=<URL 编码完整邮箱>，必带 Origin 头
pub fn get_emails(email: &str, _token: &str) -> Result<Vec<Email>, String> {
    let em = email.trim();
    if em.is_empty() {
        return Err("firetempmail 读信: 邮箱地址为空".into());
    }

    let url = format!(
        "{API_BASE_URL}/mail/get?address={}",
        urlencoding::encode(em)
    );

    block_on(async {
        let resp = http_client()
            .get(&url)
            .header("User-Agent", get_current_ua())
            .header("Accept", "application/json")
            .header("Origin", ORIGIN)
            .header("Referer", format!("{ORIGIN}/"))
            .send()
            .await
            .map_err(|e| format!("firetempmail: 获取收件箱失败: {e}"))?;

        let status = resp.status();
        let raw = resp
            .text()
            .await
            .map_err(|e| format!("firetempmail: 读取收件箱响应失败: {e}"))?;
        if !status.is_success() {
            return Err(format!("firetempmail 读信: http {status} {}", raw.trim()));
        }

        let data: Value = serde_json::from_str(&raw)
            .map_err(|e| format!("firetempmail: 解析收件箱响应失败: {e}"))?;
        if let Some(s) = data.get("status").and_then(|v| v.as_str()) {
            if !s.is_empty() && s != "ok" {
                let msg = data
                    .get("msg")
                    .and_then(|v| v.as_str())
                    .unwrap_or("unknown error");
                return Err(format!("firetempmail 读信: {msg}"));
            }
        }

        let mails = data
            .get("mails")
            .and_then(|v| v.as_array())
            .cloned()
            .unwrap_or_default();

        let mut out = Vec::with_capacity(mails.len());
        for m in &mails {
            /* 官网 JSON 无统一 to 字段，收件人固定为当前邮箱 */
            let flat = json!({
                "id": pick_str(m, &["id", "mail_id", "uid"]),
                "from": pick_str(m, &["sender", "from", "from_address"]),
                "to": em,
                "subject": pick_str(m, &["subject", "title"]),
                "text": pick_str(m, &["content-text", "content-plain", "text"]),
                "html": pick_str(m, &["content-html", "html"]),
                "date": pick_str(m, &["date", "received_at", "created_at"]),
            });
            out.push(normalize_email(&flat, em));
        }
        Ok(out)
    })
}
