/*!
 * freecustom 渠道实现
 * 网站: https://freecustom.email
 *
 * 免注册临时邮箱，任意 local part @ 可用域名即时可收信。
 * 流程:
 *   1. GET https://api2.freecustom.email/domains （无需鉴权）
 *      筛选 tier=="free" 且 expiring_soon 非 true 的域名，随机选一个
 *   2. 本地随机生成 10 位 [a-z0-9] local part，email = local@domain
 *   3. 读信时动态获取 JWT：
 *      a. POST /api/auth （无 body）→ {"token":"<JWT>"}
 *      b. GET /api/public-mailbox?fullMailboxId=<email> 取邮件元数据列表
 *      c. 对每封 GET .../api/public-mailbox?fullMailboxId=<email>&messageId=<id> 补全正文
 *
 * token 即邮箱地址本身，读信仅需 email。
 */

use rand::Rng;
use serde_json::Value;

use crate::config::{block_on, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};

const SITE_URL: &str = "https://www.freecustom.email";
const DOMAINS_URL: &str = "https://api2.freecustom.email/domains";
const REFERER: &str = "https://www.freecustom.email/en";
const UA: &str = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36";

/// 生成随机本地部分（小写字母 + 数字）
fn random_local(length: usize) -> String {
    let chars: Vec<char> = "abcdefghijklmnopqrstuvwxyz0123456789".chars().collect();
    let mut rng = rand::thread_rng();
    (0..length)
        .map(|_| chars[rng.gen_range(0..chars.len())])
        .collect()
}

/// URL 编码邮箱地址中的 @ 符号（其余字符均为 [a-z0-9.]，无需编码）
fn encode_mail(email: &str) -> String {
    email.replace('@', "%40")
}

/// 挑选一个当前可收信的域名
/// api2 /domains 无需鉴权，优先选择 tier=="free" 且未过期（expiring_soon 非 true）的域名，
/// 若无则退回全量列表随机。
async fn pick_domain(client: &wreq::Client) -> Result<String, String> {
    let resp = client
        .get(DOMAINS_URL)
        .header("User-Agent", UA)
        .header("Accept", "application/json")
        .header("Referer", REFERER)
        .send()
        .await
        .map_err(|e| format!("freecustom: 获取域名列表失败: {}", e))?;

    if !resp.status().is_success() {
        return Err(format!(
            "freecustom: 获取域名列表失败 HTTP {}",
            resp.status()
        ));
    }

    let data: Value = resp
        .json()
        .await
        .map_err(|e| format!("freecustom: 解析域名列表失败: {}", e))?;

    let list = match data.get("data").and_then(|v| v.as_array()) {
        Some(arr) if !arr.is_empty() => arr,
        _ => return Err("freecustom: 域名列表为空".into()),
    };

    // 优先未过期的 free 域名；若全部标记过期则退回全量列表
    let usable: Vec<&str> = list
        .iter()
        .filter(|d| {
            let tier = d.get("tier").and_then(|v| v.as_str()).unwrap_or("");
            let expiring = d
                .get("expiring_soon")
                .and_then(|v| v.as_bool())
                .unwrap_or(false);
            tier == "free" && !expiring
        })
        .filter_map(|d| d.get("domain").and_then(|v| v.as_str()))
        .collect();

    let pool: Vec<&str> = if !usable.is_empty() {
        usable
    } else {
        list.iter()
            .filter_map(|d| d.get("domain").and_then(|v| v.as_str()))
            .collect()
    };

    if pool.is_empty() {
        return Err("freecustom: 无可用域名".into());
    }

    let idx = rand::thread_rng().gen_range(0..pool.len());
    Ok(pool[idx].to_string())
}

/// 获取匿名访问令牌（JWT，有效期约 2 小时）
/// POST /api/auth → { token }
async fn fetch_auth_token(client: &wreq::Client) -> Result<String, String> {
    let resp = client
        .post(format!("{}/api/auth", SITE_URL))
        .header("User-Agent", UA)
        .header("Accept", "application/json")
        .header("Content-Type", "application/json")
        .header("Referer", REFERER)
        .send()
        .await
        .map_err(|e| format!("freecustom: 获取令牌失败: {}", e))?;

    if !resp.status().is_success() {
        return Err(format!("freecustom: 获取令牌失败 HTTP {}", resp.status()));
    }

    let data: Value = resp
        .json()
        .await
        .map_err(|e| format!("freecustom: 解析令牌响应失败: {}", e))?;

    let token = data
        .get("token")
        .and_then(|v| v.as_str())
        .unwrap_or("")
        .trim()
        .to_string();

    if token.is_empty() {
        return Err("freecustom: 令牌响应无效".into());
    }
    Ok(token)
}

