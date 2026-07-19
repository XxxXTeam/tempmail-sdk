/*!
 * smailpro 渠道实现
 * 网站: https://smailpro.com
 *
 * 流程为两段式：
 *   1. GET https://smailpro.com/app/payload?url={目标API}[&email=&mid=] → 返回 JWT（纯文本）
 *   2. 带 JWT 调用 api.sonjj.com 对应接口（payload={JWT}）
 *
 * 创建邮箱、获取列表、获取详情均需先取 payload 再调用 sonjj API。
 *
 * token 存储: 空字符串（本渠道不需要额外持久化 token）
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::{json, Value};

const BASE_URL: &str = "https://smailpro.com";
const API_BASE_URL: &str = "https://api.sonjj.com/v1/temp_email";

/// 设置 smailpro/sonjj 请求通用请求头
fn set_headers(req: wreq::RequestBuilder) -> wreq::RequestBuilder {
    req.header("User-Agent", get_current_ua())
        .header("Accept", "application/json, text/plain, */*")
        .header("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
        .header("Referer", format!("{BASE_URL}/"))
}

/// 获取访问 sonjj API 所需的 JWT
/// target_url 为目标 sonjj 接口地址（未编码），extra 为附加查询参数（email、mid 等）
async fn fetch_payload(target_url: &str, extra: &[(&str, &str)]) -> Result<String, String> {
    let mut query = format!("url={}", urlencoding::encode(target_url));
    for (k, v) in extra {
        query.push('&');
        query.push_str(k);
        query.push('=');
        query.push_str(&urlencoding::encode(v));
    }

    let req_url = format!("{BASE_URL}/app/payload?{query}");
    let resp = set_headers(http_client().get(&req_url))
        .send()
        .await
        .map_err(|e| format!("smailpro: 获取 payload 失败: {}", e))?;

    if !resp.status().is_success() {
        return Err(format!("smailpro payload: {}", resp.status()));
    }

    let text = resp
        .text()
        .await
        .map_err(|e| format!("smailpro: 读取 payload 响应失败: {}", e))?;

    // payload 接口返回纯文本 JWT，去除可能的引号与空白
    let payload = text.trim().trim_matches('"').to_string();
    if payload.is_empty() {
        return Err("smailpro: payload 为空".into());
    }
    Ok(payload)
}

/// 携带 JWT 调用 sonjj API 并返回解析后的 JSON
/// target_url 为目标接口地址，extra 为获取 payload 时需要的附加参数（email、mid）
async fn call_api(target_url: &str, extra: &[(&str, &str)], label: &str) -> Result<Value, String> {
    let payload = fetch_payload(target_url, extra).await?;

    let req_url = format!("{target_url}?payload={}", urlencoding::encode(&payload));
    let resp = set_headers(http_client().get(&req_url))
        .send()
        .await
        .map_err(|e| format!("smailpro: {} 请求失败: {}", label, e))?;

    if !resp.status().is_success() {
        return Err(format!("smailpro {}: {}", label, resp.status()));
    }

    resp.json()
        .await
        .map_err(|e| format!("smailpro: 解析 {} 响应失败: {}", label, e))
}

/// 创建临时邮箱
/// 调用 sonjj create 接口，返回 {"email":"...","expired_at":...}
pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let data = call_api(&format!("{API_BASE_URL}/create"), &[], "create").await?;

        let email = data
            .get("email")
            .and_then(|v| v.as_str())
            .map(|s| s.trim().to_string())
            .unwrap_or_default();

        if email.is_empty() {
            return Err("smailpro: 创建邮箱失败, 未返回邮箱地址".into());
        }

        // expired_at 可能为数字（秒/毫秒时间戳）
        let expires_at = data
            .get("expired_at")
            .and_then(|v| v.as_f64())
            .filter(|n| *n > 0.0)
            .map(|n| {
                if n > 1e12 {
                    n as i64
                } else {
                    (n as i64) * 1000
                }
            });

        Ok(EmailInfo {
            channel: Channel::Smailpro,
            email,
            token: Some(String::new()),
            expires_at,
            created_at: None,
        })
    })
}

/// 获取邮件列表
/// 1. 调用 sonjj inbox 接口获取列表（仅含 mid、from、subject、datetime）
/// 2. 对每封邮件调用 message 接口获取正文，再统一标准化
///
/// token 未使用，可传空字符串
pub fn get_emails(_token: &str, email: &str) -> Result<Vec<Email>, String> {
    let em = email.trim();
    if em.is_empty() {
        return Err("smailpro: 邮箱地址为空".into());
    }

    block_on(async {
        let data = call_api(&format!("{API_BASE_URL}/inbox"), &[("email", em)], "inbox").await?;

        let messages = match data
            .get("data")
            .and_then(|d| d.get("messages"))
            .and_then(|v| v.as_array())
        {
            Some(arr) if !arr.is_empty() => arr.clone(),
            _ => return Ok(vec![]),
        };

        let mut result = Vec::with_capacity(messages.len());
        for m in &messages {
            let mid = m.get("mid").and_then(|v| v.as_str()).unwrap_or("").trim();

            let mut flat = json!({
                "id": mid,
                "from": m.get("from").and_then(|v| v.as_str()).unwrap_or(""),
                "to": em,
                "subject": m.get("subject").and_then(|v| v.as_str()).unwrap_or(""),
                "date": m.get("datetime").and_then(|v| v.as_str()).unwrap_or(""),
            });

            // 拉取邮件正文（含 HTML 与纯文本），失败时保留列表元信息
            if !mid.is_empty() {
                if let Some((html, text)) = fetch_message(em, mid).await {
                    if !html.is_empty() || !text.is_empty() {
                        flat["html"] = json!(html);
                        flat["text"] = json!(text);
                    }
                }
            }

            result.push(normalize_email(&flat, em));
        }

        Ok(result)
    })
}

/// 获取单封邮件正文，返回 (html, text)
/// 调用 sonjj message 接口，需在 payload 参数中携带 email 与 mid
async fn fetch_message(email: &str, mid: &str) -> Option<(String, String)> {
    let data = call_api(
        &format!("{API_BASE_URL}/message"),
        &[("email", email), ("mid", mid)],
        "message",
    )
    .await
    .ok()?;

    let html = data
        .get("body")
        .and_then(|v| v.as_str())
        .unwrap_or("")
        .to_string();
    let text = data
        .get("textBody")
        .and_then(|v| v.as_str())
        .unwrap_or("")
        .to_string();
    Some((html, text))
}
