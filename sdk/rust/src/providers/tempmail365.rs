/*!
 * Tempmail365 — https://tempmail365.cn
 *
 * 基础URL: https://tempmail365.cn/tempemail.php
 * 所有请求均为 GET，参数通过 query string 传递
 * 无需认证、无 token、无 cookie
 * 域名: fengyou.cc, shop345.com, nutemail.com, qvrf.cn
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use chrono::Utc;
use rand::Rng;
use regex::Regex;
use serde_json::{json, Value};

const BASE: &str = "https://tempmail365.cn/tempemail.php";

/// 已知域名列表（备用，当 get_config 接口不可用时使用）
const FALLBACK_DOMAINS: &[&str] = &["fengyou.cc", "shop345.com", "nutemail.com", "qvrf.cn"];

/// 构造请求头
fn tm365_headers(b: wreq::RequestBuilder) -> wreq::RequestBuilder {
    b.header("Accept", "application/json, text/plain, */*")
        .header("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
        .header("Cache-Control", "no-cache")
        .header("DNT", "1")
        .header("Pragma", "no-cache")
        .header("Referer", "https://tempmail365.cn/")
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

/// 从 API 获取可用域名列表
fn fetch_domains() -> Result<Vec<String>, String> {
    block_on(async {
        let url = format!("{}?action=get_config", BASE);
        let resp = tm365_headers(http_client().get(url))
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("tempmail365 get_config {}", resp.status()));
        }
        let v: Value = resp.json().await.map_err(|e| e.to_string())?;
        let arr = v
            .get("domains")
            .and_then(|x| x.as_array())
            .ok_or_else(|| "tempmail365: 无 domains 字段".to_string())?;
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
            return Err("tempmail365: 域名列表为空".into());
        }
        Ok(out)
    })
}

/// 选择域名：优先使用指定域名，否则随机选择
fn pick_domain(domains: &[String], preferred: Option<&str>) -> Result<String, String> {
    if let Some(p) = preferred {
        let pl = p.trim().to_lowercase();
        if !pl.is_empty() {
            if let Some(hit) = domains.iter().find(|d| d.to_lowercase() == pl) {
                return Ok(hit.clone());
            }
            return Err(format!("tempmail365: 域名不可用: {}", pl));
        }
    }
    let mut rng = rand::thread_rng();
    Ok(domains[rng.gen_range(0..domains.len())].clone())
}

/// 生成随机用户名（8位字母数字）
fn random_username() -> String {
    const CHARSET: &[u8] = b"abcdefghijklmnopqrstuvwxyz0123456789";
    let mut rng = rand::thread_rng();
    (0..8)
        .map(|_| CHARSET[rng.gen_range(0..CHARSET.len())] as char)
        .collect()
}

/// 创建临时邮箱
pub fn generate_email(domain: Option<&str>) -> Result<EmailInfo, String> {
    // 获取域名列表，失败则使用备用列表
    let domains = fetch_domains()
        .unwrap_or_else(|_| FALLBACK_DOMAINS.iter().map(|s| s.to_string()).collect());
    let d = pick_domain(&domains, domain)?;
    let user = random_username();
    let addr = format!("{}@{}", user, d);

    block_on(async {
        // 调用 create_email 接口
        let url = format!(
            "{}?action=create_email&email={}&domain={}",
            BASE,
            urlencoding::encode(&addr),
            urlencoding::encode(&d)
        );
        let resp = tm365_headers(http_client().get(url))
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("tempmail365 create_email {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        if data.get("success").and_then(|x| x.as_bool()) != Some(true) {
            return Err(format!("tempmail365: 创建邮箱失败: {}", data.to_string()));
        }
        Ok(EmailInfo {
            channel: Channel::Tempmail365,
            email: addr,
            token: None,
            expires_at: None,
            created_at: None,
        })
    })
}

/// 从 HTML 内容中提取发件人
fn extract_sender(content: &str) -> String {
    // 尝试匹配 "发件人:" 或 "From:" 后面的内容
    let re = Regex::new(r"(?:发件人|From)\s*[:：]\s*([^\r\n<]+)").ok();
    if let Some(re) = re {
        if let Some(caps) = re.captures(content) {
            let sender = caps.get(1).map(|m| m.as_str().trim()).unwrap_or("");
            if !sender.is_empty() {
                return sender.to_string();
            }
        }
    }
    String::new()
}

/// 从 HTML 内容中提取主题
fn extract_subject(content: &str) -> String {
    // 尝试匹配 "主题:" 或 "Subject:" 后面的内容
    let re = Regex::new(r"(?:主题|Subject)\s*[:：]\s*([^\r\n<]+)").ok();
    if let Some(re) = re {
        if let Some(caps) = re.captures(content) {
            let subject = caps.get(1).map(|m| m.as_str().trim()).unwrap_or("");
            if !subject.is_empty() {
                return subject.to_string();
            }
        }
    }
    String::new()
}

/// 获取邮件列表
pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let em = email.trim();
    if em.is_empty() {
        return Err("tempmail365: 邮箱地址为空".into());
    }
    let url = format!(
        "{}?action=fetch_mail&email={}",
        BASE,
        urlencoding::encode(em)
    );
    block_on(async {
        let resp = tm365_headers(http_client().get(url))
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("tempmail365 fetch_mail {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let content = data
            .get("content")
            .and_then(|x| x.as_str())
            .unwrap_or("")
            .trim()
            .to_string();

        // "无邮件" 表示没有邮件
        if content.is_empty() || content == "无邮件" {
            return Ok(vec![]);
        }

        // 从 HTML 内容中提取发件人和主题
        let sender = extract_sender(&content);
        let subject = extract_subject(&content);
        let now = Utc::now().to_rfc3339();

        // 构造邮件 JSON 对象，然后通过 normalize 处理
        let mail_obj = json!({
            "from": sender,
            "subject": subject,
            "html": content,
            "date": now,
        });

        let normalized = normalize_email(&mail_obj, em);
        Ok(vec![normalized])
    })
}
