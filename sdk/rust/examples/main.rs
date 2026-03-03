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
        Some(info) => {
            println!("创建成功: {}", info.email);

            /* 获取邮件 */
            let result = get_emails(&info, None);
            println!("获取邮件 success={} count={}", result.success, result.emails.len());
            for (i, email) in result.emails.iter().enumerate() {
                println!("  [{}] from={} subject={}", i, email.from_addr, email.subject);
            }
        }
        None => {
            eprintln!("创建邮箱失败: 所有渠道均不可用");
        }
    }
}
