/*!
 * TempInbox — https://www.tempinbox.xyz
 * 无需认证、无token、无cookie
 */

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};
use rand::Rng;
use serde_json::Value;

const BASE: &str = "https://endpoint.tempinbox.xyz";

/// 可用域名列表
const DOMAINS: &[&str] = &["tempinbox.xyz", "thepiratebay.cloud", "cryptoblad.nl"];

/// 构造通用请求头
fn ti_headers(b: wreq::RequestBuilder) -> wreq::RequestBuilder {
    b.header("Accept", "application/json, text/plain, */*")
        .header("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
        .header("Cache-Control", "no-cache")
        .header("DNT", "1")
        .header("Pragma", "no-cache")
        .header("Referer", "https://www.tempinbox.xyz/")
        .header(
            "sec-ch-ua",
            r#""Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146""#,
        )
        .header("sec-ch-ua-mobile", "?0")
        .header("sec-ch-ua-platform", r#""Windows""#)
        .header("sec-fetch-dest", "empty")
        .header("sec-fetch-mode", "cors")
        .header("sec-fetch-site", "cross-site")
        .header("User-Agent", get_current_ua())
}

/// 选择域名：如果用户指定了域名则匹配，否则随机选取
fn pick_domain(preferred: Option<&str>) -> Result<String, String> {
    if let Some(p) = preferred {
        let pl = p.trim().to_lowercase();
        if !pl.is_empty() {
            if let Some(hit) = DOMAINS.iter().find(|d| d.to_lowercase() == pl) {
                return Ok(hit.to_string());
            }
            return Err(format!("tempinbox: 域名不可用: {}", pl));
        }
    }
    let mut rng = rand::thread_rng();
    Ok(DOMAINS[rng.gen_range(0..DOMAINS.len())].to_string())
}

/// 创建随机临时邮箱
pub fn generate_email(domain: Option<&str>) -> Result<EmailInfo, String> {
    block_on(async {
        let addr = if let Some(d) = domain {
            // 用户指定了域名，生成随机用户名后拼接
            let dom = pick_domain(Some(d))?;
            let user = generate_random_user();
            let target = format!("{}@{}", user, dom);
            let url = format!("{}/email/{}", BASE, urlencoding::encode(&target));
            let resp = ti_headers(http_client().get(url))
                .send()
                .await
                .map_err(|e| e.to_string())?;
            if !resp.status().is_success() {
                return Err(format!("tempinbox 创建邮箱失败 {}", resp.status()));
            }
            let body = resp.text().await.map_err(|e| e.to_string())?;
            // 响应是带引号的纯字符串，去除引号
            body.trim().trim_matches('"').to_string()
        } else {
            // 随机邮箱
            let url = format!("{}/email/Random", BASE);
            let resp = ti_headers(http_client().get(url))
                .send()
                .await
                .map_err(|e| e.to_string())?;
            if !resp.status().is_success() {
                return Err(format!("tempinbox 随机邮箱 {}", resp.status()));
            }
            let body = resp.text().await.map_err(|e| e.to_string())?;
            body.trim().trim_matches('"').to_string()
        };

        if addr.is_empty() || !addr.contains('@') {
            return Err("tempinbox: 返回的邮箱地址无效".into());
        }

        Ok(EmailInfo {
            channel: Channel::Tempinbox,
            email: addr,
            token: None,
            expires_at: None,
            created_at: None,
        })
    })
}

/// 获取邮件列表
pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let em = email.trim();
    if em.is_empty() {
        return Err("tempinbox: 邮箱地址为空".into());
    }
    let enc = urlencoding::encode(em);
    let url = format!("{}/messages/{}", BASE, enc);
    block_on(async {
        let resp = ti_headers(http_client().get(url))
            .send()
            .await
            .map_err(|e| e.to_string())?;
        if !resp.status().is_success() {
            return Err(format!("tempinbox 获取邮件 {}", resp.status()));
        }
        let data: Value = resp.json().await.map_err(|e| e.to_string())?;
        let rows = data.as_array().cloned().unwrap_or_default();
        let mut out = Vec::new();
        for raw in rows {
            out.push(normalize_email(&raw, em));
        }
        Ok(out)
    })
}

/// 生成随机用户名
fn generate_random_user() -> String {
    let mut rng = rand::thread_rng();
    let len = rng.gen_range(8..13);
    (0..len)
        .map(|_| {
            let idx = rng.gen_range(0..36);
            if idx < 26 {
                (b'a' + idx) as char
            } else {
                (b'0' + idx - 26) as char
            }
        })
        .collect()
}
