/*!
 * 24mail-chacuo — http://24mail.chacuo.net
 * POST 注册/刷新同一接口，HTTP only。
 */

use rand::Rng;
use serde_json::Value;

use crate::config::{block_on, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};

const BASE_URL: &str = "http://24mail.chacuo.net";
const DOMAINS: &[&str] = &["chacuo.net", "027168.com"];

fn random_local(length: usize) -> String {
    let chars: Vec<char> = "abcdefghijklmnopqrstuvwxyz0123456789".chars().collect();
    let mut rng = rand::thread_rng();
    (0..length)
        .map(|_| chars[rng.gen_range(0..chars.len())])
        .collect()
}

fn apply_headers(mut req: wreq::RequestBuilder) -> wreq::RequestBuilder {
    req = req.header("Accept", "application/json, text/javascript, */*; q=0.01");
    req = req.header(
        "Accept-Language",
        "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
    );
    req = req.header(
        "Content-Type",
        "application/x-www-form-urlencoded; charset=UTF-8",
    );
    req = req.header("Origin", BASE_URL);
    req = req.header("Referer", format!("{}/", BASE_URL));
    req = req.header("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0");
    req = req.header("x-requested-with", "XMLHttpRequest");
    req
}

pub fn generate_email() -> Result<EmailInfo, String> {
    let client = http_client();
    let name = random_local(10);
    let domain = DOMAINS[rand::thread_rng().gen_range(0..DOMAINS.len())];

    let body = format!("data={}%40{}&type=refresh&arg=", name, domain);

    block_on(async {
        let req = apply_headers(client.post(format!("{}/", BASE_URL)));
        let resp = req.body(body).send().await.map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!(
                "24mail-chacuo: 创建邮箱失败 HTTP {}",
                resp.status()
            ));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let status = data["status"].as_i64().unwrap_or(0);
        if status != 1 {
            let info = data["info"].as_str().unwrap_or("");
            return Err(format!(
                "24mail-chacuo: 创建失败 status={} info={}",
                status, info
            ));
        }
        let email_addr = format!("{}@{}", name, domain);
        Ok(EmailInfo {
            channel: Channel::TwentyfourmailChacuo,
            email: email_addr.clone(),
            token: Some(email_addr),
            expires_at: None,
            created_at: None,
        })
    })
}

pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let client = http_client();
    let at_idx = email.find('@');
    let (name, domain) = match at_idx {
        Some(idx) if idx > 0 => (&email[..idx], &email[idx + 1..]),
        _ => (email, DOMAINS[0]),
    };

    let body = format!("data={}%40{}&type=refresh&arg=", name, domain);

    block_on(async {
        let req = apply_headers(client.post(format!("{}/", BASE_URL)));
        let resp = req.body(body).send().await.map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!(
                "24mail-chacuo: 获取邮件失败 HTTP {}",
                resp.status()
            ));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let status = data["status"].as_i64().unwrap_or(0);
        if status != 1 {
            return Ok(vec![]);
        }
        let list = match data["data"].as_array() {
            Some(arr) if !arr.is_empty() => arr[0]["list"].as_array().cloned().unwrap_or_default(),
            _ => return Ok(vec![]),
        };

        let mut out = Vec::new();
        for item in &list {
            let flat = serde_json::json!({
                "id": item["MID"].as_str().unwrap_or(""),
                "from": item["FROM"].as_str().unwrap_or(""),
                "to": email,
                "subject": item["SUBJECT"].as_str().unwrap_or(""),
                "body": item["CONTENT"].as_str().unwrap_or(""),
                "date": item["TIME"].as_str().unwrap_or(""),
                "isRead": false,
            });
            out.push(normalize_email(&flat, email));
        }
        Ok(out)
    })
}
