/*!
 * mail.chatgpt.org.uk 渠道实现
 */

use std::sync::LazyLock;

use regex::Regex;
use serde::{Deserialize, Serialize};
use serde_json::Value;

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};

const BASE_URL: &str = "https://mail.chatgpt.org.uk/api";
const HOME_URL: &str = "https://mail.chatgpt.org.uk/";

static BROWSER_AUTH_RE: LazyLock<Regex> = LazyLock::new(|| {
    Regex::new(r"__BROWSER_AUTH\s*=\s*(\{[\s\S]*?\})\s*;").expect("browser auth regex")
});

fn is_401_error(message: &str) -> bool {
    message.contains("401")
}

fn is_retry_home_error(message: &str) -> bool {
    let m = message.to_lowercase();
    is_401_error(message) || m.contains("extract") || m.contains("gm_sid")
}

#[derive(Debug, Serialize, Deserialize)]
struct ChatgptPackedToken {
    #[serde(rename = "gmSid")]
    gm_sid: String,
    inbox: String,
}

fn extract_browser_auth(html: &str) -> String {
    let Some(caps) = BROWSER_AUTH_RE.captures(html) else {
        return String::new();
    };
    let Some(json_s) = caps.get(1) else {
        return String::new();
    };
    let Ok(v) = serde_json::from_str::<Value>(json_s.as_str()) else {
        return String::new();
    };
    v.get("token")
        .and_then(|t| t.as_str())
        .unwrap_or("")
        .to_string()
}

