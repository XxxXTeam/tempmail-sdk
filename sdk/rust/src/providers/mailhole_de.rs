/*!
 * mailhole.de 临时邮箱渠道
 *
 * 特点：无需认证，token 即邮箱地址本身。
 * - 创建邮箱：GET https://mailhole.de/api/random 返回 HTML，正则提取 `xxx@mailhole.de`
 * - 获取邮件：GET https://mailhole.de/json/{email} 返回 JSON 数组
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use regex_lite::Regex;
use serde_json::Value;
use std::sync::LazyLock;

const BASE: &str = "https://mailhole.de";

/// 从 HTML 响应中提取 mailhole.de 邮箱地址的正则
static EMAIL_RE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r"([a-z0-9.]+@mailhole\.de)").expect("mailhole 正则编译失败"));

/// 为请求附加通用浏览器头部
fn mh_headers(b: wreq::RequestBuilder) -> wreq::RequestBuilder {
    b.header("Accept", "*/*")
        .header("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8,en-US;q=0.7")
        .header("Referer", "https://mailhole.de/")
        .header("User-Agent", get_current_ua())
}

/// 创建邮箱：请求随机地址接口并从 HTML 中解析出邮箱
pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let resp = mh_headers(http_client().get(format!("{}/api/random", BASE)))
            .send()
            .await
            .map_err(|e| format!("mailhole-de: 创建邮箱请求失败: {}", e))?;
        if !resp.status().is_success() {
            return Err(format!("mailhole-de: 创建邮箱返回 HTTP {}", resp.status()));
        }
        let body = resp
            .text()
            .await
            .map_err(|e| format!("mailhole-de: 读取创建邮箱响应失败: {}", e))?;

        let email = EMAIL_RE
            .captures(&body)
            .and_then(|c| c.get(1))
            .map(|m| m.as_str().to_string())
            .ok_or("mailhole-de: 未能从响应中解析出邮箱地址")?;

        Ok(EmailInfo {
            channel: Channel::MailholeDe,
            email: email.clone(),
            token: Some(email),
            expires_at: None,
            created_at: None,
        })
    })
}

/// 获取邮件：以邮箱地址请求 JSON 接口并标准化每一封邮件
pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let addr = email.trim();
    let addr = if addr.is_empty() { token.trim() } else { addr };
    if addr.is_empty() {
        return Err("mailhole-de: 邮箱地址为空".into());
    }
    let url = format!("{}/json/{}", BASE, urlencoding::encode(addr));
    block_on(async {
        let resp = mh_headers(http_client().get(url))
            .send()
            .await
            .map_err(|e| format!("mailhole-de: 获取邮件请求失败: {}", e))?;
        if !resp.status().is_success() {
            return Err(format!("mailhole-de: 获取邮件返回 HTTP {}", resp.status()));
        }
        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("mailhole-de: 解析邮件响应失败: {}", e))?;
        let rows = data.as_array().cloned().unwrap_or_default();
        Ok(rows.iter().map(|raw| normalize_email(raw, addr)).collect())
    })
}
