/*!
 * mail.td 渠道实现
 * 网站: https://mail.td
 *
 * 流程:
 *   1. GET https://api.mail.td/api/domains
 *      返回: {"domains":[{"domain":"sugtbt.com","pro_only":false}, ...]}
 *      从中随机挑选一个 pro_only=false 的域名
 *
 *   2. 本地生成随机用户名与密码，组成邮箱地址 address
 *      auth_key = sha256hex(password)
 *      计算 SHA-256 Proof-of-Work：
 *      input = address.toLowerCase() + timestamp(秒).toString() + nonce.toString()
 *      hash  = SHA-256(input)，要求 hash 具备 difficulty 个前导零位
 *      初始 difficulty = 15
 *
 *   3. POST https://api.mail.td/api/accounts
 *      body: {"address":"user@domain","auth_key":"...","pow":{"t":秒,"n":"nonce","d":difficulty}}
 *      - 成功 201: {"id":"uuid","address":"email","token":"jwt","suggested_next_difficulty":15}
 *      - 需要重试: {"status":"retry","required_difficulty":20,"token":"retry_token"}
 *        此时提升 difficulty 并在 pow 中附带 "token" 字段重试（最多 3 次）
 *
 *   4. GET https://api.mail.td/api/accounts/{id}/messages?page=1
 *      Header: Authorization: Bearer {jwt}
 *      返回: {"messages":[{"id","from":{"address","name"},"subject","text","html","seen","created_at"}],"page":1}
 *
 * token 存储: JSON 字符串 {"jwt":"...","id":"..."}
 */

