/*!
 * MinMail — https://minmail.app/api
 */

use rand::Rng;
use serde_json::Value;
use std::time::{SystemTime, UNIX_EPOCH};
use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};

const BASE: &str = "https://minmail.app/api";

fn random_seg(n: usize) -> String {
    let chars: Vec<char> = "abcdefghijklmnopqrstuvwxyz0123456789".chars().collect();
    let mut rng = rand::thread_rng();
    (0..n).map(|_| chars[rng.gen_range(0..chars.len())]).collect()
}

fn visitor_id() -> String {
    format!(
        "{}-{}-{}-{}-{}",
        random_seg(8),
        random_seg(4),
        random_seg(4),
        random_seg(4),
        random_seg(12)
    )
}

fn cookie_header() -> String {
    let now = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_millis() as i64)
        .unwrap_or(0);
    let rnd: i32 = rand::thread_rng().gen_range(0..1_000_000);
    let ga_id = format!("GA1.1.{}.{}", now, rnd);
    format!(
        "_ga=GA1.1.{ga_id}; _ga_DFGB8WF1WG=GS2.1.s{now}$o1$g0$t{now}$j60$l0$h0"
    )
}

fn build_req(client: &wreq::Client, path: &str, vid: &str) -> wreq::RequestBuilder {
    let url = format!("{}{}", BASE, path);
    let b = client.get(&url);
    b.header("Accept", "*/*")
        .header(
            "Accept-Language",
            "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6,zh-TW;q=0.5",
        )
        .header("Origin", "https://minmail.app")
        .header("Referer", "https://minmail.app/cn")
        .header("User-Agent", get_current_ua())
        .header("sec-ch-ua", r#""Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146""#)
        .header("sec-ch-ua-mobile", "?0")
        .header("sec-ch-ua-platform", r#""Windows""#)
        .header("sec-fetch-dest", "empty")
        .header("sec-fetch-mode", "cors")
        .header("sec-fetch-site", "same-origin")
        .header("visitor-id", vid)
        .header("Cookie", cookie_header())
}

pub fn generate_email() -> Result<EmailInfo, String> {
    let vid = visitor_id();
    block_on(async {
        let resp = build_req(&http_client(), "/mail/address?refresh=true&expire=1440&part=main", &vid)
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("minmail address {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let address = data["address"].as_str().unwrap_or("");
        if address.is_empty() {
            return Err("minmail: empty address".into());
        }
        let expire_min = data["expire"].as_i64().unwrap_or(0);
        let expires_at = if expire_min > 0 {
            let now = SystemTime::now()
                .duration_since(UNIX_EPOCH)
                .map(|d| d.as_millis() as i64)
                .unwrap_or(0);
            Some(now + expire_min * 60 * 1000)
        } else {
            None
        };
        Ok(EmailInfo {
            channel: Channel::Minmail,
            email: address.to_string(),
            token: Some(vid),
            expires_at,
            created_at: None,
        })
    })
}

pub fn get_emails(email: &str, token: Option<&str>) -> Result<Vec<Email>, String> {
    let vid = token.map(|s| s.to_string()).unwrap_or_else(visitor_id);
    let email = email.to_string();
    block_on(async {
        let resp = build_req(&http_client(), "/mail/list?part=main", &vid)
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("minmail list {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let messages = data["message"].as_array().cloned().unwrap_or_default();
        let mut out = Vec::new();
        for raw in messages {
            let to = raw["to"].as_str().unwrap_or("");
            if !to.is_empty() && to != email.as_str() {
                continue;
            }
            let to_s = raw["to"].as_str().unwrap_or(email.as_str());
            let flat = serde_json::json!({
                "id": raw["id"],
                "from": raw["from"],
                "to": to_s,
                "subject": raw["subject"],
                "text": raw["preview"],
                "html": raw["content"],
                "date": raw["date"],
                "isRead": raw["isRead"],
            });
            out.push(normalize_email(&flat, &email));
        }
        Ok(out)
    })
}
