/*!
 * mail.zhujump.com 固定域渠道
 * 通过注册账号、登录会话后创建固定域邮箱，再通过邮箱 ID 获取邮件列表。
 */

use base64::Engine;
use rand::Rng;
use serde::{Deserialize, Serialize};
use serde_json::{json, Value};
use wreq::header::HeaderMap;

use crate::config::{block_on, get_current_ua, http_client_no_cookie_jar};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};

const BASE_URL: &str = "https://mail.zhujump.com";
const TOKEN_PREFIX: &str = "zhj1:";

#[derive(Serialize, Deserialize)]
struct ZhujumpToken {
    c: String,
    i: String,
    b: Option<String>,
}

fn random_value(prefix: &str, size: usize) -> String {
    const CHARS: &[u8] = b"abcdefghijklmnopqrstuvwxyz0123456789";
    let mut rng = rand::thread_rng();
    let mut out = String::from(prefix);
    for _ in 0..size {
        out.push(CHARS[rng.gen_range(0..CHARS.len())] as char);
    }
    out
}

fn random_password() -> String {
    format!("Sdk!{}A1", random_value("", 12))
}

fn login_referer(base_url: &str) -> String {
    format!("{}/zh-CN/login", base_url.trim_end_matches('/'))
}

fn json_headers(
    builder: wreq::RequestBuilder,
    base_url: &str,
    cookie: Option<&str>,
) -> wreq::RequestBuilder {
    let builder = builder
        .header("Accept", "application/json")
        .header("Origin", base_url.trim_end_matches('/'))
        .header("Referer", login_referer(base_url))
        .header("User-Agent", get_current_ua());
    if let Some(cookie) = cookie {
        if !cookie.trim().is_empty() {
            return builder.header("Cookie", cookie);
        }
    }
    builder
}

fn parse_cookie_header(hdr: &str) -> std::collections::BTreeMap<String, String> {
    let mut m = std::collections::BTreeMap::new();
    for part in hdr.split(';') {
        let part = part.trim();
        if part.is_empty() {
            continue;
        }
        if let Some(i) = part.find('=') {
            let k = part[..i].trim();
            let v = part[i + 1..].trim();
            if !k.is_empty() {
                m.insert(k.to_string(), v.to_string());
            }
        }
    }
    m
}

fn merge_set_cookies(hdr: &str, headers: &HeaderMap) -> String {
    let mut m = parse_cookie_header(hdr);
    for val in headers.get_all("set-cookie") {
        let Ok(s) = val.to_str() else {
            continue;
        };
        let first = s.split(';').next().unwrap_or("").trim();
        if let Some(i) = first.find('=') {
            let k = first[..i].trim();
            let v = first[i + 1..].trim();
            if !k.is_empty() {
                m.insert(k.to_string(), v.to_string());
            }
        }
    }
    m.into_iter()
        .map(|(k, v)| format!("{}={}", k, v))
        .collect::<Vec<_>>()
        .join("; ")
}

fn encode_token(cookie: &str, email_id: &str, base_url: &str) -> Result<String, String> {
    let raw = serde_json::to_vec(&ZhujumpToken {
        c: cookie.to_string(),
        i: email_id.to_string(),
        b: Some(base_url.trim_end_matches('/').to_string()),
    })
    .map_err(|e| format!("zhujump: token encode: {}", e))?;
    Ok(format!(
        "{}{}",
        TOKEN_PREFIX,
        base64::engine::general_purpose::STANDARD.encode(raw)
    ))
}

fn decode_token(token: &str) -> Result<ZhujumpToken, String> {
    if !token.starts_with(TOKEN_PREFIX) {
        return Err("zhujump: invalid session token".into());
    }
    let raw = base64::engine::general_purpose::STANDARD
        .decode(&token[TOKEN_PREFIX.len()..])
        .map_err(|_| "zhujump: invalid session token".to_string())?;
    let mut parsed: ZhujumpToken =
        serde_json::from_slice(&raw).map_err(|_| "zhujump: invalid session token".to_string())?;
    if parsed.c.trim().is_empty() || parsed.i.trim().is_empty() {
        return Err("zhujump: invalid session token".into());
    }
    if parsed.b.as_deref().unwrap_or("").trim().is_empty() {
        parsed.b = Some(BASE_URL.to_string());
    }
    Ok(parsed)
}