use crate::config::{block_on, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use rand::Rng;
use serde::{Deserialize, Serialize};
use serde_json::{json, Value};
use sha2::{Digest, Sha256};

const API_BASE: &str = "https://api.mail.td/api";
/// 初始 Proof-of-Work 难度（前导零位数）
const INITIAL_DIFFICULTY: u32 = 15;
/// 创建账户遇到 retry 时的最大重试次数
const MAX_POW_RETRIES: u32 = 3;

/// 序列化进 token 的账户认证信息
#[derive(Serialize, Deserialize)]
struct MailTdToken {
    /// 访问令牌（JWT）
    jwt: String,
    /// 账户 ID，用于拉取邮件
    id: String,
}

/// 生成指定长度的随机字符串（小写字母 + 数字）
fn random_string(len: usize) -> String {
    let charset: &[u8] = b"abcdefghijklmnopqrstuvwxyz0123456789";
    let mut rng = rand::thread_rng();
    (0..len)
        .map(|_| charset[rng.gen_range(0..charset.len())] as char)
        .collect()
}

/// 计算字符串的 SHA-256 十六进制摘要
fn sha256_hex(input: &str) -> String {
    let digest = Sha256::digest(input.as_bytes());
    hex::encode(digest)
}

/// 校验哈希是否满足 difficulty 个前导零位
/// 前 floor(d/8) 字节必须全为 0，第 floor(d/8)+1 字节的高 d%8 位必须为 0
fn has_leading_zero_bits(hash: &[u8], difficulty: u32) -> bool {
    let full_bytes = (difficulty / 8) as usize;
    let rem_bits = difficulty % 8;
    if hash.len() < full_bytes {
        return false;
    }
    for byte in &hash[..full_bytes] {
        if *byte != 0 {
            return false;
        }
    }
    if rem_bits > 0 {
        let mask: u8 = 0xFF << (8 - rem_bits);
        if hash.get(full_bytes).copied().unwrap_or(0xFF) & mask != 0 {
            return false;
        }
    }
    true
}

/// 求解 Proof-of-Work，返回满足难度的 nonce 字符串
/// input = address(小写) + timestamp.toString() + nonce.toString()
fn solve_pow(address: &str, timestamp: u64, difficulty: u32, start_nonce: u64) -> String {
    let addr_lower = address.to_lowercase();
    let ts = timestamp.to_string();
    let mut nonce = start_nonce;
    loop {
        let nonce_str = nonce.to_string();
        let mut hasher = Sha256::new();
        hasher.update(addr_lower.as_bytes());
        hasher.update(ts.as_bytes());
        hasher.update(nonce_str.as_bytes());
        let hash = hasher.finalize();
        if has_leading_zero_bits(&hash, difficulty) {
            return nonce_str;
        }
        nonce = nonce.wrapping_add(1);
    }
}

/// 当前 Unix 时间戳（秒）
fn now_secs() -> u64 {
    std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .map(|d| d.as_secs())
        .unwrap_or(0)
}

/// 构建通用请求头
fn headers(b: wreq::RequestBuilder) -> wreq::RequestBuilder {
    b.header("Accept", "application/json")
        .header("Content-Type", "application/json")
}

/// 获取一个可用域名（pro_only=false）
async fn fetch_domain() -> Result<String, String> {
    let resp = headers(http_client().get(format!("{}/domains", API_BASE)))
        .send()
        .await
        .map_err(|e| format!("mail-td: 获取域名列表请求失败: {}", e))?;

    if !resp.status().is_success() {
        return Err(format!("mail-td: 获取域名列表返回 HTTP {}", resp.status()));
    }

    let data: Value = resp
        .json()
        .await
        .map_err(|e| format!("mail-td: 解析域名列表响应失败: {}", e))?;

    let domains = data
        .get("domains")
        .and_then(|v| v.as_array())
        .ok_or("mail-td: 域名列表为空")?;

    // 仅保留非 pro 域名
    let usable: Vec<&str> = domains
        .iter()
        .filter(|d| !d.get("pro_only").and_then(|v| v.as_bool()).unwrap_or(false))
        .filter_map(|d| d.get("domain").and_then(|v| v.as_str()))
        .filter(|s| !s.is_empty())
        .collect();

    if usable.is_empty() {
        return Err("mail-td: 无可用域名".into());
    }

    let idx = rand::thread_rng().gen_range(0..usable.len());
    Ok(usable[idx].to_string())
}

/// 创建临时邮箱
pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let domain = fetch_domain().await?;
        let username = random_string(12);
        let address = format!("{}@{}", username, domain);

        // 密码及其 SHA-256 十六进制作为 auth_key
        let password = random_string(16);
        let auth_key = sha256_hex(&password);

        let mut difficulty = INITIAL_DIFFICULTY;
        let mut retry_token: Option<String> = None;
        // nonce 起始值随机，避免固定搜索路径
        let start_nonce: u64 = rand::thread_rng().gen::<u32>() as u64;

        // 初始一次 + 最多 MAX_POW_RETRIES 次重试
        for _ in 0..=MAX_POW_RETRIES {
            let timestamp = now_secs();
            let nonce = solve_pow(&address, timestamp, difficulty, start_nonce);

            let mut pow = json!({
                "t": timestamp,
                "n": nonce,
                "d": difficulty,
            });
            if let Some(rt) = &retry_token {
                pow["token"] = json!(rt);
            }

            let body = json!({
                "address": address,
                "auth_key": auth_key,
                "pow": pow,
            });

            let resp = headers(http_client().post(format!("{}/accounts", API_BASE)))
                .body(body.to_string())
                .send()
                .await
                .map_err(|e| format!("mail-td: 创建账户请求失败: {}", e))?;

            let status = resp.status();
            let data: Value = resp
                .json()
                .await
                .map_err(|e| format!("mail-td: 解析创建账户响应失败: {}", e))?;

            // 需要提升难度重试
            if data.get("status").and_then(|v| v.as_str()) == Some("retry") {
                difficulty = data
                    .get("required_difficulty")
                    .and_then(|v| v.as_u64())
                    .map(|v| v as u32)
                    .unwrap_or(difficulty + 1);
                retry_token = data
                    .get("token")
                    .and_then(|v| v.as_str())
                    .map(|s| s.to_string());
                continue;
            }

            // 创建成功：提取 jwt 与账户 ID
            if let Some(jwt) = data.get("token").and_then(|v| v.as_str()) {
                if jwt.is_empty() {
                    return Err("mail-td: 返回的 token 为空".into());
                }
                let id = data
                    .get("id")
                    .and_then(|v| v.as_str())
                    .unwrap_or("")
                    .to_string();
                if id.is_empty() {
                    return Err("mail-td: 返回的账户 ID 为空".into());
                }
                let email = data
                    .get("address")
                    .and_then(|v| v.as_str())
                    .filter(|s| !s.is_empty() && s.contains('@'))
                    .unwrap_or(&address)
                    .to_string();

                let token = serde_json::to_string(&MailTdToken {
                    jwt: jwt.to_string(),
                    id,
                })
                .map_err(|e| format!("mail-td: 序列化 token 失败: {}", e))?;

                return Ok(EmailInfo {
                    channel: Channel::MailTd,
                    email,
                    token: Some(token),
                    expires_at: None,
                    created_at: None,
                });
            }

            return Err(format!("mail-td: 创建账户失败, HTTP {}", status));
        }

        Err("mail-td: Proof-of-Work 重试次数耗尽".into())
    })
}

