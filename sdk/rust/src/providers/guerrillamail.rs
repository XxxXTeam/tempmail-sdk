/*!
 * guerrillamail.com 渠道实现
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::Value;

const BASE_URL: &str = "https://api.guerrillamail.com/ajax.php";

pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let resp = http_client()
            .get(format!("{}?f=get_email_address&lang=en", BASE_URL))
            .header("User-Agent", get_current_ua())
            .send()
            .await
            .map_err(|e| format!("guerrillamail request failed: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("guerrillamail generate failed: {}", resp.status()));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("parse failed: {}", e))?;
        let email_addr = data["email_addr"].as_str().unwrap_or("");
        let sid_token = data["sid_token"].as_str().unwrap_or("");

        if email_addr.is_empty() || sid_token.is_empty() {
            return Err("Failed to generate email: missing email_addr or sid_token".into());
        }

        Ok(EmailInfo {
            channel: Channel::GuerrillaMail,
            email: email_addr.to_string(),
            token: Some(sid_token.to_string()),
            expires_at: data["email_timestamp"]
                .as_i64()
                .map(|ts| (ts + 3600) * 1000),
            created_at: None,
        })
    })
}

pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let token = token.to_string();
    let email = email.to_string();
    block_on(async {
        let resp = http_client()
            .get(format!(
                "{}?f=check_email&seq=0&sid_token={}",
                BASE_URL,
                urlencoding::encode(&token)
            ))
            .header("User-Agent", get_current_ua())
            .send()
            .await
            .map_err(|e| format!("guerrillamail request failed: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!(
                "guerrillamail get emails failed: {}",
                resp.status()
            ));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("parse failed: {}", e))?;
        let list = data["list"].as_array().cloned().unwrap_or_default();

        let mut out = Vec::with_capacity(list.len());
        for item in &list {
            let mut body = item["mail_body"].as_str().unwrap_or("").to_string();
            // 如果 body 为空，通过 fetch_email 接口获取完整邮件内容
            if body.is_empty() {
                let mid_s = item["mail_id"]
                    .as_str()
                    .map(|s| s.to_string())
                    .or_else(|| item["mail_id"].as_i64().map(|n| n.to_string()))
                    .or_else(|| item["mail_id"].as_u64().map(|n| n.to_string()))
                    .unwrap_or_default();
                if !mid_s.is_empty() {
                    let detail_url = format!(
                        "{}?f=fetch_email&sid_token={}&email_id={}",
                        BASE_URL,
                        urlencoding::encode(&token),
                        urlencoding::encode(&mid_s)
                    );
                    if let Ok(dr) = http_client()
                        .get(&detail_url)
                        .header("User-Agent", get_current_ua())
                        .send()
                        .await
                    {
                        if dr.status().is_success() {
                            if let Ok(detail) = dr.json::<Value>().await {
                                if let Some(mb) = detail["mail_body"].as_str() {
                                    if !mb.is_empty() {
                                        body = mb.to_string();
                                    }
                                }
                            }
                        }
                    }
                }
            }

            let text = if body.is_empty() {
                item["mail_excerpt"].as_str().unwrap_or("").to_string()
            } else {
                // 去除 HTML 标签提取纯文本
                let re = regex::Regex::new(r"<[^>]+>")
                    .unwrap_or_else(|_| regex::Regex::new("").unwrap());
                let stripped = re.replace_all(&body, " ");
                stripped.split_whitespace().collect::<Vec<_>>().join(" ")
            };

            let flat = serde_json::json!({
                "id": item["mail_id"],
                "from": item["mail_from"],
                "to": &email,
                "subject": item["mail_subject"],
                "text": text,
                "html": body,
                "date": item["mail_date"].as_str().unwrap_or(""),
                "isRead": item["mail_read"].as_i64() == Some(1),
            });
            out.push(normalize_email(&flat, &email));
        }
        Ok(out)
    })
}
