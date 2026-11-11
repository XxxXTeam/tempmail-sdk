/*!
 * Shadowmail 渠道实现（shadowmail.win）
 * 契约（前端 JS 抓取 + curl 实测）：
 * - 注册：POST /api/register {"email":"<随机>@gmail.com","password":"Abcd1234!"}。
 * - 登录：POST /api/login（同 body）→ Set-Cookie: sessionId=<uuid>。
 * - 建箱：POST /api/new-address → {"address":"<id>@shadowmail.win","id":<id>}。
 * - 读信：POST /api/get-emails {"address":"<地址>"} → {"mails":[...]}。
 *
 * 会话以显式 Cookie 头逐请求携带；凭据串持久化注册邮箱/密码/会话 id，
 * 会话过期（401/404）后自动重新登录重试一次。
 */

use crate::config::{block_on, get_current_ua, http_client_no_cookie_jar};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use rand::RngExt;
use serde_json::{json, Value};
use wreq::Client;

const BASE_URL: &str = "https://shadowmail.win";
/// 固定注册密码（平台无自选密码入口）
const PASSWORD: &str = "Abcd1234!";
/// 平台唯一收信域
const DOMAIN: &str = "shadowmail.win";
/// 凭据串前缀
const TOKEN_PREFIX: &str = "shadowmail|";

/// 生成随机注册邮箱前缀（sdk+8 位小写字母）
fn random_account() -> String {
    const CHARS: &[u8] = b"abcdefghijklmnopqrstuvwxyz";
    let mut rng = rand::rng();
    let body: String = (0..8)
        .map(|_| {
            let idx = rng.random_range(0..CHARS.len());
            CHARS[idx] as char
        })
        .collect();
    format!("sdk{body}")
}

/// 从登录响应提取 sessionId 值（纯 uuid，不含键名）
fn session_from_response(resp: &wreq::Response) -> String {
    for cookie in resp.cookies() {
        if cookie.name() == "sessionId" {
            return cookie.value().to_string();
        }
    }
    String::new()
}

/// 注册或登录（POST /api/register、/api/login），返回会话 id（纯 uuid）
async fn register_or_login(
    client: &Client,
    account: &str,
    password: &str,
    is_login: bool,
) -> Result<String, String> {
    let path = if is_login {
        "/api/login"
    } else {
        "/api/register"
    };
    let resp = client
        .post(format!("{BASE_URL}{path}"))
        .header("Content-Type", "application/json")
        .header("Accept", "application/json")
        .header("User-Agent", get_current_ua())
        .body(json!({"email": account, "password": password}).to_string())
        .send()
        .await
        .map_err(|e| format!("shadowmail {path}: 请求失败: {e}"))?;
    let status = resp.status();
    /* 先收集会话 id（sessionId Cookie），再消费响应体 */
    let session = session_from_response(&resp);
    let body = resp
        .text()
        .await
        .map_err(|e| format!("shadowmail {path}: 读取失败: {e}"))?;
    if !status.is_success() {
        return Err(format!("shadowmail {path}: http {status}"));
    }
    let data: Value =
        serde_json::from_str(&body).map_err(|e| format!("shadowmail {path}: 解析失败: {e}"))?;
    let message = data
        .get("message")
        .and_then(|v| v.as_str())
        .unwrap_or("")
        .to_string();
    if is_login && message != "Successfull Login" {
        return Err(format!("shadowmail login: {message}"));
    }
    /* 重复注册（幂等）：消息为 Email already in use 时视为账号已存在 */
    if !is_login && message != "Successfully Registered" && message != "Email already in use" {
        return Err(format!("shadowmail register: {message}"));
    }
    if is_login && session.is_empty() {
        return Err("shadowmail login: 未下发 sessionId Cookie".into());
    }
    Ok(session)
}

/// 创建 shadowmail 临时邮箱
/// token 凭据串格式："shadowmail|<account>|<password>|<sessionId>"
pub fn generate_email() -> Result<EmailInfo, String> {
    let client = http_client_no_cookie_jar();
    /* 关键：平台会话回源校验 IP，且 Set-Cookie sessionId 原样提取；
     * 显式将 login 下发的 sessionId 作为 Cookie 头串透传，杜绝作废会话 */
    let account = format!("{}@gmail.com", random_account());

    block_on(async {
        /* 1) 注册（幂等：已存在同名账号则跳过） */
        register_or_login(&client, &account, PASSWORD, false).await?;
        /* 2) 登录取得 sessionId（纯 uuid，不合成键值对） */
        let session = register_or_login(&client, &account, PASSWORD, true).await?;
        /* 3) 创建地址（每账号 12 槽）：SDK 层显式 Cookie 头传 sessionId */
        let resp = client
            .post(format!("{BASE_URL}/api/new-address"))
            .header("Content-Type", "application/json")
            .header("Accept", "application/json")
            .header("User-Agent", get_current_ua())
            .header("Cookie", format!("sessionId={session}"))
            .body("{}")
            .send()
            .await
            .map_err(|e| format!("shadowmail new-address: 请求失败: {e}"))?;
        let status = resp.status();
        let body = resp
            .text()
            .await
            .map_err(|e| format!("shadowmail new-address: 读取失败: {e}"))?;
        if !status.is_success() {
            return Err(format!("shadowmail new-address: http {status}"));
        }
        let data: Value = serde_json::from_str(&body)
            .map_err(|e| format!("shadowmail new-address: 解析失败: {e}"))?;
        let address = data
            .get("address")
            .and_then(|v| v.as_str())
            .unwrap_or("")
            .to_lowercase()
            .trim()
            .to_string();
        if address.is_empty() || !address.ends_with(&format!("@{DOMAIN}")) {
            return Err("shadowmail new-address: 响应缺少有效地址".into());
        }

        /* Token 持久化：account|password|sessionId */
        let token = format!("{TOKEN_PREFIX}{account}|{PASSWORD}|{session}");
        Ok(EmailInfo {
            channel: Channel::Shadowmail,
            email: address,
            token: Some(token),
            expires_at: None,
            created_at: None,
        })
    })
}

