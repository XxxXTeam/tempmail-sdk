/**
 * 示例: 使用 Rust SDK 创建临时邮箱并获取邮件
 */

use tempmail_sdk::*;

fn main() {
    set_log_level(LogLevelFilter::Info);

    /* 创建临时邮箱 */
    let options = GenerateEmailOptions {
        channel: Some(Channel::GuerrillaMail),
        ..Default::default()
    };

    println!("正在创建临时邮箱...");
    match generate_email(&options) {
        Ok(info) => {
            println!("创建成功: {}", info.email);
            println!("Token: {}", info.token.as_deref().unwrap_or("(none)"));

            /* 获取邮件 */
            let get_opts = GetEmailsOptions {
                channel: info.channel.clone(),
                email: info.email.clone(),
                token: info.token.clone(),
                retry: None,
            };

            let result = get_emails(&get_opts);
            println!("获取邮件 success={} count={}", result.success, result.emails.len());
            for (i, email) in result.emails.iter().enumerate() {
                println!("  [{}] from={} subject={}", i, email.from_addr, email.subject);
            }
        }
        Err(e) => {
            eprintln!("创建邮箱失败: {}", e);
        }
    }
}