pub fn generate_email(domain: &str, channel: Channel) -> Result<EmailInfo, String> {
    generate_email_for_instance(BASE_URL, domain, channel, None)
}

pub fn generate_email_for_instance(
    base_url: &str,
    domain: &str,
    channel: Channel,
    expiry_time: Option<i64>,
) -> Result<EmailInfo, String> {
    let username = random_value("sdk", 10);
    let password = random_password();
    let local = random_value("sdk", 10);
    let base_url = base_url.trim_end_matches('/').to_string();

    block_on(async {
        let client = http_client_no_cookie_jar();
        let mut cookie = String::new();

        let register_resp = json_headers(
            client.post(format!("{}/api/auth/register", base_url)),
            &base_url,
            None,
        )
        .header("Content-Type", "application/json")
        .body(
            json!({
                "username": username,
                "password": password,
                "turnstileToken": "",
            })
            .to_string(),
        )
        .send()
        .await
        .map_err(|e| format!("zhujump register: {}", e))?;
        if !register_resp.status().is_success() {
            return Err(format!("zhujump register: {}", register_resp.status()));
        }
        cookie = merge_set_cookies(&cookie, register_resp.headers());
        let _ = register_resp.text().await;

        let csrf_resp = json_headers(
            client.get(format!("{}/api/auth/csrf", base_url)),
            &base_url,
            Some(&cookie),
        )
        .send()
        .await
        .map_err(|e| format!("zhujump csrf: {}", e))?;
        if !csrf_resp.status().is_success() {
            return Err(format!("zhujump csrf: {}", csrf_resp.status()));
        }
        cookie = merge_set_cookies(&cookie, csrf_resp.headers());
        let csrf_json: Value = csrf_resp
            .json()
            .await
            .map_err(|e| format!("zhujump csrf: {}", e))?;
        let csrf = csrf_json
            .get("csrfToken")
            .and_then(|v| v.as_str())
            .unwrap_or("")
            .trim()
            .to_string();
        if csrf.is_empty() {
            return Err("zhujump: csrf token missing".into());
        }

        let login_body = format!(
            "username={}&password={}&turnstileToken=&redirect=false&csrfToken={}&callbackUrl={}",
            urlencoding::encode(&username),
            urlencoding::encode(&password),
            urlencoding::encode(&csrf),
            urlencoding::encode(&login_referer(&base_url)),
        );
        let login_resp = json_headers(
            client.post(format!("{}/api/auth/callback/credentials?", base_url)),
            &base_url,
            Some(&cookie),
        )
        .header("Content-Type", "application/x-www-form-urlencoded")
        .header("x-auth-return-redirect", "1")
        .body(login_body)
        .send()
        .await
        .map_err(|e| format!("zhujump login: {}", e))?;
        if !login_resp.status().is_success() {
            return Err(format!("zhujump login: {}", login_resp.status()));
        }
        cookie = merge_set_cookies(&cookie, login_resp.headers());
        let _ = login_resp.text().await;

        let session_resp = json_headers(
            client.get(format!("{}/api/auth/session", base_url)),
            &base_url,
            Some(&cookie),
        )
        .send()
        .await
        .map_err(|e| format!("zhujump session: {}", e))?;
        if !session_resp.status().is_success() {
            return Err(format!("zhujump session: {}", session_resp.status()));
        }
        let session_json: Value = session_resp
            .json()
            .await
            .map_err(|e| format!("zhujump session: {}", e))?;
        let current_username = session_json
            .get("user")
            .and_then(|v| v.get("username"))
            .and_then(|v| v.as_str())
            .unwrap_or("")
            .trim()
            .to_string();
        if current_username != username {
            return Err("zhujump: login verification failed".into());
        }

        let mut generate_body = json!({
            "name": local,
            "domain": domain,
        });
        if let Value::Object(ref mut obj) = generate_body {
            if let Some(expiry_time) = expiry_time {
                obj.insert("expiryTime".to_string(), Value::Number(expiry_time.into()));
            }
        }

        let generate_resp = json_headers(
            client.post(format!("{}/api/emails/generate", base_url)),
            &base_url,
            Some(&cookie),
        )
        .header("Content-Type", "application/json")
        .body(generate_body.to_string())
        .send()
        .await
        .map_err(|e| format!("zhujump generate: {}", e))?;
        if !generate_resp.status().is_success() {
            return Err(format!("zhujump generate: {}", generate_resp.status()));
        }
        let generate_json: Value = generate_resp
            .json()
            .await
            .map_err(|e| format!("zhujump generate: {}", e))?;
        let email = generate_json
            .get("email")
            .and_then(|v| v.as_str())
            .unwrap_or("")
            .trim()
            .to_string();
        let email_id = generate_json
            .get("id")
            .and_then(|v| v.as_str())
            .unwrap_or("")
            .trim()
            .to_string();
        if email.is_empty() || email_id.is_empty() {
            return Err("zhujump: invalid generate response".into());
        }
        let token = encode_token(&cookie, &email_id, &base_url)?;
        Ok(EmailInfo {
            channel,
            email,
            token: Some(token),
            expires_at: None,
            created_at: None,
        })
    })
}

pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let session = decode_token(token)?;
    let recipient = email.trim().to_string();
    if recipient.is_empty() {
        return Err("zhujump: empty email".into());
    }

    block_on(async {
        let base_url = session
            .b
            .as_deref()
            .unwrap_or(BASE_URL)
            .trim_end_matches('/')
            .to_string();
        let resp = json_headers(
            http_client_no_cookie_jar().get(format!(
                "{}/api/emails/{}",
                base_url,
                urlencoding::encode(&session.i)
            )),
            &base_url,
            Some(&session.c),
        )
        .send()
        .await
        .map_err(|e| format!("zhujump get emails: {}", e))?;
        if !resp.status().is_success() {
            return Err(format!("zhujump get emails: {}", resp.status()));
        }
        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("zhujump get emails: {}", e))?;
        let rows = data
            .get("messages")
            .and_then(|v| v.as_array())
            .cloned()
            .unwrap_or_default();

        let mut out = Vec::new();
        for row in rows {
            let mut obj = match row {
                Value::Object(map) => map,
                _ => continue,
            };
            let message_id = obj
                .get("id")
                .and_then(|v| v.as_str())
                .unwrap_or("")
                .trim()
                .to_string();
            let has_body = !obj
                .get("content")
                .and_then(|v| v.as_str())
                .unwrap_or("")
                .trim()
                .is_empty()
                || !obj
                    .get("html")
                    .and_then(|v| v.as_str())
                    .unwrap_or("")
                    .trim()
                    .is_empty();
            if !message_id.is_empty() && !has_body {
                let detail_resp = json_headers(
                    http_client_no_cookie_jar().get(format!(
                        "{}/api/emails/{}/{}",
                        base_url,
                        urlencoding::encode(&session.i),
                        urlencoding::encode(&message_id)
                    )),
                    &base_url,
                    Some(&session.c),
                )
                .send()
                .await;
                if let Ok(detail_resp) = detail_resp {
                    if detail_resp.status().is_success() {
                        if let Ok(Value::Object(mut detail_json)) =
                            detail_resp.json::<Value>().await
                        {
                            if let Some(Value::Object(detail)) = detail_json.remove("message") {
                                for (k, v) in detail {
                                    obj.insert(k, v);
                                }
                            }
                        }
                    }
                }
            }
            let missing_to = obj
                .get("to_address")
                .map(|v| v.is_null() || v.as_str().unwrap_or("").trim().is_empty())
                .unwrap_or(true);
            if missing_to {
                obj.insert("to_address".to_string(), Value::String(recipient.clone()));
            }
            out.push(normalize_email(&Value::Object(obj), &recipient));
        }
        Ok(out)
    })
}
