/*!
 * nimail 渠道实现
 * 网站: https://www.nimail.cn
 *
 * 简单 POST API 临时邮箱，无需认证：
 *   1. POST /api/applymail（body: mail={前缀}@nimail.cn）注册邮箱
 *      响应 {"user":"xxx@nimail.cn","success":"true", ...}
 *   2. POST /api/getmails（body: mail={email}&time=0）获取邮件列表
 *      响应 {"to":"...","mail":[...],"success":"true", ...}
 *
 * token 即邮箱地址本身。
 */

use rand::Rng;
use serde_json::Value;

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};

const BASE_URL: &str = "https://www.nimail.cn";

/// 生成随机本地部分（小写字母 + 数字）
fn random_local(length: usize) -> String {
    let chars: Vec<char> = "abcdefghijklmnopqrstuvwxyz0123456789".chars().collect();
    let mut rng = rand::thread_rng();
    (0..length)
        .map(|_| chars[rng.gen_range(0..chars.len())])
        .collect()
}

/// URL 编码邮箱地址中的 @ 符号
fn encode_mail(email: &str) -> String {
    email.replace('@', "%40")
}

/// 创建 nimail.cn 临时邮箱
pub fn generate_email() -> Result<EmailInfo, String> {
    let client = http_client();
    let name = random_local(10);
    let email = format!("{}@nimail.cn", name);
    let body = format!("mail={}", encode_mail(&email));

    block_on(async {
        let resp = client
            .post(format!("{}/api/applymail", BASE_URL))
            .header("User-Agent", get_current_ua())
            .header("Content-Type", "application/x-www-form-urlencoded")
            .header("Origin", BASE_URL)
            .header("Referer", format!("{}/", BASE_URL))
            .body(body)
            .send()
            .await
            .map_err(|e| format!("nimail: 创建邮箱请求失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("nimail: 创建邮箱失败 HTTP {}", resp.status()));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("nimail: 解析创建响应失败: {}", e))?;

        let success = data.get("success").and_then(|v| v.as_str()).unwrap_or("");
        let user = data
            .get("user")
            .and_then(|v| v.as_str())
            .unwrap_or("")
            .trim()
            .to_string();

        if success != "true" || user.is_empty() {
            return Err("nimail: 创建邮箱失败，无效响应".into());
        }

        Ok(EmailInfo {
            channel: Channel::Nimail,
            email: user.clone(),
            token: Some(user),
            expires_at: None,
            created_at: None,
        })
    })
}

/// 获取 nimail.cn 邮件列表
pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let client = http_client();
    let body = format!("mail={}&time=0", encode_mail(email));

    block_on(async {
        let resp = client
            .post(format!("{}/api/getmails", BASE_URL))
            .header("User-Agent", get_current_ua())
            .header("Content-Type", "application/x-www-form-urlencoded")
            .header("Origin", BASE_URL)
            .header("Referer", format!("{}/", BASE_URL))
            .body(body)
            .send()
            .await
            .map_err(|e| format!("nimail: 获取邮件请求失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("nimail: 获取邮件失败 HTTP {}", resp.status()));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("nimail: 解析邮件响应失败: {}", e))?;

        let success = data.get("success").and_then(|v| v.as_str()).unwrap_or("");
        if success != "true" {
            return Ok(vec![]);
        }
        let list = match data.get("mail").and_then(|v| v.as_array()) {
            Some(arr) if !arr.is_empty() => arr,
            _ => return Ok(vec![]),
        };

        let mut out = Vec::with_capacity(list.len());
        for msg in list {
            let flat = serde_json::json!({
                "id": json_str(msg.get("id")),
                "from": first_str(msg, &["from", "sender"]),
                "to": email,
                "subject": first_str(msg, &["subject", "title"]),
                "text": first_str(msg, &["text", "content"]),
                "html": first_str(msg, &["html", "content"]),
                "date": first_str(msg, &["date", "time"]),
                "isRead": false,
            });
            out.push(normalize_email(&flat, email));
        }
        Ok(out)
    })
}

/// 按优先级从消息对象提取首个非空字符串字段
fn first_str(msg: &Value, keys: &[&str]) -> String {
    for k in keys {
        let s = json_str(msg.get(k));
        if !s.is_empty() {
            return s;
        }
    }
    String::new()
}

/// 安全提取 JSON 值为字符串（字符串原样、数字转文本）
fn json_str(v: Option<&Value>) -> String {
    match v {
        Some(v) if v.is_string() => v.as_str().unwrap_or("").to_string(),
        Some(v) if v.is_number() => v.to_string(),
        _ => String::new(),
    }
}