/// 创建 freecustom.email 临时邮箱
/// 该服务免注册，任意 local part @ 可用域名即时可收信。
pub fn generate_email() -> Result<EmailInfo, String> {
    let client = http_client();

    block_on(async {
        let domain = pick_domain(&client).await?;
        let email = format!("{}@{}", random_local(10), domain);

        Ok(EmailInfo {
            channel: Channel::Freecustom,
            email: email.clone(),
            token: Some(email),
            expires_at: None,
            created_at: None,
        })
    })
}

/// 获取 freecustom.email 邮件列表
/// 1. POST /api/auth 取 JWT
/// 2. GET /api/public-mailbox?fullMailboxId=<email> 取邮件元数据列表
/// 3. 对每封 GET /api/public-mailbox?fullMailboxId=<email>&messageId=<id> 补全正文
pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let addr = email.trim();
    if addr.is_empty() {
        return Err("freecustom: 缺少邮箱地址".into());
    }
    let addr = addr.to_string();
    let client = http_client();

    block_on(async {
        let jwt = fetch_auth_token(&client).await?;
        let bearer = format!("Bearer {}", jwt);
        let encoded = encode_mail(&addr);

        let list_resp = client
            .get(format!(
                "{}/api/public-mailbox?fullMailboxId={}",
                SITE_URL, encoded
            ))
            .header("User-Agent", UA)
            .header("Accept", "application/json")
            .header("Referer", REFERER)
            .header("Authorization", &bearer)
            .header("x-fce-client", "web-client")
            .send()
            .await
            .map_err(|e| format!("freecustom: 获取邮件失败: {}", e))?;

        if !list_resp.status().is_success() {
            return Err(format!(
                "freecustom: 获取邮件失败 HTTP {}",
                list_resp.status()
            ));
        }

        let list_data: Value = list_resp
            .json()
            .await
            .map_err(|e| format!("freecustom: 解析邮件列表失败: {}", e))?;

        let success = list_data
            .get("success")
            .and_then(|v| v.as_bool())
            .unwrap_or(false);
        if !success {
            return Ok(vec![]);
        }
        let list = match list_data.get("data").and_then(|v| v.as_array()) {
            Some(arr) if !arr.is_empty() => arr,
            _ => return Ok(vec![]),
        };

        let mut out = Vec::with_capacity(list.len());
        for item in list {
            let id = json_str(item.get("id"));
            if id.is_empty() {
                continue;
            }

            // 默认使用列表元数据，尝试补全正文，失败则退回
            let mut full = item.clone();
            let msg_resp = client
                .get(format!(
                    "{}/api/public-mailbox?fullMailboxId={}&messageId={}",
                    SITE_URL, encoded, id
                ))
                .header("User-Agent", UA)
                .header("Accept", "application/json")
                .header("Referer", REFERER)
                .header("Authorization", &bearer)
                .header("x-fce-client", "web-client")
                .send()
                .await;

            if let Ok(resp) = msg_resp {
                if resp.status().is_success() {
                    if let Ok(msg_data) = resp.json::<Value>().await {
                        let ok = msg_data
                            .get("success")
                            .and_then(|v| v.as_bool())
                            .unwrap_or(false);
                        if ok {
                            if let Some(d) = msg_data.get("data") {
                                if d.is_object() {
                                    full = d.clone();
                                }
                            }
                        }
                    }
                }
            }

            let to = {
                let t = json_str(full.get("to"));
                if t.is_empty() {
                    addr.clone()
                } else {
                    t
                }
            };
            let raw = serde_json::json!({
                "id": json_str(full.get("id")),
                "from": json_str(full.get("from")),
                "to": to,
                "subject": json_str(full.get("subject")),
                "text": json_str(full.get("text")),
                "html": json_str(full.get("html")),
                "date": json_str(full.get("date")),
                "isRead": false,
            });
            out.push(normalize_email(&raw, &addr));
        }

        Ok(out)
    })
}

/// 安全提取 JSON 值为字符串（字符串原样、数字转文本）
fn json_str(v: Option<&Value>) -> String {
    match v {
        Some(v) if v.is_string() => v.as_str().unwrap_or("").to_string(),
        Some(v) if v.is_number() => v.to_string(),
        _ => String::new(),
    }
}
