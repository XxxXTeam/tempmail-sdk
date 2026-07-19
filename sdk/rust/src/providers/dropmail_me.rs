/*!
 * dropmail-me 渠道实现
 * 网站: https://dropmail.me
 * 流程：GET /en/ 提取 data-k → 解码 secret → 生成 auth token
 *       GraphQL introduceSession 创建会话 → session(id) 获取邮件
 * token 存储 JSON: {"session_id":"...","auth_token":"website_..."}
 */

use base64::Engine;
use chrono::Utc;
use rand::Rng;
use regex::Regex;

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use serde_json::Value;

const BASE_URL: &str = "https://dropmail.me";

/// FNV-1a 哈希（与前端 JS 实现一致）
fn fnv_hash(s: &str) -> String {
    let mut hash: u32 = 2166136261;
    for byte in s.bytes() {
        hash ^= byte as u32;
        hash = hash.wrapping_add(
            (hash << 1)
                .wrapping_add((hash << 4).wrapping_add(
                    (hash << 7).wrapping_add((hash << 8).wrapping_add(hash << 24)),
                )),
        );
    }
    format!("{:x}", hash)
}

/// 从 data-k 值中提取 secret: reverse + base64 decode
fn extract_secret(data_k: &str) -> Result<String, String> {
    let reversed: String = data_k.chars().rev().collect();
    let decoded = base64::engine::general_purpose::STANDARD
        .decode(&reversed)
        .map_err(|e| format!("dropmail-me: base64 解码 secret 失败: {}", e))?;
    String::from_utf8(decoded).map_err(|e| format!("dropmail-me: secret 非 UTF-8: {}", e))
}

/// 生成随机部分：YYYYMMDD + 16位随机字母数字
fn random_part() -> String {
    let date_str = Utc::now().format("%Y%m%d").to_string();
    let chars: &[u8] = b"abcdefghijklmnopqrstuvwxyz0123456789";
    let mut rng = rand::thread_rng();
    let rand_str: String = (0..16).map(|_| chars[rng.gen_range(0..chars.len())] as char).collect();
    format!("{}{}", date_str, rand_str)
}

/// 从页面提取 data-k 并生成 auth token
fn generate_token() -> Result<String, String> {
    block_on(async {
        let resp = http_client()
            .get(format!("{}/en/", BASE_URL))
            .header("User-Agent", get_current_ua())
            .header(
                "Accept",
                "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
            )
            .send()
            .await
            .map_err(|e| format!("dropmail-me: 获取页面失败: {}", e))?;

        let status = resp.status();
        let text = resp
            .text()
            .await
            .map_err(|e| format!("dropmail-me: 读取页面响应失败: {}", e))?;

        if !status.is_success() {
            return Err(format!("dropmail-me: 获取页面失败: {} {}", status, text));
        }

        let re = Regex::new(r#"<meta\s+name="app-config"\s+data-k="([^"]+)""#).unwrap();
        let caps = re
            .captures(&text)
            .ok_or_else(|| "dropmail-me: 未找到 data-k 属性".to_string())?;
        let data_k = &caps[1];

        let secret = extract_secret(data_k)?;
        let rnd = random_part();
        let hash = fnv_hash(&format!("{}{}", rnd, secret));
        Ok(format!("website_{}_{}", rnd, hash))
    })
}

/// 执行 GraphQL 请求
fn graphql_request(auth_token: &str, query: &str) -> Result<Value, String> {
    block_on(async {
        let api_url = format!("{}/api/graphql/{}", BASE_URL, auth_token);
        let body = serde_json::json!({"query": query});

        let resp = http_client()
            .post(&api_url)
            .header("Content-Type", "application/json")
            .header("User-Agent", get_current_ua())
            .json(&body)
            .send()
            .await
            .map_err(|e| format!("dropmail-me: GraphQL 请求失败: {}", e))?;

        let status = resp.status();
        let text = resp
            .text()
            .await
            .map_err(|e| format!("dropmail-me: 读取 GraphQL 响应失败: {}", e))?;

        if !status.is_success() {
            return Err(format!("dropmail-me: GraphQL 请求失败: {} {}", status, text));
        }

        let data: Value = serde_json::from_str(&text)
            .map_err(|e| format!("dropmail-me: 解析 GraphQL 响应失败: {}", e))?;
        Ok(data)
    })
}

/// 创建临时邮箱
pub fn generate_email(_duration: u32, _domain: Option<&str>) -> Result<EmailInfo, String> {
    let auth_token = generate_token()?;

    let query = "mutation { introduceSession { id expiresAt addresses { address } } }";
    let data = graphql_request(&auth_token, query)?;

    let session = &data["data"]["introduceSession"];
    let session_id = session["id"]
        .as_str()
        .ok_or_else(|| format!("dropmail-me: 创建会话失败，响应: {}", data))?;

    let addresses = session["addresses"]
        .as_array()
        .ok_or("dropmail-me: 会话中无可用邮箱地址")?;

    if addresses.is_empty() {
        return Err("dropmail-me: 会话中无可用邮箱地址".to_string());
    }

    let email_addr = addresses[0]["address"]
        .as_str()
        .ok_or("dropmail-me: 地址字段缺失")?
        .to_string();

    let token_payload = serde_json::json!({
        "session_id": session_id,
        "auth_token": auth_token,
    })
    .to_string();

    Ok(EmailInfo {
        channel: Channel::DropmailMe,
        email: email_addr,
        token: Some(token_payload),
        expires_at: None,
        created_at: None,
    })
}

/// 获取邮件列表
pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let session: Value = serde_json::from_str(token)
        .map_err(|e| format!("dropmail-me: 解析 token 失败: {}", e))?;

    let session_id = session["session_id"]
        .as_str()
        .ok_or("dropmail-me: token 中缺少 session_id 字段")?;
    let auth_token = session["auth_token"]
        .as_str()
        .ok_or("dropmail-me: token 中缺少 auth_token 字段")?;

    let query = format!(
        r#"{{ session(id:"{}") {{ mails {{ id headerFrom headerSubject text html receivedAt }} }} }}"#,
        session_id
    );
    let data = graphql_request(auth_token, &query)?;

    let mails = data["data"]["session"]["mails"]
        .as_array()
        .cloned()
        .unwrap_or_default();

    let mut result = Vec::new();
    for m in &mails {
        let raw = serde_json::json!({
            "id": m["id"].as_str().unwrap_or(""),
            "from": m["headerFrom"].as_str().unwrap_or(""),
            "to": email,
            "subject": m["headerSubject"].as_str().unwrap_or(""),
            "text": m["text"].as_str().unwrap_or(""),
            "html": m["html"].as_str().unwrap_or(""),
            "received_at": m["receivedAt"].as_str().unwrap_or(""),
        });
        result.push(normalize_email(&raw, email));
    }

    Ok(result)
}
