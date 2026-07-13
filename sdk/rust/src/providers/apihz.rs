/*!
 * apihz（接口盒子）渠道实现
 * 网站: https://apihz.cn
 *
 * 需 id/key 凭据，内置官方公共账号 88888888/88888888（共享频次）作为默认，
 * 可用环境变量 APIHZ_ID / APIHZ_KEY 或 config 覆盖。
 *
 *   1. 创建: GET /api/mail/mailcache.php?id=&key=&domain=&name=&pwd=&buytype=0
 *      返回 {"code":200,"mail":"...","pwd":"...","endtime":"..."}
 *   2. 读信: GET /api/mail/mailgetlist.php?id=&key=&mail=&pwd=&page=1
 *      返回 {"code":200,"num":"N","data":[{frommail,fromname,subject,time1,time2,content}]}
 *
 * 邮箱有效期 10 分钟，读信必须携带创建时的 pwd。
 * token = "apihz1:" + base64(JSON{mail,pwd})，同时保存 mail + pwd。
 */

use base64::Engine;
use rand::Rng;
use serde_json::Value;

use crate::config::{block_on, get_config, get_current_ua, http_client};
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};

const BASE_URL: &str = "https://cn.apihz.cn";
const TOK_PREFIX: &str = "apihz1:";

/// apihz 官方公共账号（共享频次），未配置自有 id/key 时使用
const PUBLIC_ID: &str = "88888888";
const PUBLIC_KEY: &str = "88888888";

/// 可用收信域名（apihz 自研，MX 指向 mail.apimail.* 自建服务器）
const DOMAINS: [&str; 2] = ["apimail.email", "apimail.vip"];

/// 读取 apihz 调用凭据：优先配置/环境变量，回退官方公共账号
fn get_credentials() -> (String, String) {
    let cfg = get_config();
    let id = cfg
        .apihz_id
        .filter(|s| !s.trim().is_empty())
        .unwrap_or_else(|| PUBLIC_ID.to_string());
    let key = cfg
        .apihz_key
        .filter(|s| !s.trim().is_empty())
        .unwrap_or_else(|| PUBLIC_KEY.to_string());
    (id, key)
}

/// 生成随机本地部分（小写字母 + 数字）
fn random_local(length: usize) -> String {
    let chars: Vec<char> = "abcdefghijklmnopqrstuvwxyz0123456789".chars().collect();
    let mut rng = rand::thread_rng();
    (0..length)
        .map(|_| chars[rng.gen_range(0..chars.len())])
        .collect()
}

/// 生成 12 位随机密码（读信必须携带创建时的密码）
fn random_password() -> String {
    let chars: Vec<char> = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
        .chars()
        .collect();
    let mut rng = rand::thread_rng();
    (0..12)
        .map(|_| chars[rng.gen_range(0..chars.len())])
        .collect()
}

/// token 编码：前缀 + base64(JSON{mail,pwd})，读信时解出 pwd
fn enc_token(mail: &str, pwd: &str) -> String {
    let raw = serde_json::json!({ "mail": mail, "pwd": pwd }).to_string();
    format!(
        "{}{}",
        TOK_PREFIX,
        base64::engine::general_purpose::STANDARD.encode(raw.as_bytes())
    )
}

/// token 解码：去前缀 → base64 decode → JSON.parse，得到 (mail, pwd)
fn dec_token(tok: &str) -> Result<(String, String), String> {
    let body = tok
        .strip_prefix(TOK_PREFIX)
        .ok_or_else(|| "apihz: 无效 token".to_string())?;
    let raw = base64::engine::general_purpose::STANDARD
        .decode(body)
        .map_err(|_| "apihz: 无效 token".to_string())?;
    let text = String::from_utf8(raw).map_err(|_| "apihz: 无效 token".to_string())?;
    let obj: Value = serde_json::from_str(&text).map_err(|_| "apihz: 无效 token".to_string())?;
    let mail = obj
        .get("mail")
        .and_then(|v| v.as_str())
        .unwrap_or("")
        .trim()
        .to_string();
    let pwd = obj
        .get("pwd")
        .and_then(|v| v.as_str())
        .unwrap_or("")
        .trim()
        .to_string();
    if mail.is_empty() || pwd.is_empty() {
        return Err("apihz: 无效 token".to_string());
    }
    Ok((mail, pwd))
}

