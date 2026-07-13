/*!
 * Mailinator 官方姊妹域名 nospam.thurstons.us
 * MX 指向 mail.mailinator.com，收信复用 mailinator provider 的读信逻辑。
 */

use crate::providers::mailinator;
use crate::types::{Channel, Email, EmailInfo};
use rand::Rng;

const DOMAIN: &str = "nospam.thurstons.us";

fn random_string(len: usize) -> String {
    const CHARS: &[u8] = b"abcdefghijklmnopqrstuvwxyz0123456789";
    let mut rng = rand::thread_rng();
    (0..len)
        .map(|_| CHARS[rng.gen_range(0..CHARS.len())] as char)
        .collect()
}

pub fn generate_email() -> Result<EmailInfo, String> {
    let local = random_string(12);
    Ok(EmailInfo {
        channel: Channel::NospamThurstonsUs,
        email: format!("{}@{}", local, DOMAIN),
        token: None,
        expires_at: None,
        created_at: None,
    })
}

pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    mailinator::get_emails(email)
}
