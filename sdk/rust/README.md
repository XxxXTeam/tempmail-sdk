# tempmail-sdk (Rust)

[![crates.io](https://img.shields.io/crates/v/tempmail-sdk.svg)](https://crates.io/crates/tempmail-sdk)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

临时邮箱 SDK（Rust），支持 11 个邮箱服务提供商，所有渠道返回**统一标准化格式**。

## 安装

```toml
# 从 crates.io
[dependencies]
tempmail-sdk = "1.1.0"

# 从 GitHub
[dependencies]
tempmail-sdk = { git = "https://github.com/XxxXTeam/tempmail-sdk", subdirectory = "sdk/rust" }
```

## 支持的渠道

| 渠道 | 服务商 | 需要 Token | 说明 |
|------|--------|:----------:|------|
| `Tempmail` | tempmail.ing | - | 支持自定义有效期 |
| `LinshiEmail` | linshi-email.com | - | |
| `TempmailLol` | tempmail.lol | ✅ | 支持指定域名 |
| `ChatgptOrgUk` | mail.chatgpt.org.uk | - | |
| `TempmailLa` | tempmail.la | - | 支持分页 |
| `TempMailIO` | temp-mail.io | - | |
| `Awamail` | awamail.com | ✅ | Session Cookie 自动管理 |
| `MailTm` | mail.tm | ✅ | 自动注册账号获取 Bearer Token |
| `Dropmail` | dropmail.me | ✅ | GraphQL API |
| `GuerrillaMail` | guerrillamail.com | ✅ | 公开 JSON API |
| `Maildrop` | maildrop.cc | ✅ | GraphQL API，自带反垃圾 |

## 快速开始

```rust
use tempmail_sdk::{generate_email, get_emails, GenerateEmailOptions, GetEmailsOptions, Channel};

fn main() {
    // 创建临时邮箱
    let opts = GenerateEmailOptions {
        channel: Some(Channel::GuerrillaMail),
        ..Default::default()
    };
    let info = generate_email(&opts).unwrap();
    println!("邮箱: {}", info.email);

    // 获取邮件
    let result = get_emails(&GetEmailsOptions {
        channel: info.channel,
        email: info.email,
        token: info.token,
        retry: None,
    });
    println!("success={} count={}", result.success, result.emails.len());
}
```

## 使用客户端类

```rust
use tempmail_sdk::{TempEmailClient, GenerateEmailOptions, Channel};

let mut client = TempEmailClient::new();
let info = client.generate(&GenerateEmailOptions {
    channel: Some(Channel::Maildrop),
    ..Default::default()
}).unwrap();
let result = client.get_emails().unwrap();
```

## 日志

需要配合 `env_logger` 或其他 `log` 实现：

```rust
env_logger::init();
tempmail_sdk::set_log_level(tempmail_sdk::LogLevelFilter::Debug);
```
