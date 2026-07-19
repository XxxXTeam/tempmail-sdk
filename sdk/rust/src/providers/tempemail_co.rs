/*!
 * tempemail.co 渠道实现
 * 网站: https://tempemail.co
 * 域名: @fmail10.de（由 API 返回，可能变化）
 *
 * 流程（Cookie Session + REST JSON）:
 *   1. GET https://tempemail.co/mail/random
 *      需要保存 Cookie（cookie jar）
 *      返回: {"result":true,"id":"user@fmail10.de","address":"user@fmail10.de"}
 *
 *   2. GET https://tempemail.co/get-mails?mail_id={address}&unseen=0&is_new=1
 *      需要带上 Cookie
 *      返回: {"result":true,"mails":"<html table>","count":0}
 *      mails 字段是 HTML 字符串，包含邮件表格
 *      当 count > 0 时，HTML 中每个邮件项有 class="read-mail" data-id="{id}" 属性
 *      用正则提取邮件 ID: data-id="(\d+)"
 *
 *   3. GET https://tempemail.co/mail/info?id={mailId}
 *      返回: {"result":true,"mail":{"fromName":"","fromAddress":"xxx","displayDate":"xxx","subject":"xxx","textHtml":"<html>"}}
 *
 * token 存储: JSON 字符串 {"address":"user@fmail10.de","cookie":"..."}
 */

use std::collections::BTreeMap;
use std::sync::LazyLock;

use regex::Regex;
use serde::{Deserialize, Serialize};
use serde_json::Value;

use crate::config::{block_on, get_current_ua, http_client_no_cookie_jar};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};

const ORIGIN: &str = "https://tempemail.co";

/// 匹配邮件列表 HTML 中的邮件 ID（data-id="数字"）
static MAIL_ID_RE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r#"data-id="(\d+)""#).expect("re"));

/// token 中序列化的会话信息
#[derive(Serialize, Deserialize)]
struct TempemailCoSess {
    /// 邮箱地址
    address: String,
    /// Cookie 字符串
    cookie: String,
}

/// 解析 Cookie 头为 key=value 映射
fn parse_cookie_header(hdr: &str) -> BTreeMap<String, String> {
    let mut m = BTreeMap::new();
    for part in hdr.split(';') {
        let part = part.trim();
        if part.is_empty() {
            continue;
        }
        if let Some(i) = part.find('=') {
            let k = part[..i].trim();
            let v = part[i + 1..].trim();
            if !k.is_empty() {
                m.insert(k.to_string(), v.to_string());
            }
        }
    }
    m
}

/// 将 Cookie 映射转换回 Cookie 头字符串
fn cookie_header_from_map(m: &BTreeMap<String, String>) -> String {
    m.iter()
        .map(|(k, v)| format!("{}={}", k, v))
        .collect::<Vec<_>>()
        .join("; ")
}

/// 合并 Set-Cookie 响应头到已有 Cookie 中
fn merge_set_cookies(hdr: &str, headers: &wreq::header::HeaderMap) -> String {
    let mut m = parse_cookie_header(hdr);
    for val in headers.get_all("set-cookie") {
        let Ok(s) = val.to_str() else {
            continue;
        };
        let first = s.split(';').next().unwrap_or("");
        if let Some(i) = first.find('=') {
            let k = first[..i].trim();
            let v = first[i + 1..].trim();
            if !k.is_empty() {
                m.insert(k.to_string(), v.to_string());
            }
        }
    }
    cookie_header_from_map(&m)
}

/// 编码会话信息为 token 字符串
fn encode_sess(s: &TempemailCoSess) -> Result<String, String> {
    serde_json::to_string(s).map_err(|e| format!("tempemail-co: token 序列化失败: {}", e))
}

/// 从 token 字符串解码会话信息
fn decode_sess(tok: &str) -> Result<TempemailCoSess, String> {
    let s: TempemailCoSess =
        serde_json::from_str(tok).map_err(|_| "tempemail-co: token 反序列化失败".to_string())?;
    if s.address.is_empty() {
        return Err("tempemail-co: token 中 address 为空".into());
    }
    if s.cookie.is_empty() {
        return Err("tempemail-co: token 中 cookie 为空".into());
    }
    Ok(s)
}

/// 构建浏览器请求头
fn browser_headers(referer: &str) -> Vec<(&'static str, String)> {
    vec![
        ("User-Agent", get_current_ua().to_string()),
        (
            "Accept",
            "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,*/*;q=0.8"
                .to_string(),
        ),
        ("Accept-Language", "en-US,en;q=0.9".to_string()),
        ("Cache-Control", "no-cache".to_string()),
        ("Pragma", "no-cache".to_string()),
        ("Referer", referer.to_string()),
    ]
}

/// 构建 JSON 请求头
fn json_headers(referer: &str) -> Vec<(&'static str, String)> {
    vec![
        ("User-Agent", get_current_ua().to_string()),
        (
            "Accept",
            "application/json, text/javascript, */*; q=0.01".to_string(),
        ),
        ("Accept-Language", "en-US,en;q=0.9".to_string()),
        ("X-Requested-With", "XMLHttpRequest".to_string()),
        ("Referer", referer.to_string()),
    ]
}

