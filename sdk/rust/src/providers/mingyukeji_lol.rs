/*!
 * mailmomy 域名变体 mingyukeji.lol
 * 独立自研后端，收信复用 mailmomy provider 的读信逻辑。
 */

use crate::providers::mailmomy;
use crate::types::{Channel, Email, EmailInfo};
use rand::Rng;

const DOMAIN: &str = "mingyukeji.lol";

/// 生成随机本地部分（小写字母 + 数字）
fn random_local(len: usize) -> String {
    const CHARS: &[u8] = b"abcdefghijklmnopqrstuvwxyz0123456789";
    let mut rng = rand::thread_rng();
    (0..len)
        .map(|_| CHARS[rng.gen_range(0..CHARS.len())] as char)
        .collect()
}

/// 创建 mingyukeji.lol 临时邮箱（免注册，token 即邮箱地址本身）
pub fn generate_email() -> Result<EmailInfo, String> {
    let email = format!("{}@{}", random_local(10), DOMAIN);
    Ok(EmailInfo {
        channel: Channel::MingyukejiLol,
        email: email.clone(),
        token: Some(email),
        expires_at: None,
        created_at: None,
    })
}

/// 获取 mingyukeji.lol 邮件列表（复用 mailmomy 读信逻辑）
pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    mailmomy::get_emails(email)
}