/// 获取邮件列表
/// GET /accounts/{id}/messages?page=1，Header: Authorization: Bearer {jwt}
pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let em = email.trim();
    if em.is_empty() {
        return Err("mail-td: 邮箱地址为空".into());
    }

    let tk = token.trim();
    if tk.is_empty() {
        return Err("mail-td: token 为空".into());
    }

    let parsed: MailTdToken =
        serde_json::from_str(tk).map_err(|_| "mail-td: token 反序列化失败".to_string())?;
    if parsed.jwt.is_empty() || parsed.id.is_empty() {
        return Err("mail-td: token 内容无效".into());
    }

    block_on(async {
        let url = format!("{}/accounts/{}/messages?page=1", API_BASE, parsed.id);
        let resp = headers(http_client().get(&url))
            .header("Authorization", format!("Bearer {}", parsed.jwt))
            .send()
            .await
            .map_err(|e| format!("mail-td: 获取邮件列表请求失败: {}", e))?;

        if resp.status().as_u16() == 404 {
            return Ok(Vec::new());
        }

        if !resp.status().is_success() {
            return Err(format!("mail-td: 获取邮件列表返回 HTTP {}", resp.status()));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("mail-td: 解析邮件列表响应失败: {}", e))?;

        // 响应格式: {"messages":[...],"page":1}
        let items = if let Some(arr) = data.get("messages").and_then(|v| v.as_array()) {
            arr.clone()
        } else if let Some(arr) = data.as_array() {
            arr.clone()
        } else {
            return Ok(Vec::new());
        };

        let mut result = Vec::with_capacity(items.len());
        for msg in &items {
            // from 为嵌套对象 {"address":"","name":""}，提取其中的地址字符串
            let from_addr = msg
                .get("from")
                .and_then(|f| f.get("address"))
                .and_then(|v| v.as_str())
                .unwrap_or("");

            let raw = json!({
                "id": msg.get("id").unwrap_or(&Value::Null),
                "from": from_addr,
                "to": em,
                "subject": msg.get("subject").unwrap_or(&Value::Null),
                "text": msg.get("text").unwrap_or(&Value::Null),
                "html": msg.get("html").unwrap_or(&Value::Null),
                "date": msg.get("created_at").unwrap_or(&Value::Null),
                "seen": msg.get("seen").unwrap_or(&Value::Bool(false)),
                "attachments": msg.get("attachments").unwrap_or(&Value::Array(vec![])),
            });
            result.push(normalize_email(&raw, em));
        }

        Ok(result)
    })
}
