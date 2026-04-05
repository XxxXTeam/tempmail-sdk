/*!
 * 10mail.wangtz.cn：POST /api/tempMail 创建；POST /api/emailList 拉取（无需 token）
 */

use serde_json::{json, Map, Value};

use crate::config::{block_on, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};

const ORIGIN: &str = "https://10mail.wangtz.cn";
const MAIL_DOMAIN: &str = "wangtz.cn";

fn json_scalar_str(v: &Value) -> String {
    match v {
        Value::String(s) => s.clone(),
        Value::Number(n) => n.to_string(),
        Value::Bool(b) => b.to_string(),
        _ => v.to_string(),
    }
}

fn wangtz_flatten_message(raw: &Map<String, Value>) -> Value {
    let mut flat = raw.clone();
    if let Some(mid) = raw.get("messageId") {
        flat.insert("id".to_string(), Value::String(json_scalar_str(mid)));
    }
    if let Some(Value::Object(from_obj)) = raw.get("from") {
        if let Some(t) = from_obj.get("text") {
            flat.insert("from".to_string(), Value::String(json_scalar_str(t)));
        }
    }
    Value::Object(flat)
}

fn email_name_from_domain(domain: Option<&str>) -> String {
    let Some(s) = domain else {
        return String::new();
    };
    let s = s.trim();
    if let Some(i) = s.find('@') {
        s[..i].trim().to_string()
    } else {
        s.to_string()
    }
}

pub fn generate_email(domain: Option<&str>) -> Result<EmailInfo, String> {
    let email_name = email_name_from_domain(domain);
    block_on(async {
        let body = json!({ "emailName": email_name }).to_string();
        let resp = http_client()
            .post(format!("{ORIGIN}/api/tempMail"))
            .header("User-Agent", get_current_ua())
            .header("Accept", "*/*")
            .header("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6")
            .header("Content-Type", "application/json; charset=utf-8")
            .header("Origin", ORIGIN)
            .header("Referer", format!("{ORIGIN}/"))
            .header("token", "null")
            .header("Authorization", "null")
            .body(body)
            .send()
            .await
            .map_err(|e| format!("10mail-wangtz generate: {}", e))?;
        if !resp.status().is_success() {
            return Err(format!("10mail-wangtz generate: {}", resp.status()));
        }
        let v: Value = resp.json().await.map_err(|e| e.to_string())?;
        let code = v.get("code").and_then(|c| c.as_i64()).unwrap_or(-1);
        if code != 0 {
            let msg = v.get("msg").and_then(|m| m.as_str()).unwrap_or("error");
            return Err(format!("10mail-wangtz: {}", msg.trim()));
        }
        let data = v
            .get("data")
            .and_then(|d| d.as_object())
            .ok_or_else(|| "10mail-wangtz: missing data".to_string())?;
        let local = data
            .get("mailName")
            .and_then(|m| m.as_str())
            .unwrap_or("")
            .trim();
        if local.is_empty() {
            return Err("10mail-wangtz: empty mailName".into());
        }
        let addr = format!("{local}@{MAIL_DOMAIN}");
        let expires_at = data
            .get("endTime")
            .and_then(|t| t.as_f64())
            .filter(|&x| x > 0.0)
            .map(|ms| ms as i64);
        Ok(EmailInfo {
            channel: Channel::TenmailWangtz,
            email: addr,
            token: None,
            expires_at,
            created_at: None,
        })
    })
}

pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    let email = email.trim().to_string();
    block_on(async {
        let body = json!({ "emailName": email }).to_string();
        let resp = http_client()
            .post(format!("{ORIGIN}/api/emailList"))
            .header("User-Agent", get_current_ua())
            .header("Accept", "*/*")
            .header("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6")
            .header("Content-Type", "application/json; charset=utf-8")
            .header("Origin", ORIGIN)
            .header("Referer", format!("{ORIGIN}/"))
            .header("token", "null")
            .header("Authorization", "null")
            .body(body)
            .send()
            .await
            .map_err(|e| format!("10mail-wangtz inbox: {}", e))?;
        if !resp.status().is_success() {
            return Err(format!("10mail-wangtz inbox: {}", resp.status()));
        }
        let arr: Vec<Value> = resp.json().await.map_err(|e| e.to_string())?;
        let mut out = Vec::with_capacity(arr.len());
        for item in arr {
            if let Value::Object(m) = item {
                let flat = wangtz_flatten_message(&m);
                out.push(normalize_email(&flat, &email));
            }
        }
        Ok(out)
    })
}