/// URL 查询参数编码
fn q(s: &str) -> String {
    urlencoding::encode(s).into_owned()
}

/// 创建 apihz（接口盒子）临时邮箱
pub fn generate_email() -> Result<EmailInfo, String> {
    let (id, key) = get_credentials();
    let domain = {
        let mut rng = rand::thread_rng();
        DOMAINS[rng.gen_range(0..DOMAINS.len())]
    };
    let name = random_local(10);
    let pwd = random_password();

    let url = format!(
        "{}/api/mail/mailcache.php?id={}&key={}&domain={}&name={}&pwd={}&buytype=0",
        BASE_URL,
        q(&id),
        q(&key),
        q(domain),
        q(&name),
        q(&pwd)
    );

    block_on(async {
        let resp = http_client()
            .get(&url)
            .header("User-Agent", get_current_ua())
            .header("Accept", "application/json")
            .send()
            .await
            .map_err(|e| format!("apihz: 创建邮箱请求失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("apihz: 创建邮箱失败 HTTP {}", resp.status()));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("apihz: 解析创建响应失败: {}", e))?;

        let code = data.get("code").and_then(|v| v.as_i64()).unwrap_or(0);
        let mail = data
            .get("mail")
            .and_then(|v| v.as_str())
            .unwrap_or("")
            .trim()
            .to_string();
        if code != 200 || mail.is_empty() {
            let msg = data
                .get("msg")
                .and_then(|v| v.as_str())
                .map(|s| s.to_string())
                .unwrap_or_else(|| data.to_string());
            return Err(format!("apihz: 创建邮箱失败 {}", msg));
        }

        /* 优先使用响应回传的 pwd（与请求一致），确保读信密码正确 */
        let final_pwd = data
            .get("pwd")
            .and_then(|v| v.as_str())
            .filter(|s| !s.trim().is_empty())
            .map(|s| s.to_string())
            .unwrap_or(pwd);

        Ok(EmailInfo {
            channel: Channel::Apihz,
            email: mail.clone(),
            token: Some(enc_token(&mail, &final_pwd)),
            expires_at: None,
            created_at: None,
        })
    })
}

/// 获取 apihz 邮件列表
pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    if token.is_empty() {
        return Err("apihz: 缺少 token".to_string());
    }
    let (mail, pwd) = dec_token(token)?;
    let (id, key) = get_credentials();
    let addr = if mail.is_empty() {
        email.to_string()
    } else {
        mail
    };

    let url = format!(
        "{}/api/mail/mailgetlist.php?id={}&key={}&mail={}&pwd={}&page=1",
        BASE_URL,
        q(&id),
        q(&key),
        q(&addr),
        q(&pwd)
    );

    block_on(async {
        let resp = http_client()
            .get(&url)
            .header("User-Agent", get_current_ua())
            .header("Accept", "application/json")
            .send()
            .await
            .map_err(|e| format!("apihz: 获取邮件请求失败: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("apihz: 获取邮件失败 HTTP {}", resp.status()));
        }

        let data: Value = resp
            .json()
            .await
            .map_err(|e| format!("apihz: 解析邮件响应失败: {}", e))?;

        let code = data.get("code").and_then(|v| v.as_i64()).unwrap_or(0);
        let list = match data.get("data").and_then(|v| v.as_array()) {
            Some(arr) if code == 200 => arr,
            _ => return Ok(vec![]),
        };

        let mut out = Vec::with_capacity(list.len());
        for (idx, msg) in list.iter().enumerate() {
            let time1 = json_str(msg.get("time1"));
            let id_val = if time1.is_empty() {
                idx.to_string()
            } else {
                time1.clone()
            };
            let from = {
                let fm = json_str(msg.get("frommail"));
                if fm.is_empty() {
                    json_str(msg.get("fromname"))
                } else {
                    fm
                }
            };
            let content = json_str(msg.get("content"));
            let date = {
                let t2 = json_str(msg.get("time2"));
                if t2.is_empty() {
                    time1.clone()
                } else {
                    t2
                }
            };
            let flat = serde_json::json!({
                "id": id_val,
                "from": from,
                "to": addr,
                "subject": json_str(msg.get("subject")),
                "text": content,
                "html": content,
                "date": date,
                "isRead": false,
            });
            out.push(normalize_email(&flat, &addr));
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
