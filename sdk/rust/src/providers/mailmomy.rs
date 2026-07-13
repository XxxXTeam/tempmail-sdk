/*!
 * mailmomy 渠道实现
 * 网站: https://mailmomy.com
 *
 * 独立自研后端，免注册、无鉴权、无 CAPTCHA、纯 GET JSON API：
 *   1. GET /api/domains/active 拉取可用域名池（JSON 字符串数组），随机选一个域名
 *   2. 本地随机 10 位 [a-z0-9] local part 组成邮箱地址
 *   3. GET /api/mail/messages?to={email}&page=1&limit=20 读取邮件列表
 *
 * token 即邮箱地址本身。
 */

use rand::Rng;
use serde_json::Value;
use wreq::Client;

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};

const BASE_URL: &str = "https://mailmomy.com";
const DEFAULT_DOMAIN: &str = "mailmomy.com";

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

/// 拉取 mailmomy 当前可用域名池并随机选择一个
/// 接口失败或返回空时回退到默认 mailmomy.com
async fn pick_domain(client: &Client) -> String {
    let resp = client
        .get(format!("{}/api/domains/active", BASE_URL))
        .header("User-Agent", get_current_ua())
        .header("Accept", "application/json")
        .send()
        .await;

    let resp = match resp {
        Ok(r) if r.status().is_success() => r,
        _ => return DEFAULT_DOMAIN.to_string(),
    };

    let data: Value = match resp.json().await {
        Ok(v) => v,
        Err(_) => return DEFAULT_DOMAIN.to_string(),
    };

    let list: Vec<String> = match data.as_array() {
        Some(arr) => arr
            .iter()
            .filter_map(|v| v.as_str())
            .filter(|s| !s.is_empty())
            .map(|s| s.to_string())
            .collect(),
        None => return DEFAULT_DOMAIN.to_string(),
    };

    if list.is_empty() {
        return DEFAULT_DOMAIN.to_string();
    }
    let idx = rand::thread_rng().gen_range(0..list.len());
    list[idx].clone()
}

/// 创建 mailmomy.com 临时邮箱
/// 服务免注册，直接构造 <local>@<域名> 即可收信
pub fn generate_email() -> Result<EmailInfo, String> {
    let client = http_client();

    block_on(async {
        let domain = pick_domain(&client).await;
        let email = format!("{}@{}", random_local(10), domain);
        Ok(EmailInfo {
            channel: Channel::Mailmomy,
            email: email.clone(),
            token: Some(email),
            expires_at: None,
            created_at: None,
        })
    })
}

/// 获取 mailmomy.com 邮件列表
/// GET /api/mail/messages?to={email}&page=1&limit=20
/// 返回 {emails:[{id,recipient,from,subject,message,bodyText,receivedAt}], total, page, limit, pages}
pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    if email.is_empty() {
        return Err("mailmomy: 缺少邮箱地址".into());
    }
    let client = http_client();

    block_on(async {
        let url = format!(
            "{}/api/mail/messages?to={}&page=1&limit=20",
            BASE_URL,
            encode_mail(email)
        );
        let resp = client
            .get(url)
            .header("User-Agent", get_current_ua())
            .header("Accept", "application/json")
            .send()
            .await
            .map_err(|e| format!("mailmomy: 获取邮件请求失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("mailmomy: 获取邮件失败 HTTP {}", resp.status()));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("mailmomy: 解析邮件响应失败: {}", e))?;

        let list = match data.get("emails").and_then(|v| v.as_array()) {
            Some(arr) if !arr.is_empty() => arr,
            _ => return Ok(vec![]),
        };

        let mut out = Vec::with_capacity(list.len());
        for msg in list {
            let recipient = json_str(msg.get("recipient"));
            let to = if recipient.is_empty() {
                email.to_string()
            } else {
                recipient
            };
            let message = json_str(msg.get("message"));
            let body_text = json_str(msg.get("bodyText"));
            let text = if body_text.is_empty() {
                message.clone()
            } else {
                body_text
            };
            let flat = serde_json::json!({
                "id": json_str(msg.get("id")),
                "from": json_str(msg.get("from")),
                "to": to,
                "subject": json_str(msg.get("subject")),
                "text": text,
                "html": message,
                "date": json_str(msg.get("receivedAt")),
                "isRead": false,
            });
            out.push(normalize_email(&flat, email));
        }
        Ok(out)
    })
}

/// 安全提取 JSON 值为字符串（字符串原样、数字转文本）
fn json_str(v: Option<&Value>) -> String {
    match v {
        Some(v) if v.is_string() => v.as_str().unwrap_or("").to_string(),
        Some(v) if v.is_number() => v.to_string(),
        _ => String::new(),
    }
}
