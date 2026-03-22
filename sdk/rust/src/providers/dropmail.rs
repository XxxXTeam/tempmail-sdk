/*!
 * dropmail.me 渠道实现 (GraphQL)
 * 自动 af_ 令牌：/api/token/generate，将过期时 /api/token/renew
 */

use std::sync::Mutex;
use std::time::{Duration, Instant};

use serde_json::{json, Value};
use crate::types::{Channel, EmailInfo, Email};
use crate::normalize::normalize_email;
use crate::config::{http_client, block_on, get_current_ua, get_config};

const TOKEN_GENERATE_URL: &str = "https://dropmail.me/api/token/generate";
const TOKEN_RENEW_URL: &str = "https://dropmail.me/api/token/renew";
const AUTO_TOKEN_CACHE: Duration = Duration::from_secs(50 * 60);
const RENEW_BEFORE: Duration = Duration::from_secs(10 * 60);

struct AfCache {
    token: String,
    valid_until: Instant,
}

static DM_AF_CACHE: Mutex<Option<AfCache>> = Mutex::new(None);

fn explicit_dropmail_auth_token() -> Option<String> {
    let cfg = get_config();
    if let Some(ref t) = cfg.dropmail_auth_token {
        let s = t.trim();
        if !s.is_empty() {
            return Some(s.to_string());
        }
    }
    std::env::var("DROPMAIL_AUTH_TOKEN").ok().map(|s| s.trim().to_string()).filter(|s| !s.is_empty())
        .or_else(|| std::env::var("DROPMAIL_API_TOKEN").ok().map(|s| s.trim().to_string()).filter(|s| !s.is_empty()))
}

fn dropmail_auto_token_disabled() -> bool {
    if get_config().dropmail_disable_auto_token {
        return true;
    }
    std::env::var("DROPMAIL_NO_AUTO_TOKEN").ok()
        .map(|v| {
            let v = v.trim().to_ascii_lowercase();
            v == "1" || v == "true" || v == "yes"
        })
        .unwrap_or(false)
}

fn dropmail_renew_lifetime() -> String {
    get_config().dropmail_renew_lifetime.as_ref()
        .map(|s| s.trim().to_string())
        .filter(|s| !s.is_empty())
        .or_else(|| std::env::var("DROPMAIL_RENEW_LIFETIME").ok().filter(|s| !s.trim().is_empty()))
        .unwrap_or_else(|| "1d".to_string())
}

fn cache_duration_for_lifetime(lifetime: &str) -> Duration {
    let s = lifetime.trim().to_ascii_lowercase();
    match s.as_str() {
        "1h" => Duration::from_secs(50 * 60),
        "1d" => Duration::from_secs(23 * 60 * 60),
        _ if s.ends_with('d') => {
            if let Ok(days) = s.trim_end_matches('d').parse::<u64>() {
                return Duration::from_secs((days.saturating_sub(1)) * 24 * 60 * 60);
            }
            AUTO_TOKEN_CACHE
        }
        _ => AUTO_TOKEN_CACHE,
    }
}

async fn post_json_token_api(url: &str, body: Value) -> Result<Value, String> {
    let resp = http_client()
        .post(url)
        .header("Accept", "application/json")
        .header("Content-Type", "application/json")
        .header("Origin", "https://dropmail.me")
        .header("Referer", "https://dropmail.me/api/")
        .header("User-Agent", get_current_ua())
        .json(&body)
        .send().await.map_err(|e| format!("dropmail token request failed: {}", e))?;

    if !resp.status().is_success() {
        return Err(format!("dropmail token HTTP {}", resp.status()));
    }
    resp.json().await.map_err(|e| format!("dropmail token parse: {}", e))
}

async fn fetch_af_generate() -> Result<String, String> {
    let v = post_json_token_api(TOKEN_GENERATE_URL, json!({"type": "af", "lifetime": "1h"})).await?;
    let tok = v["token"].as_str().unwrap_or("").trim();
    if tok.is_empty() || !tok.starts_with("af_") {
        return Err(v["error"].as_str().unwrap_or("DropMail token/generate 未返回有效 af_ 令牌").to_string());
    }
    Ok(tok.to_string())
}