/// 创建临时邮箱
/// GET /mail/random → 获取分配的邮箱地址，保存 Cookie
pub fn generate_email() -> Result<EmailInfo, String> {
    block_on(async {
        let client = http_client_no_cookie_jar();

        // GET /mail/random 创建随机邮箱
        let mut req = client.get(format!("{}/mail/random", ORIGIN));
        for (k, v) in browser_headers(ORIGIN) {
            req = req.header(k, v);
        }
        let resp = req
            .send()
            .await
            .map_err(|e| format!("tempemail-co: 创建邮箱请求失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("tempemail-co: 创建邮箱返回 HTTP {}", resp.status()));
        }

        // 收集 Set-Cookie
        let cookie_hdr = merge_set_cookies("", resp.headers());

        let body: Value = resp
            .json()
            .await
            .map_err(|e| format!("tempemail-co: 解析创建邮箱响应失败: {}", e))?;

        // 检查 result 字段
        let result_ok = body
            .get("result")
            .and_then(|v| v.as_bool())
            .unwrap_or(false);
        if !result_ok {
            return Err("tempemail-co: API 返回 result=false".into());
        }

        // 优先取 address 字段，其次 id 字段
        let address = body
            .get("address")
            .or_else(|| body.get("id"))
            .and_then(|v| v.as_str())
            .map(|s| s.trim().to_string())
            .ok_or_else(|| "tempemail-co: 响应中未找到 address 字段".to_string())?;

        if address.is_empty() || !address.contains('@') {
            return Err(format!("tempemail-co: 返回的邮箱地址无效: {}", address));
        }

        let tok = encode_sess(&TempemailCoSess {
            address: address.clone(),
            cookie: cookie_hdr,
        })?;

        Ok(EmailInfo {
            channel: Channel::TempemailCo,
            email: address,
            token: Some(tok),
            expires_at: None,
            created_at: None,
        })
    })
}

/// 获取邮件列表
/// 1. GET /get-mails?mail_id={address}&unseen=0&is_new=1 → 用正则从 HTML 中提取邮件 ID
/// 2. 对每个 ID 调用 GET /mail/info?id={id} → 获取邮件详情 JSON
pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let sess = decode_sess(token)?;
    let em = email.trim();
    if em.is_empty() {
        return Err("tempemail-co: 邮箱地址为空".into());
    }

    block_on(async {
        let client = http_client_no_cookie_jar();
        let page_url = format!("{}/", ORIGIN);

        // 第一步：GET /get-mails 获取邮件列表（HTML 包裹在 JSON 中）
        let list_url = format!(
            "{}/get-mails?mail_id={}&unseen=0&is_new=1",
            ORIGIN, sess.address
        );
        let mut req = client.get(&list_url);
        for (k, v) in json_headers(&page_url) {
            req = req.header(k, v);
        }
        req = req.header("Cookie", &sess.cookie);
        let resp = req
            .send()
            .await
            .map_err(|e| format!("tempemail-co: 获取邮件列表请求失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!(
                "tempemail-co: 获取邮件列表返回 HTTP {}",
                resp.status()
            ));
        }

        let body: Value = resp
            .json()
            .await
            .map_err(|e| format!("tempemail-co: 解析邮件列表响应失败: {}", e))?;

        // 检查 count 字段，0 表示无邮件
        let count = body
            .get("count")
            .and_then(|v| v.as_i64().or_else(|| v.as_u64().map(|n| n as i64)))
            .unwrap_or(0);

        if count <= 0 {
            return Ok(vec![]);
        }

        // 从 mails 字段的 HTML 中用正则提取邮件 ID
        let mails_html = body.get("mails").and_then(|v| v.as_str()).unwrap_or("");

        let mail_ids: Vec<String> = MAIL_ID_RE
            .captures_iter(mails_html)
            .filter_map(|cap| cap.get(1).map(|m| m.as_str().to_string()))
            .collect();

        if mail_ids.is_empty() {
            return Ok(vec![]);
        }

        // 第二步：逐个获取邮件详情
        let mut result = Vec::with_capacity(mail_ids.len());
        for mail_id in &mail_ids {
            let info_url = format!("{}/mail/info?id={}", ORIGIN, mail_id);
            let mut reqd = client.get(&info_url);
            for (k, v) in json_headers(&page_url) {
                reqd = reqd.header(k, v);
            }
            reqd = reqd.header("Cookie", &sess.cookie);

            let detail = match reqd.send().await {
                Ok(r) if r.status().is_success() => r.json::<Value>().await.unwrap_or(Value::Null),
                _ => continue,
            };

            // 从 mail 子对象中提取字段
            let mail_obj = detail.get("mail").unwrap_or(&Value::Null);

            let from = mail_obj
                .get("fromAddress")
                .or_else(|| mail_obj.get("fromName"))
                .and_then(|v| v.as_str())
                .unwrap_or("");
            let subject = mail_obj
                .get("subject")
                .and_then(|v| v.as_str())
                .unwrap_or("");
            let date = mail_obj
                .get("displayDate")
                .and_then(|v| v.as_str())
                .unwrap_or("");
            let html_body = mail_obj
                .get("textHtml")
                .and_then(|v| v.as_str())
                .unwrap_or("");

            let raw = serde_json::json!({
                "id": mail_id,
                "from": from,
                "to": em,
                "subject": subject,
                "html": html_body,
                "text": "",
                "date": date,
                "isRead": false,
                "attachments": [],
            });
            result.push(normalize_email(&raw, em));
        }

        Ok(result)
    })
}
