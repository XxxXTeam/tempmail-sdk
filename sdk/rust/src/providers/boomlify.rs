/*!
 * Boomlify — https://v1.boomlify.com
 * 须先 POST /emails/public/create 登记收件箱，再用 inboxId@域名 收信。
 */

use std::sync::OnceLock;

use rand::Rng;
use regex::Regex;
use serde_json::{json, Value};
use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};

const BASE: &str = "https://v1.boomlify.com";

static INBOX_UUID_RE: OnceLock<Regex> = OnceLock::new();

fn json_active(v: &Value) -> bool {
    match v {
        Value::Number(n) => n.as_i64() == Some(1) || n.as_f64() == Some(1.0),
        Value::Bool(b) => *b,
        _ => false,
    }
}

fn fetch_domains() -> Result<Vec<(String, String)>, String> {
    block_on(async {
        let resp = http_client()
            .get(format!("{}/domains/public", BASE))
            .header("Accept", "application/json, text/plain, */*")
            .header("Accept-Language", "zh")
            .header("Origin", "https://boomlify.com")
            .header("Referer", "https://boomlify.com/")
            .header("User-Agent", get_current_ua())
            .header("sec-ch-ua", r#""Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146""#)
            .header("sec-ch-ua-mobile", "?0")
            .header("sec-ch-ua-platform", r#""Windows""#)
            .header("sec-fetch-dest", "empty")
            .header("sec-fetch-mode", "cors")
            .header("sec-fetch-site", "same-site")
            .header("x-user-language", "zh")
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("boomlify domains {}", resp.status()));
        }
        let arr: Vec<Value> = resp.json().await.map_err(|e| e.to_string())?;
        let mut out = Vec::new();
        for d in arr {
            let dom = d.get("domain").and_then(|x| x.as_str()).unwrap_or("");
            let id = d.get("id").and_then(|x| x.as_str()).unwrap_or("");
            if dom.is_empty() || id.is_empty() {
                continue;
            }
            let active = d.get("is_active").map(json_active).unwrap_or(false)
                || d.get("isActive").map(json_active).unwrap_or(false);
            if active {
                out.push((id.to_string(), dom.to_string()));
            }
        }
        Ok(out)
    })
}

fn create_public_inbox(domain_id: &str) -> Result<String, String> {
    block_on(async {
        let body = json!({ "domainId": domain_id }).to_string();
        let resp = http_client()
            .post(format!("{}/emails/public/create", BASE))
            .header("Accept", "application/json, text/plain, */*")
            .header("Content-Type", "application/json")
            .header("Accept-Language", "zh")
            .header("Origin", "https://boomlify.com")
            .header("Referer", "https://boomlify.com/")
            .header("User-Agent", get_current_ua())
            .header("sec-ch-ua", r#""Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146""#)
            .header("sec-ch-ua-mobile", "?0")
            .header("sec-ch-ua-platform", r#""Windows""#)
            .header("sec-fetch-dest", "empty")
            .header("sec-fetch-mode", "cors")
            .header("sec-fetch-site", "same-site")
            .header("x-user-language", "zh")
            .body(body)
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("boomlify public create {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        if let Some(err) = data.get("error").and_then(|x| x.as_str()) {
            let mut msg = err.to_string();
            if let Some(m) = data.get("message").and_then(|x| x.as_str()) {
                msg.push_str(" — ");
                msg.push_str(m);
            }
            if data.get("captchaRequired").and_then(|x| x.as_bool()) == Some(true) {
                msg.push_str("（服务端限流/需验证码，请稍后重试）");
            }
            return Err(format!("boomlify: {msg}"));
        }
        let id = data
            .get("id")
            .and_then(|x| x.as_str())
            .unwrap_or("")
            .to_string();
        if id.is_empty() {
            return Err("boomlify: public create returned no inbox id".into());
        }
        Ok(id)
    })
}

fn inbox_path_segment(email: &str) -> String {
    let re = INBOX_UUID_RE.get_or_init(|| {
        Regex::new(r"(?i)^[0-9a-f]{8}-[0-9a-f]{4}-[1-5][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$")
            .expect("inbox uuid regex")
    });
    let local = email
        .rsplit_once('@')
        .map(|(a, _)| a)
        .unwrap_or(email);
    if re.is_match(local.trim()) {
        local.trim().to_string()
    } else {
        email.to_string()
    }
}

pub fn generate_email() -> Result<EmailInfo, String> {
    let domains = fetch_domains()?;
    if domains.is_empty() {
        return Err("boomlify: no domains".into());
    }
    let mut rng = rand::thread_rng();
    let (did, dom) = &domains[rng.gen_range(0..domains.len())];
    let box_id = create_public_inbox(did)?;
    let email = format!("{}@{}", box_id, dom);
    Ok(EmailInfo {
        channel: Channel::Boomlify,
        email,
        token: Some(box_id),
        expires_at: None,
        created_at: None,
    })
}

pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let seg = inbox_path_segment(email);
    let enc = urlencoding::encode(&seg);
    let url = format!("{}/emails/public/{}", BASE, enc);
    block_on(async {
        let resp = http_client()
            .get(&url)
            .header("Accept", "application/json, text/plain, */*")
            .header("Accept-Language", "zh")
            .header("Origin", "https://boomlify.com")
            .header("Referer", "https://boomlify.com/")
            .header("User-Agent", get_current_ua())
            .header("sec-ch-ua", r#""Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146""#)
            .header("sec-ch-ua-mobile", "?0")
            .header("sec-ch-ua-platform", r#""Windows""#)
            .header("sec-fetch-dest", "empty")
            .header("sec-fetch-mode", "cors")
            .header("sec-fetch-site", "same-site")
            .header("x-user-language", "zh")
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("boomlify emails {}", resp.status()));
        }
        let rows: Vec<Value> = resp.json().await.map_err(|e| e.to_string())?;
        let mut out = Vec::new();
        for raw in rows {
            let from = raw["from_email"]
                .as_str()
                .or_else(|| raw["from_name"].as_str())
                .unwrap_or("");
            let flat = serde_json::json!({
                "id": raw["id"],
                "from": from,
                "to": email,
                "subject": raw["subject"],
                "text": raw["body_text"],
                "html": raw["body_html"],
                "received_at": raw["received_at"],
                "isRead": false,
            });
            out.push(normalize_email(&flat, email));
        }
        Ok(out)
    })
}