async fn fetch_af_renew(current: &str, lifetime: &str) -> Result<String, String> {
    let v = post_json_token_api(TOKEN_RENEW_URL, json!({"token": current, "lifetime": lifetime})).await?;
    let tok = v["token"].as_str().unwrap_or("").trim();
    if tok.is_empty() || !tok.starts_with("af_") {
        return Err(v["error"].as_str().unwrap_or("DropMail token/renew 未返回有效 af_ 令牌").to_string());
    }
    Ok(tok.to_string())
}

fn resolve_dropmail_auth_token() -> Result<String, String> {
    if let Some(t) = explicit_dropmail_auth_token() {
        return Ok(t);
    }
    if dropmail_auto_token_disabled() {
        return Err("DropMail 已禁用自动令牌：请设置 DROPMAIL_AUTH_TOKEN 或 config.dropmail_auth_token，见 https://dropmail.me/api/".to_string());
    }

    let now = Instant::now();
    let renew_life = dropmail_renew_lifetime();

    let old_for_renew = {
        let guard = DM_AF_CACHE.lock().map_err(|_| "DropMail 内部锁错误".to_string())?;
        if let Some(ref c) = *guard {
            if now < c.valid_until - RENEW_BEFORE {
                return Ok(c.token.clone());
            }
        }
        guard.as_ref().map(|c| c.token.clone())
    };

    let tok: (String, Duration) = block_on(async move {
        if let Some(ref t) = old_for_renew {
            if let Ok(x) = fetch_af_renew(t, &renew_life).await {
                return Ok::<(String, Duration), String>((x, cache_duration_for_lifetime(&renew_life)));
            }
        }
        let g = fetch_af_generate().await?;
        Ok::<(String, Duration), String>((g, AUTO_TOKEN_CACHE))
    })?;

    let mut guard = DM_AF_CACHE.lock().map_err(|_| "DropMail 内部锁错误".to_string())?;
    *guard = Some(AfCache {
        token: tok.0.clone(),
        valid_until: Instant::now() + tok.1,
    });
    Ok(tok.0)
}

fn graphql_url(af: &str) -> String {
    format!("https://dropmail.me/api/graphql/{}", urlencoding::encode(af))
}

fn graphql_request(query: &str, variables: Option<&Value>) -> Result<Value, String> {
    let af = resolve_dropmail_auth_token()?;
    let url = graphql_url(&af);
    let query = query.to_string();
    let vars = variables.cloned();
    block_on(async {
        let mut params = vec![("query", query.clone())];
        if let Some(ref v) = vars {
            params.push(("variables", v.to_string()));
        }

        let resp = http_client()
            .post(&url)
            .header("User-Agent", get_current_ua())
            .form(&params)
            .send().await.map_err(|e| format!("dropmail request failed: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("dropmail request failed: {}", resp.status()));
        }

        let data: Value = resp.json().await.map_err(|e| format!("parse failed: {}", e))?;
        if let Some(errors) = data["errors"].as_array() {
            if !errors.is_empty() {
                return Err(format!("GraphQL error: {}", errors[0]["message"].as_str().unwrap_or("unknown")));
            }
        }

        Ok(data["data"].clone())
    })
}

pub fn generate_email() -> Result<EmailInfo, String> {
    let data = graphql_request(
        "mutation {introduceSession {id, expiresAt, addresses{id, address}}}",
        None,
    )?;

    let session = &data["introduceSession"];
    let addr = session["addresses"][0]["address"].as_str()
        .ok_or("Failed to generate email")?;

    Ok(EmailInfo {
        channel: Channel::Dropmail,
        email: addr.to_string(),
        token: session["id"].as_str().map(|s| s.to_string()),
        expires_at: session["expiresAt"].as_i64(),
        created_at: None,
    })
}

pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let query = "query ($id: ID!) { session(id:$id) { mails { id, fromAddr, toAddr, receivedAt, text, headerSubject, html } } }";
    let vars = serde_json::json!({"id": token});
    let data = graphql_request(query, Some(&vars))?;

    let mails = data["session"]["mails"].as_array().cloned().unwrap_or_default();
    Ok(mails.iter().map(|m| {
        let flat = serde_json::json!({
            "id": m["id"], "from": m["fromAddr"], "to": m["toAddr"].as_str().unwrap_or(email),
            "subject": m["headerSubject"], "text": m["text"], "html": m["html"],
            "received_at": m["receivedAt"],
        });
        normalize_email(&flat, email)
    }).collect())
}
