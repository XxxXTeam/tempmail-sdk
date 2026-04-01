use rand::Rng;
use serde_json::Value;
use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};

const BASE: &str = "https://maildrop.cx";
const EXCLUDED_SUFFIX: &str = "transformer.edu.kg";

fn md_headers(b: wreq::RequestBuilder) -> wreq::RequestBuilder {
    b.header("Accept", "application/json, text/plain, */*")
        .header("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6")
        .header("Cache-Control", "no-cache")
        .header("DNT", "1")
        .header("Pragma", "no-cache")
        .header("Referer", "https://maildrop.cx/zh-cn/app")
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
        .header("x-requested-with", "XMLHttpRequest")
}

fn random_local(len: usize) -> String {
    let chars: Vec<char> = "abcdefghijklmnopqrstuvwxyz0123456789".chars().collect();
    let mut rng = rand::thread_rng();
    (0..len).map(|_| chars[rng.gen_range(0..chars.len())]).collect()
}

fn fetch_suffixes() -> Result<Vec<String>, String> {
    block_on(async {
        let resp = md_headers(http_client().get(format!("{}/api/suffixes.php", BASE)))
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("maildrop suffixes {}", resp.status()));
        }
        let v: Value = resp.json().await.map_err(|e| e.to_string())?;
        let arr = v
            .as_array()
            .ok_or_else(|| "maildrop: invalid suffixes".to_string())?;
        let ex = EXCLUDED_SUFFIX.to_lowercase();
        let mut out = Vec::new();
        for x in arr {
            if let Some(s) = x.as_str() {
                let t = s.trim();
                if !t.is_empty() && t.to_lowercase() != ex {
                    out.push(t.to_string());
                }
            }
        }
        if out.is_empty() {
            return Err("maildrop: no domains".into());
        }
        Ok(out)
    })
}

fn pick_suffix(suffixes: &[String], preferred: Option<&str>) -> String {
    if let Some(p) = preferred {
        let pl = p.trim().to_lowercase();
        if !pl.is_empty() {
            if let Some(hit) = suffixes.iter().find(|d| d.to_lowercase() == pl) {
                return hit.clone();
            }
        }
    }
    let mut rng = rand::thread_rng();
    suffixes[rng.gen_range(0..suffixes.len())].clone()
}

pub fn generate_email(domain: Option<&str>) -> Result<EmailInfo, String> {
    let suffixes = fetch_suffixes()?;
    let dom = pick_suffix(&suffixes, domain);
    let local = random_local(10);
    let email = format!("{}@{}", local, dom);
    Ok(EmailInfo {
        channel: Channel::Maildrop,
        email: email.clone(),
        token: Some(email),
        expires_at: None,
        created_at: None,
    })
}

pub fn get_emails(_token: &str, email: &str) -> Result<Vec<Email>, String> {
    let addr = email.trim();
    let addr = if addr.is_empty() { _token.trim() } else { addr };
    if addr.is_empty() {
        return Err("maildrop: empty address".into());
    }
    let q = urlencoding::encode(addr);
    let url = format!(
        "{}/api/emails.php?addr={}&page=1&limit=20",
        BASE,
        q
    );
    block_on(async {
        let resp = md_headers(http_client().get(url))
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("maildrop emails {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let rows = data
            .get("emails")
            .and_then(|x| x.as_array())
            .cloned()
            .unwrap_or_default();

        Ok(rows
            .iter()
            .map(|raw| normalize_email(raw, addr))
            .collect())
    })
}
