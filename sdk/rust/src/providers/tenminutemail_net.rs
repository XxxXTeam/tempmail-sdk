/*!
 * 10minutemail.net 渠道实现
 * 网站: https://10minutemail.net
 *
 * 流程:
 *   1. GET /address.api.php 获取邮箱地址，从 Set-Cookie 提取 PHPSESSID
 *   2. GET /address.api.php 带 Cookie 获取邮件列表（mail_list 数组）
 *   3. GET /mail.api.php?mailid={id} 获取单封邮件详情
 *
 * token 存储: JSON {"cookie":"PHPSESSID=xxx"}
 */

use crate::config::{block_on, get_current_ua, http_client_no_cookie_jar};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::Value;

const BASE_URL: &str = "https://10minutemail.net";

/// 创建临时邮箱
pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let client = http_client_no_cookie_jar();
        let resp = client
            .get(format!("{}/address.api.php", BASE_URL))
            .header("User-Agent", get_current_ua())
            .header("Accept", "application/json")
            .send()
            .await
            .map_err(|e| format!("10minutemail.net 创建邮箱请求失败: {}", e))?;

        // 从响应头提取 PHPSESSID
        let cookie_str = extract_phpsessid(&resp)?;

        let status = resp.status();
        let text = resp
            .text()
            .await
            .map_err(|e| format!("10minutemail.net 读取响应失败: {}", e))?;

        if !status.is_success() {
            return Err(format!("10minutemail.net 创建邮箱失败: {} {}", status, text));
        }

        let data: Value = serde_json::from_str(&text)
            .map_err(|e| format!("10minutemail.net 解析响应失败: {}", e))?;

        let address = data["mail_get_mail"]
            .as_str()
            .ok_or("10minutemail.net: 响应中缺少 mail_get_mail 字段")?
            .to_string();

        let expires_at = data["mail_get_duetime"].as_i64().map(|t| t * 1000);

        let token_payload = serde_json::json!({
            "cookie": cookie_str,
        })
        .to_string();

        Ok(EmailInfo {
            channel: Channel::TenminutemailNet,
            email: address,
            token: Some(token_payload),
            expires_at,
            created_at: None,
        })
    })
}

/// 从 Set-Cookie 响应头提取 PHPSESSID
fn extract_phpsessid(resp: &wreq::Response) -> Result<String, String> {
    for val in resp.headers().get_all("set-cookie") {
        let s = val.to_str().unwrap_or("");
        if let Some(start) = s.find("PHPSESSID=") {
            let rest = &s[start..];
            let end = rest.find(';').unwrap_or(rest.len());
            return Ok(rest[..end].to_string());
        }
    }
    Err("10minutemail.net: 响应中未找到 PHPSESSID".to_string())
}

/// 获取邮件列表
pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let session: Value = serde_json::from_str(token)
        .map_err(|e| format!("10minutemail.net 解析 token 失败: {}", e))?;

    let cookie = session["cookie"]
        .as_str()
        .ok_or("10minutemail.net: token 中缺少 cookie 字段")?;

    block_on(async {
        let client = http_client_no_cookie_jar();

        // 获取邮件列表
        let resp = client
            .get(format!("{}/address.api.php", BASE_URL))
            .header("User-Agent", get_current_ua())
            .header("Accept", "application/json")
            .header("Cookie", cookie)
            .send()
            .await
            .map_err(|e| format!("10minutemail.net 获取邮件列表请求失败: {}", e))?;

        let status = resp.status();
        let text = resp
            .text()
            .await
            .map_err(|e| format!("10minutemail.net 读取响应失败: {}", e))?;

        if !status.is_success() {
            return Err(format!("10minutemail.net 获取邮件列表失败: {} {}", status, text));
        }

        let data: Value = serde_json::from_str(&text)
            .map_err(|e| format!("10minutemail.net 解析邮件列表失败: {}", e))?;

        let mail_list = match data["mail_list"].as_array() {
            Some(arr) => arr.clone(),
            None => return Ok(Vec::new()),
        };

        let mut result = Vec::new();
        for item in &mail_list {
            let mail_id = item["mail_id"].as_str().unwrap_or("");
            // 排除系统欢迎邮件
            if mail_id.is_empty() || mail_id == "welcome" {
                continue;
            }

            // 获取邮件详情
            let detail = fetch_mail_detail(cookie, mail_id, email).await?;
            result.push(detail);
        }

        Ok(result)
    })
}

/// 获取单封邮件详情
async fn fetch_mail_detail(cookie: &str, mail_id: &str, email: &str) -> Result<Email, String> {
    let client = http_client_no_cookie_jar();

    let resp = client
        .get(format!("{}/mail.api.php?mailid={}", BASE_URL, mail_id))
        .header("User-Agent", get_current_ua())
        .header("Accept", "application/json")
        .header("Cookie", cookie)
        .send()
        .await
        .map_err(|e| format!("10minutemail.net 获取邮件详情失败: {}", e))?;

    let status = resp.status();
    let text = resp
        .text()
        .await
        .map_err(|e| format!("10minutemail.net 读取邮件详情失败: {}", e))?;

    if !status.is_success() {
        return Err(format!("10minutemail.net 获取邮件详情失败: {} {}", status, text));
    }

    let detail: Value = serde_json::from_str(&text)
        .map_err(|e| format!("10minutemail.net 解析邮件详情失败: {}", e))?;

    // 提取 HTML 和纯文本内容
    let mut html_body = String::new();
    let mut text_body = String::new();

    if let Some(body_arr) = detail["body"].as_array() {
        for part in body_arr {
            let content_type = part["content"].as_str().unwrap_or("");
            let body_content = part["body"].as_str().unwrap_or("");
            if content_type.contains("text/html") {
                html_body = body_content.to_string();
            } else if content_type.contains("text/plain") {
                text_body = body_content.to_string();
            }
        }
    }

    // 如果 body 数组中没有纯文本，从 plain 字段提取
    if text_body.is_empty() {
        if let Some(plain_arr) = detail["plain"].as_array() {
            let parts: Vec<&str> = plain_arr
                .iter()
                .filter_map(|v| v.as_str())
                .collect();
            text_body = parts.join("\n");
        }
    }

    let raw = serde_json::json!({
        "id": mail_id,
        "from": detail["from"].as_str().unwrap_or(""),
        "to": email,
        "subject": detail["subject"].as_str().unwrap_or(""),
        "html": html_body,
        "text": text_body,
        "date": detail["datetime"].as_str().unwrap_or(""),
        "isRead": false,
        "attachments": [],
    });

    Ok(normalize_email(&raw, email))
}
