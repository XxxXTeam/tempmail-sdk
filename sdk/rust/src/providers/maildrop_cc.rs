/*!
 * maildrop.cc 渠道实现
 * 网站: https://maildrop.cc
 *
 * 流程（GraphQL API，无认证）:
 *   1. 创建邮箱：无需 API 调用，直接生成随机用户名 + "@maildrop.cc"
 *      （公共邮箱，任何人可查看任意地址收件箱）
 *   2. 获取邮件：POST https://api.maildrop.cc/graphql
 *      - inbox(mailbox) 查询邮件列表（id/headerfrom/subject/date，无正文）
 *      - message(mailbox,id) 查询单封详情（含 html 正文）
 *
 * token 存储: 空字符串（公共邮箱，无需令牌）
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::{json, Value};

const DOMAIN: &str = "maildrop.cc";
const GRAPHQL_URL: &str = "https://api.maildrop.cc/graphql";

/// 生成随机用户名（小写字母 + 数字）
fn random_local(n: usize) -> String {
    const CHARS: &[u8] = b"abcdefghijklmnopqrstuvwxyz0123456789";
    (0..n)
        .map(|_| {
            let b = rand::random::<u8>();
            CHARS[(b as usize) % CHARS.len()] as char
        })
        .collect()
}

/// 提取地址 @ 前的用户名部分
fn mailbox_of(email: &str) -> String {
    let email = email.trim();
    match email.split_once('@') {
        Some((local, _)) => local.to_string(),
        None => email.to_string(),
    }
}

/// 构造 inbox 列表查询语句（邮箱名经 JSON 序列化转义，避免注入）
fn inbox_query(mailbox: &str) -> String {
    let mb = Value::String(mailbox.to_string()).to_string();
    format!("query {{ inbox(mailbox: {mb}) {{ id headerfrom subject date }} }}")
}

/// 构造 message 单封详情查询语句
fn message_query(mailbox: &str, id: &str) -> String {
    let mb = Value::String(mailbox.to_string()).to_string();
    let mid = Value::String(id.to_string()).to_string();
    format!("query {{ message(mailbox: {mb}, id: {mid}) {{ id headerfrom subject date html }} }}")
}

/// 发送 GraphQL 请求并返回解析后的 JSON
async fn do_graphql(query: String) -> Result<Value, String> {
    let resp = http_client()
        .post(GRAPHQL_URL)
        .header("Accept", "application/json")
        .header("Content-Type", "application/json")
        .header("Origin", format!("https://{DOMAIN}"))
        .header("Referer", format!("https://{DOMAIN}/"))
        .header("User-Agent", get_current_ua())
        .json(&json!({ "query": query }))
        .send()
        .await
        .map_err(|e| format!("maildrop-cc: GraphQL 请求失败: {}", e))?;

    if !resp.status().is_success() {
        return Err(format!("maildrop-cc graphql: {}", resp.status()));
    }

    resp.json()
        .await
        .map_err(|e| format!("maildrop-cc: 解析 GraphQL 响应失败: {}", e))
}

/// 创建临时邮箱
/// 无需 API 调用，直接生成随机用户名 + "@maildrop.cc"
pub fn generate_email() -> Result<EmailInfo, String> {
    let email = format!("{}@{}", random_local(10), DOMAIN);
    Ok(EmailInfo {
        channel: Channel::MaildropCc,
        email,
        token: Some(String::new()),
        expires_at: None,
        created_at: None,
    })
}

/// 获取邮件列表
/// 先用 inbox 查询拿到 id 列表，再逐封用 message 查询补全 html 正文
/// maildrop.cc 为公共邮箱服务，无需 token（token 参数保留以对齐接口，忽略）
pub fn get_emails(_token: &str, email: &str) -> Result<Vec<Email>, String> {
    let mailbox = mailbox_of(email);
    if mailbox.is_empty() {
        return Err("maildrop-cc: 邮箱地址为空".into());
    }

    block_on(async {
        // 1. 查询邮件列表
        let inbox_resp = do_graphql(inbox_query(&mailbox)).await?;
        let items = match inbox_resp
            .get("data")
            .and_then(|d| d.get("inbox"))
            .and_then(|v| v.as_array())
        {
            Some(arr) if !arr.is_empty() => arr.clone(),
            _ => return Ok(vec![]),
        };

        // 2. 逐封查询详情补全 html，详情失败时回退使用列表元信息
        let mut result = Vec::with_capacity(items.len());
        for item in &items {
            let id = item.get("id").and_then(|v| v.as_str()).unwrap_or("");
            if id.is_empty() {
                continue;
            }

            let detail = do_graphql(message_query(&mailbox, id))
                .await
                .ok()
                .and_then(|v| v.get("data").and_then(|d| d.get("message")).cloned())
                .filter(|m| !m.is_null());

            let raw = match detail {
                Some(m) => json!({
                    "id": m.get("id").and_then(|v| v.as_str()).unwrap_or(id),
                    "from": m.get("headerfrom").and_then(|v| v.as_str()).unwrap_or(""),
                    "to": email,
                    "subject": m.get("subject").and_then(|v| v.as_str()).unwrap_or(""),
                    "date": m.get("date").and_then(|v| v.as_str()).unwrap_or(""),
                    "html": m.get("html").and_then(|v| v.as_str()).unwrap_or(""),
                }),
                None => json!({
                    "id": id,
                    "from": item.get("headerfrom").and_then(|v| v.as_str()).unwrap_or(""),
                    "to": email,
                    "subject": item.get("subject").and_then(|v| v.as_str()).unwrap_or(""),
                    "date": item.get("date").and_then(|v| v.as_str()).unwrap_or(""),
                }),
            };
            result.push(normalize_email(&raw, email));
        }

        Ok(result)
    })
}
