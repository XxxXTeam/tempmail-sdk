/*!
 * linshiyouxiang.net 渠道实现
 * 网站: https://www.linshiyouxiang.net
 *
 * 流程:
 *   1. GET / 获取 temp_mail cookie，从 HTML 正则提取 tempMailGlobal(邮箱)
 *      和 mailCodeGlobal(校验 code，HMAC 哈希)
 *   2. POST /get-messages  body: {"email":"...","code":"..."}
 *      返回 {"emails":null|[...],"success":true}
 *
 * token 存储: mailCodeGlobal 的值（后续请求用于校验）
 */

use std::sync::LazyLock;

use regex::Regex;
use serde_json::Value;

use crate::config::{block_on, get_current_ua, http_client_no_cookie_jar};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};

const BASE_URL: &str = "https://www.linshiyouxiang.net";

/// 提取首页 HTML 中的 tempMailGlobal（邮箱地址）
static EMAIL_RE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r#"tempMailGlobal\s*=\s*'([^']+)'"#).expect("re"));

/// 提取首页 HTML 中的 mailCodeGlobal（校验 code）
static CODE_RE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r#"mailCodeGlobal\s*=\s*'([^']+)'"#).expect("re"));

/// 创建临时邮箱
/// 1. GET / 获取首页 HTML
/// 2. 从 HTML 正则提取 tempMailGlobal（邮箱）和 mailCodeGlobal（校验 code）
///
/// token 存储 mailCodeGlobal
pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let client = http_client_no_cookie_jar();

        let resp = client
            .get(format!("{BASE_URL}/"))
            .header("User-Agent", get_current_ua())
            .header(
                "Accept",
                "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
            )
            .header("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
            .send()
            .await
            .map_err(|e| format!("linshiyouxiang-net: 请求首页失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!(
                "linshiyouxiang-net: 首页返回 HTTP {}",
                resp.status()
            ));
        }

        let html = resp
            .text()
            .await
            .map_err(|e| format!("linshiyouxiang-net: 读取首页失败: {}", e))?;

        // 提取邮箱地址
        let email = EMAIL_RE
            .captures(&html)
            .and_then(|c| c.get(1))
            .map(|m| m.as_str().trim().to_string())
            .filter(|s| !s.is_empty())
            .ok_or_else(|| "linshiyouxiang-net: 未能从首页提取邮箱地址".to_string())?;

        // 提取校验 code
        let code = CODE_RE
            .captures(&html)
            .and_then(|c| c.get(1))
            .map(|m| m.as_str().trim().to_string())
            .unwrap_or_default();

        Ok(EmailInfo {
            channel: Channel::LinshiyouxiangNet,
            email,
            token: Some(code),
            expires_at: None,
            created_at: None,
        })
    })
}

/// 获取邮件列表
/// POST /get-messages  body: {"email":"...","code":"<token>"}
pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let em = email.trim();
    if em.is_empty() {
        return Err("linshiyouxiang-net: 邮箱地址为空".into());
    }

    block_on(async {
        let client = http_client_no_cookie_jar();
        let req_body = serde_json::json!({ "email": em, "code": token }).to_string();

        let resp = client
            .post(format!("{BASE_URL}/get-messages"))
            .header("User-Agent", get_current_ua())
            .header("Accept", "application/json, text/javascript, */*; q=0.01")
            .header("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
            .header("Content-Type", "application/json")
            .header("X-Requested-With", "XMLHttpRequest")
            .header("Referer", format!("{BASE_URL}/"))
            .header("Origin", BASE_URL)
            .body(req_body)
            .send()
            .await
            .map_err(|e| format!("linshiyouxiang-net: 请求获取邮件失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!(
                "linshiyouxiang-net: get-messages 返回 HTTP {}",
                resp.status()
            ));
        }

        // 解析响应: {"emails":null,"success":true} 或 {"emails":[...],"success":true}
        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("linshiyouxiang-net: 解析邮件列表失败: {}", e))?;

        let emails = match data.get("emails").and_then(|v| v.as_array()) {
            Some(arr) if !arr.is_empty() => arr,
            _ => return Ok(vec![]),
        };

        let mut result = Vec::with_capacity(emails.len());
        for item in emails {
            result.push(normalize_email(item, em));
        }

        Ok(result)
    })
}
