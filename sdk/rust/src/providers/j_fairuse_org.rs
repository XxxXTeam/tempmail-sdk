/*!
 * Mailinator 姊妹域名 j.fairuse.org
 * MX 指向 mail.mailinator.com，收信复用 mailinator provider 的读信逻辑。
 */

use crate::providers::mailinator;
use crate::types::{Channel, Email, EmailInfo};
use rand::Rng;

const DOMAIN: &str = "j.fairuse.org";

/// 生成随机本地部分（小写字母 + 数字）
fn random_local(len: usize) -> String {
    const CHARS: &[u8] = b"abcdefghijklmnopqrstuvwxyz0123456789";
    let mut rng = rand::thread_rng();
    (0..len)
        .map(|_| CHARS[rng.gen_range(0..CHARS.len())] as char)
        .collect()
}

/// 创建 j.fairuse.org 临时邮箱（Mailinator 姊妹域名，无需 token）
pub fn generate_email() -> Result<EmailInfo, String> {
    Ok(EmailInfo {
        channel: Channel::JFairuseOrg,
        email: format!("{}@{}", random_local(12), DOMAIN),
        token: None,
        expires_at: None,
        created_at: None,
    })
}

/// 获取 j.fairuse.org 邮件列表（复用 mailinator 读信逻辑）
pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    mailinator::get_emails(email)
}
