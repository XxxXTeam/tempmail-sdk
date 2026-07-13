/*!
 * anonymmail.net — POST JSON API，需要 cookie session
 * 创建邮箱: HEAD 获取 cookie → POST /api/create
 * 获取邮件: POST /api/get
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use rand::Rng;
use serde_json::Value;

const BASE: &str = "https://anonymmail.net";

/// 随机生成 10 位字母数字字符串作为用户名
fn random_username() -> String {
    const CHARS: &[u8] = b"abcdefghijklmnopqrstuvwxyz0123456789";
    let mut rng = rand::thread_rng();
    (0..10)
        .map(|_| CHARS[rng.gen_range(0..CHARS.len())] as char)
        .collect()
}

/// 获取可用域名列表
fn fetch_domains() -> Result<Vec<String>, String> {
    block_on(async {
        let resp = http_client()
            .post(format!("{}/api/getDomains", BASE))
            .header("Accept", "*/*")
            .header("Content-Type", "application/x-www-form-urlencoded")
            .header("Referer", format!("{}/", BASE))
            .header("User-Agent", get_current_ua())
            .send()
            .await
            .map_err(|e| format!("anonymmail: {}", e))?;
        if !resp.status().is_success() {
            return Err(format!("anonymmail: 获取域名失败 {}", resp.status()));
        }
        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("anonymmail: {}", e))?;
        let arr = data.as_array().ok_or("anonymmail: 域名响应非数组")?;
        let mut domains = Vec::new();
        for item in arr {
            if let Some(d) = item.get("domain").and_then(|v| v.as_str()) {
                let t = d.trim();
                if !t.is_empty() {
                    domains.push(t.to_string());
                }
            }
        }
        if domains.is_empty() {
            return Err("anonymmail: 无可用域名".into());
        }
        Ok(domains)
    })
}

/// 创建邮箱
pub fn generate_email() -> Result<EmailInfo, String> {
    let domains = fetch_domains()?;
    let mut rng = rand::thread_rng();
    let domain = &domains[rng.gen_range(0..domains.len())];
    let username = random_username();
    let email = format!("{}@{}", username, domain);

    block_on(async {
        // 先 HEAD 获取 session cookie
        let head_resp = http_client()
            .head(format!("{}/", BASE))
            .header("User-Agent", get_current_ua())
            .send()
            .await
            .map_err(|e| format!("anonymmail: HEAD 失败 {}", e))?;
        // 忽略 HEAD 响应状态，只需要 cookie 被客户端保存
        let _ = head_resp;

        // POST /api/create 创建邮箱
        let body = format!("email={}", urlencoding::encode(&email));
        let resp = http_client()
            .post(format!("{}/api/create", BASE))
            .header("Accept", "*/*")
            .header("Content-Type", "application/x-www-form-urlencoded")
            .header("Referer", format!("{}/", BASE))
            .header("User-Agent", get_current_ua())
            .body(body)
            .send()
            .await
            .map_err(|e| format!("anonymmail: 创建邮箱失败 {}", e))?;
        if !resp.status().is_success() {
            return Err(format!("anonymmail: 创建邮箱 HTTP {}", resp.status()));
        }
        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("anonymmail: 解析响应失败 {}", e))?;
        if data.get("success").and_then(|v| v.as_bool()) != Some(true) {
            return Err(format!(
                "anonymmail: 创建失败 {}",
                serde_json::to_string(&data).unwrap_or_default()
            ));
        }
        // 使用返回的 email 地址（如果存在）
        let addr = data
            .get("email")
            .and_then(|v| v.as_str())
            .unwrap_or(&email)
            .trim()
            .to_string();

        Ok(EmailInfo {
            channel: Channel::Anonymmail,
            email: addr,
            token: None,
            expires_at: None,
            created_at: None,
        })
    })
}

/// 获取邮件列表
pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let em = email.trim();
    if em.is_empty() {
        return Err("anonymmail: 邮箱地址为空".into());
    }

    block_on(async {
        let body = format!("email={}", urlencoding::encode(em));
        let resp = http_client()
            .post(format!("{}/api/get", BASE))
            .header("Accept", "*/*")
            .header("Content-Type", "application/x-www-form-urlencoded")
            .header("Referer", format!("{}/", BASE))
            .header("User-Agent", get_current_ua())
            .body(body)
            .send()
            .await
            .map_err(|e| format!("anonymmail: {}", e))?;
        if !resp.status().is_success() {
            return Err(format!("anonymmail: 获取邮件失败 {}", resp.status()));
        }
        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("anonymmail: {}", e))?;
        // 响应格式: {"email@domain":{"created_at":"...","emails":[...]}}
        let obj = data.as_object().ok_or("anonymmail: 响应非对象")?;
        let mut out = Vec::new();
        for (_key, val) in obj {
            if let Some(emails) = val.get("emails").and_then(|v| v.as_array()) {
                for raw in emails {
                    // 映射字段: token→id, body→html
                    let flat = serde_json::json!({
                        "id": raw.get("token").and_then(|v| v.as_str()).unwrap_or(""),
                        "from": raw.get("from").and_then(|v| v.as_str()).unwrap_or(""),
                        "to": em,
                        "subject": raw.get("subject").and_then(|v| v.as_str()).unwrap_or(""),
                        "html": raw.get("body").and_then(|v| v.as_str()).unwrap_or(""),
                        "date": raw.get("date").and_then(|v| v.as_str()).unwrap_or(""),
                        "isRead": false,
                    });
                    out.push(normalize_email(&flat, em));
                }
            }
        }
        Ok(out)
    })
}
