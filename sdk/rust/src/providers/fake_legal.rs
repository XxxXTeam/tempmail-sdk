/*!
 * Fake Legal — https://imgui.de
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use rand::Rng;
use serde_json::Value;
use serde_json::json;

const BASE: &str = "https://imgui.de";

fn fl_headers(b: wreq::RequestBuilder) -> wreq::RequestBuilder {
    b.header("Accept", "application/json, text/plain, */*")
        .header("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
        .header("Cache-Control", "no-cache")
        .header("DNT", "1")
        .header("Pragma", "no-cache")
        .header("Referer", "https://imgui.de/")
        .header(
            "sec-ch-ua",
            r#""Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146""#,
        )
        .header("sec-ch-ua-mobile", "?0")
        .header("sec-ch-ua-platform", r#""Windows""#)
        .header("sec-fetch-dest", "empty")
        .header("sec-fetch-mode", "cors")
        .header("sec-fetch-site", "same-origin")
        .header("User-Agent", get_current_ua())
}

fn random_username(len: usize) -> String {
    const ALPHANUM: &[u8] = b"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    let mut rng = rand::thread_rng();
    (0..len)
        .map(|_| ALPHANUM[rng.gen_range(0..ALPHANUM.len())] as char)
        .collect()
}

fn fetch_domains() -> Result<Vec<String>, String> {
    block_on(async {
        let resp = fl_headers(http_client().get(format!("{}/api/domains", BASE)))
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("fake-legal domains {}", resp.status()));
        }
        let v: Value = resp.json().await.map_err(|e| e.to_string())?;
        let arr = v
            .get("domains")
            .and_then(|x| x.as_array())
            .ok_or_else(|| "fake-legal: no domains".to_string())?;
        let mut out = Vec::new();
        for x in arr {
            if let Some(s) = x.as_str() {
                let t = s.trim();
                if !t.is_empty() {
                    out.push(t.to_string());
                }
            }
        }
        if out.is_empty() {
            return Err("fake-legal: no domains".into());
        }
        Ok(out)
    })
}

fn pick_domain(domains: &[String], preferred: Option<&str>) -> Result<String, String> {
    if let Some(p) = preferred {
        let pl = p.trim().to_lowercase();
        if !pl.is_empty() {
            if let Some(hit) = domains.iter().find(|d| d.to_lowercase() == pl) {
                return Ok(hit.clone());
            }
            return Err(format!("fake-legal: domain not available: {}", pl));
        }
    }
    let mut rng = rand::thread_rng();
    Ok(domains[rng.gen_range(0..domains.len())].clone())
}

pub fn generate_email(domain: Option<&str>) -> Result<EmailInfo, String> {
    let domains = fetch_domains()?;
    let d = pick_domain(&domains, domain)?;
    block_on(async {
        let resp = if d == "imgui.de" || d == "pulsewebmenu.de" {
            let username = random_username(12);
            let body = json!({ "username": username, "domain": d });
            fl_headers(http_client().post(format!("{}/api/inbox/custom", BASE)))
                .header("Content-Type", "application/json")
                .body(body.to_string())
                .send()
                .await
                .map_err(|e| e.to_string())?
        } else {
            let q = urlencoding::encode(&d);
            let url = format!("{}/api/inbox/new?domain={}", BASE, q);
            fl_headers(http_client().get(url))
                .send()
                .await
                .map_err(|e| e.to_string())?
        };
        if !resp.status().is_success() {
            return Err(format!("fake-legal new inbox {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        if data.get("success").and_then(|x| x.as_bool()) != Some(true) {
            return Err("fake-legal: create failed".into());
        }
        let addr = data
            .get("address")
            .and_then(|x| x.as_str())
            .unwrap_or("")
            .trim()
            .to_string();
        if addr.is_empty() {
            return Err("fake-legal: missing address".into());
        }
        Ok(EmailInfo {
            channel: Channel::FakeLegal,
            email: addr,
            token: None,
            expires_at: None,
            created_at: None,
        })
    })
}

pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let em = email.trim();
    if em.is_empty() {
        return Err("fake-legal: empty email".into());
    }
    let enc = urlencoding::encode(em);
    let url = format!("{}/api/inbox/{}", BASE, enc);
    block_on(async {
        let resp = fl_headers(http_client().get(url))
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("fake-legal inbox {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        if data.get("success").and_then(|x| x.as_bool()) != Some(true) {
            return Ok(vec![]);
        }
        let rows = data
            .get("emails")
            .and_then(|x| x.as_array())
            .cloned()
            .unwrap_or_default();
        let mut out = Vec::new();
        for raw in rows {
            out.push(normalize_email(&raw, em));
        }
        Ok(out)
    })
}
