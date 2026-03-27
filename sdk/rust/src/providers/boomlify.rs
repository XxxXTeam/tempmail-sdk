/*!
 * Boomlify — https://v1.boomlify.com
 */

use rand::Rng;
use serde_json::Value;
use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};

const BASE: &str = "https://v1.boomlify.com";

fn random_local(n: usize) -> String {
    let chars: Vec<char> = "abcdefghijklmnopqrstuvwxyz0123456789".chars().collect();
    let mut rng = rand::thread_rng();
    (0..n).map(|_| chars[rng.gen_range(0..chars.len())]).collect()
}

fn json_active(v: &Value) -> bool {
    match v {
        Value::Number(n) => n.as_i64() == Some(1) || n.as_f64() == Some(1.0),
        Value::Bool(b) => *b,
        _ => false,
    }
}

fn fetch_domains() -> Result<Vec<String>, String> {
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
            if dom.is_empty() {
                continue;
            }
            let active = d.get("is_active").map(json_active).unwrap_or(false)
                || d.get("isActive").map(json_active).unwrap_or(false);
            if active {
                out.push(dom.to_string());
            }
        }
        Ok(out)
    })
}

pub fn generate_email() -> Result<EmailInfo, String> {
    let domains = fetch_domains()?;
    if domains.is_empty() {
        return Err("boomlify: no domains".into());
    }
    let mut rng = rand::thread_rng();
    let domain = &domains[rng.gen_range(0..domains.len())];
    let local = random_local(8);
    Ok(EmailInfo {
        channel: Channel::Boomlify,
        email: format!("{}@{}", local, domain),
        token: None,
        expires_at: None,
        created_at: None,
    })
}

pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let enc = urlencoding::encode(email);
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