fn fetch_home_session_once() -> Result<(String, String), String> {
    block_on(async {
        let resp = http_client()
            .get(HOME_URL)
            .header("User-Agent", get_current_ua())
            .header("Accept", "*/*")
            .header("Referer", HOME_URL)
            .header("Origin", "https://mail.chatgpt.org.uk")
            .header("DNT", "1")
            .send()
            .await
            .map_err(|e| format!("chatgpt-org-uk home request failed: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("chatgpt-org-uk home failed: {}", resp.status()));
        }

        let gm_sid = resp
            .headers()
            .get_all("set-cookie")
            .iter()
            .filter_map(|v| v.to_str().ok())
            .find_map(|s| {
                let part = s.split(';').next().unwrap_or("");
                if part.starts_with("gm_sid=") {
                    Some(part["gm_sid=".len()..].to_string())
                } else {
                    None
                }
            })
            .ok_or("Failed to extract gm_sid cookie")?;

        let html = resp.text().await.map_err(|e| format!("read body: {}", e))?;
        let browser = extract_browser_auth(&html);
        if browser.is_empty() {
            return Err("Failed to extract __BROWSER_AUTH from homepage".into());
        }

        Ok((gm_sid, browser))
    })
}

fn fetch_home_session() -> Result<(String, String), String> {
    match fetch_home_session_once() {
        Ok(v) => Ok(v),
        Err(err) => {
            if is_retry_home_error(&err) {
                fetch_home_session_once()
            } else {
                Err(err)
            }
        }
    }
}

fn fetch_inbox_token_once(email: &str, gm_sid: &str) -> Result<String, String> {
    let email = email.to_string();
    let gm_sid = gm_sid.to_string();
    block_on(async {
        let resp = http_client()
            .post(format!("{}/inbox-token", BASE_URL))
            .header("User-Agent", get_current_ua())
            .header("Accept", "*/*")
            .header("Referer", HOME_URL)
            .header("Origin", "https://mail.chatgpt.org.uk")
            .header("DNT", "1")
            .header("Content-Type", "application/json")
            .header("Cookie", format!("gm_sid={}", gm_sid))
            .json(&serde_json::json!({"email": email}))
            .send()
            .await
            .map_err(|e| format!("chatgpt-org-uk inbox-token request failed: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("chatgpt-org-uk inbox-token failed: {}", resp.status()));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("parse failed: {}", e))?;
        let token = data["auth"]["token"].as_str().unwrap_or("");
        if token.is_empty() {
            return Err("Failed to get inbox token".into());
        }

        Ok(token.to_string())
    })
}

fn fetch_inbox_token_with_retry(email: &str, gm_sid: &str) -> Result<String, String> {
    match fetch_inbox_token_once(email, gm_sid) {
        Ok(t) => Ok(t),
        Err(err) => {
            if is_401_error(&err) {
                let (sid, _) = fetch_home_session()?;
                return fetch_inbox_token_once(email, &sid);
            }
            Err(err)
        }
    }
}

fn refresh_inbox_pair(email: &str) -> Result<(String, String), String> {
    let (gm_sid, _) = fetch_home_session()?;
    let inbox = fetch_inbox_token_with_retry(email, &gm_sid)?;
    Ok((inbox, gm_sid))
}

fn parse_packed_token(packed: &str) -> (String, String) {
    let t = packed.trim();
    if t.starts_with('{') {
        if let Ok(p) = serde_json::from_str::<ChatgptPackedToken>(t) {
            if !p.gm_sid.is_empty() && !p.inbox.is_empty() {
                return (p.gm_sid, p.inbox);
            }
        }
    }
    (String::new(), packed.to_string())
}

pub fn generate_email() -> Result<EmailInfo, String> {
    let (gm_sid, browser_token) = fetch_home_session()?;

    let (email, inbox_from_auth) = block_on(async {
        let resp = http_client()
            .get(format!("{}/generate-email", BASE_URL))
            .header("User-Agent", get_current_ua())
            .header("Accept", "*/*")
            .header("Referer", HOME_URL)
            .header("Origin", "https://mail.chatgpt.org.uk")
            .header("DNT", "1")
            .header("Cookie", format!("gm_sid={}", gm_sid))
            .header("X-Inbox-Token", &browser_token)
            .send()
            .await
            .map_err(|e| format!("chatgpt-org-uk request failed: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("chatgpt-org-uk generate failed: {}", resp.status()));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("parse failed: {}", e))?;
        if !data["success"].as_bool().unwrap_or(false) {
            return Err("Failed to generate email".into());
        }

        let email = data["data"]["email"].as_str().unwrap_or("").to_string();
        if email.is_empty() {
            return Err("Failed to generate email".into());
        }

        let from_auth = data["auth"]["token"].as_str().unwrap_or("").to_string();
        Ok((email, from_auth))
    })?;

    let inbox_jwt = if !inbox_from_auth.is_empty() {
        inbox_from_auth
    } else {
        fetch_inbox_token_once(&email, &gm_sid)?
    };

    let packed = serde_json::to_string(&ChatgptPackedToken {
        gm_sid: gm_sid.clone(),
        inbox: inbox_jwt,
    })
    .map_err(|e| e.to_string())?;

    Ok(EmailInfo {
        channel: Channel::ChatgptOrgUk,
        email,
        token: Some(packed),
        expires_at: None,
        created_at: None,
    })
}

fn fetch_emails(inbox: &str, email: &str, gm_sid: &str) -> Result<Vec<Email>, String> {
    let inbox = inbox.to_string();
    let email = email.to_string();
    let gm_sid = gm_sid.to_string();
    block_on(async {
        let resp = http_client()
            .get(format!(
                "{}/emails?email={}",
                BASE_URL,
                urlencoding::encode(&email)
            ))
            .header("User-Agent", get_current_ua())
            .header("Accept", "*/*")
            .header("Referer", HOME_URL)
            .header("Origin", "https://mail.chatgpt.org.uk")
            .header("DNT", "1")
            .header("Cookie", format!("gm_sid={}", gm_sid))
            .header("x-inbox-token", inbox)
            .send()
            .await
            .map_err(|e| format!("chatgpt-org-uk request failed: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("chatgpt-org-uk get emails failed: {}", resp.status()));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("parse failed: {}", e))?;
        if !data["success"].as_bool().unwrap_or(false) {
            return Err("Failed to get emails".into());
        }

        Ok(data["data"]["emails"]
            .as_array()
            .map(|arr| {
                arr.iter()
                    .map(|raw| normalize_email(raw, &email))
                    .collect()
            })
            .unwrap_or_default())
    })
}

pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    if token.is_empty() {
        return Err("token is required for chatgpt-org-uk".into());
    }

    let (mut gm_sid, inbox) = parse_packed_token(token);
    if gm_sid.is_empty() {
        gm_sid = fetch_home_session()?.0;
    }

    match fetch_emails(&inbox, email, &gm_sid) {
        Ok(emails) => Ok(emails),
        Err(err) => {
            if is_401_error(&err) {
                let (refreshed, sid) = refresh_inbox_pair(email)?;
                return fetch_emails(&refreshed, email, &sid);
            }
            Err(err)
        }
    }
}
