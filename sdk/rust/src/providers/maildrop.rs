use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use rand::Rng;
use serde_json::Value;

const BASE: &str = "https://maildrop.cx";
const EXCLUDED_SUFFIX: &str = "transformer.edu.kg";

fn md_headers(b: wreq::RequestBuilder) -> wreq::RequestBuilder {
    b.header("Accept", "application/json, text/plain, */*")
        .header(
            "Accept-Language",
            "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
        )
        .header("Cache-Control", "no-cache")
        .header("DNT", "1")
        .header("Pragma", "no-cache")
        .header("Referer", "https://maildrop.cx/zh-cn/app")
        .header(
            "sec-ch-ua",
            r#""Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146""#,
        )
        .header("sec-ch-ua-mobile", "?0")
        .header("sec-ch-ua-platform", r#""Windows""#)
        .header("sec-fetch-dest", "empty")
        .header("sec-fetch-mode", "cors")
        .header("sec-fetch-site", "same-origin")
        .header("User-Agent", get_current_ua())
        .header("x-requested-with", "XMLHttpRequest")
}

fn random_local(len: usize) -> String {
    let chars: Vec<char> = "abcdefghijklmnopqrstuvwxyz0123456789".chars().collect();
    let mut rng = rand::thread_rng();
    (0..len)
        .map(|_| chars[rng.gen_range(0..chars.len())])
        .collect()
}

fn fetch_suffixes() -> Result<Vec<String>, String> {
    block_on(async {
        let resp = md_headers(http_client().get(format!("{}/api/suffixes.php", BASE)))
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("maildrop suffixes {}", resp.status()));
        }
        let v: Value = resp.json().await.map_err(|e| e.to_string())?;
        let arr = v
            .as_array()
            .ok_or_else(|| "maildrop: invalid suffixes".to_string())?;
        let ex = EXCLUDED_SUFFIX.to_lowercase();
        let mut out = Vec::new();
        for x in arr {
            if let Some(s) = x.as_str() {
                let t = s.trim();
                if !t.is_empty() && t.to_lowercase() != ex {
                    out.push(t.to_string());
                }
            }
        }
        if out.is_empty() {
            return Err("maildrop: no domains".into());
        }
        Ok(out)
    })
}

fn pick_suffix(suffixes: &[String], preferred: Option<&str>) -> Result<String, String> {
    if let Some(p) = preferred {
        let pl = p.trim().to_lowercase();
        if !pl.is_empty() {
            if let Some(hit) = suffixes.iter().find(|d| d.to_lowercase() == pl) {
                return Ok(hit.clone());
            }
            return Err(format!("maildrop: domain not available: {}", pl));
        }
    }
    let mut rng = rand::thread_rng();
    Ok(suffixes[rng.gen_range(0..suffixes.len())].clone())
}

pub fn generate_email(domain: Option<&str>) -> Result<EmailInfo, String> {
    let suffixes = fetch_suffixes()?;
    let dom = pick_suffix(&suffixes, domain)?;
    let local = random_local(10);
    let email = format!("{}@{}", local, dom);
    Ok(EmailInfo {
        channel: Channel::Maildrop,
        email: email.clone(),
        token: Some(email),
        expires_at: None,
        created_at: None,
    })
}

/// 通过详情接口获取单封邮件完整内容
/// GET /api/email_content.php?id={id}
/// 详情响应字段（从前端代码确认）:
/// - content: 完整 HTML 正文
/// - subject / from_addr / date: 邮件元数据
/// - attachment: JSON 字符串数组 [{filename, path, size}]（可能为空）
async fn fetch_detail(id: &str) -> Option<Value> {
    let trimmed = id.trim();
    if trimmed.is_empty() {
        return None;
    }
    let url = format!(
        "{}/api/email_content.php?id={}",
        BASE,
        urlencoding::encode(trimmed)
    );
    let resp = md_headers(http_client().get(url)).send().await.ok()?;
    if !resp.status().is_success() {
        return None;
    }
    resp.json::<Value>().await.ok()
}

/// 解析详情接口的 attachment 字段（JSON 字符串）为附件列表
fn parse_attachments(raw: &Value) -> Value {
    if let Some(s) = raw.as_str() {
        if !s.trim().is_empty() {
            if let Ok(v) = serde_json::from_str::<Value>(s) {
                if v.is_array() {
                    return v;
                }
            }
        }
    }
    Value::Array(vec![])
}

pub fn get_emails(_token: &str, email: &str) -> Result<Vec<Email>, String> {
    let addr = email.trim();
    let addr = if addr.is_empty() { _token.trim() } else { addr };
    if addr.is_empty() {
        return Err("maildrop: empty address".into());
    }
    let q = urlencoding::encode(addr);
    let url = format!("{}/api/emails.php?addr={}&page=1&limit=20", BASE, q);
    block_on(async {
        let resp = md_headers(http_client().get(url))
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("maildrop emails {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let rows = data
            .get("emails")
            .and_then(|x| x.as_array())
            .cloned()
            .unwrap_or_default();

        let mut out = Vec::with_capacity(rows.len());
        for raw in &rows {
            /* 提取 ID（支持 string/number） */
            let id = raw
                .get("id")
                .and_then(|v| {
                    v.as_str()
                        .map(|s| s.to_string())
                        .or_else(|| v.as_i64().map(|n| n.to_string()))
                        .or_else(|| v.as_u64().map(|n| n.to_string()))
                })
                .unwrap_or_default();

            /* 列表 description 作为文本回退 */
            let desc = raw
                .get("description")
                .and_then(|v| v.as_str())
                .unwrap_or("")
                .trim()
                .to_string();

            /* 拉取详情覆盖 html/from/subject/date/attachments */
            let mut from = raw
                .get("from_addr")
                .and_then(|v| v.as_str())
                .unwrap_or("")
                .trim()
                .to_string();
            let mut subject = raw
                .get("subject")
                .and_then(|v| v.as_str())
                .unwrap_or("")
                .trim()
                .to_string();
            let mut date = raw
                .get("date")
                .and_then(|v| v.as_str())
                .unwrap_or("")
                .to_string();
            let mut html = String::new();
            let mut attachments = Value::Array(vec![]);

            if !id.is_empty() {
                if let Some(detail) = fetch_detail(&id).await {
                    if let Some(c) = detail.get("content").and_then(|v| v.as_str()) {
                        if !c.trim().is_empty() {
                            html = c.to_string();
                        }
                    }
                    if let Some(f) = detail.get("from_addr").and_then(|v| v.as_str()) {
                        if !f.trim().is_empty() {
                            from = f.trim().to_string();
                        }
                    }
                    if let Some(s) = detail.get("subject").and_then(|v| v.as_str()) {
                        if !s.trim().is_empty() {
                            subject = s.trim().to_string();
                        }
                    }
                    if let Some(d) = detail.get("date").and_then(|v| v.as_str()) {
                        if !d.trim().is_empty() {
                            date = d.to_string();
                        }
                    }
                    if let Some(att_raw) = detail.get("attachment") {
                        attachments = parse_attachments(att_raw);
                    }
                }
            }

            let is_read = raw
                .get("isRead")
                .and_then(|v| v.as_bool().or_else(|| v.as_i64().map(|n| n != 0)))
                .unwrap_or(false);

            let flat = serde_json::json!({
                "id": id,
                "from": from,
                "to": addr,
                "subject": subject,
                "text": desc,
                "html": html,
                "date": date,
                "isRead": is_read,
                "attachments": attachments,
            });
            out.push(normalize_email(&flat, addr));
        }
        Ok(out)
    })
}