/// 解析凭据串为 account/password/sessionId 三元组
fn parse_token(token: &str) -> Result<(String, String, String), String> {
    let cred = token
        .strip_prefix(TOKEN_PREFIX)
        .ok_or("shadowmail: token 格式错误")?;
    let parts: Vec<&str> = cred.split('|').collect();
    if parts.len() != 3 {
        return Err("shadowmail: token 字段缺失".into());
    }
    let (account, password, session) = (parts[0], parts[1], parts[2]);
    if account.is_empty() || password.is_empty() || session.is_empty() {
        return Err("shadowmail: token 凭据字段为空".into());
    }
    Ok((
        account.to_string(),
        password.to_string(),
        session.to_string(),
    ))
}

/// POST /api/get-emails 拉取收件箱（显式 Cookie），返回 (状态码, 响应体)
async fn fetch_emails_raw(
    client: &Client,
    session: &str,
    address: &str,
) -> Result<(wreq::StatusCode, String), String> {
    let resp = client
        .post(format!("{BASE_URL}/api/get-emails"))
        .header("Content-Type", "application/json")
        .header("Accept", "application/json")
        .header("User-Agent", get_current_ua())
        .header("Cookie", format!("sessionId={session}"))
        .body(json!({"address": address}).to_string())
        .send()
        .await
        .map_err(|e| format!("shadowmail get-emails: 请求失败: {e}"))?;
    let status = resp.status();
    let body = resp
        .text()
        .await
        .map_err(|e| format!("shadowmail get-emails: 读取失败: {e}"))?;
    Ok((status, body))
}

/// 读取 shadowmail 收件箱
/// 会话失效（401/404）时以凭据内账号密码重新登录换新 sessionId 重试一次。
pub fn get_emails(email: &str, token: &str) -> Result<Vec<Email>, String> {
    let em = email.trim();
    let (account, password, initial_session) = parse_token(token)?;
    let client = http_client_no_cookie_jar();

    block_on(async {
        let mut session = initial_session;

        let (mut status, mut body) = fetch_emails_raw(&client, &session, em).await?;
        /* sessionId 最长 1 小时（Max-Age 3600），过期后重登录重试一次 */
        if status == 401 || status == 404 {
            if let Ok(new_session) = register_or_login(&client, &account, &password, true).await {
                session = new_session;
                let (s, b) = fetch_emails_raw(&client, &session, em).await?;
                status = s;
                body = b;
            }
        }
        if !status.is_success() {
            return Err(format!("shadowmail get-emails: http {status}"));
        }
        let data: Value = serde_json::from_str(&body)
            .map_err(|e| format!("shadowmail get-emails: 解析失败: {e}"))?;
        let message = data
            .get("message")
            .and_then(|v| v.as_str())
            .unwrap_or("")
            .to_string();
        if message != "Emails read" {
            return Err(format!("shadowmail get-emails: {message}"));
        }
        let mails = data
            .get("mails")
            .and_then(|v| v.as_array())
            .cloned()
            .unwrap_or_default();

        let out: Vec<Email> = mails
            .iter()
            .map(|m| {
                let mut flat = m.clone();
                if let Some(obj) = flat.as_object_mut() {
                    obj.insert(
                        "from".to_string(),
                        m.get("sender").cloned().unwrap_or(Value::Null),
                    );
                    obj.insert("to".to_string(), json!(em));
                    obj.insert(
                        "date".to_string(),
                        m.get("created_at").cloned().unwrap_or(Value::Null),
                    );
                    /* 平台无 text/html 区分，body 为正文（默认按纯文本处理） */
                    obj.insert(
                        "text".to_string(),
                        m.get("body").cloned().unwrap_or(Value::Null),
                    );
                }
                normalize_email(&flat, em)
            })
            .collect();
        Ok(out)
    })
}
