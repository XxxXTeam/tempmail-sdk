# tempmail-sdk (Rust)

[![crates.io](https://img.shields.io/crates/v/tempmail-sdk.svg)](https://crates.io/crates/tempmail-sdk)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

临时邮箱 SDK（Rust），支持 **55** 个邮箱服务提供商，所有渠道返回**统一标准化格式**。顺序与 `client.rs` 中 `ALL_CHANNELS` 一致，并与 Go / npm / Python / C 的 `listChannels` 顺序对齐。

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

| 渠道（`Channel`） | 服务商 | 需要 Token | 说明 |
|------|--------|:----------:|------|
| `Tempmail` | tempmail.ing | - | 支持自定义有效期 |
| `TempmailCn` | tempmail.cn | - | Socket.IO：`request shortid` / `set shortid` / `mail`；`GenerateEmailOptions.domain` 可指定自定义接入域名 |
| `TaEasy` | ta-easy.com | ✅ | REST `api-endpoint.ta-easy.com` |
| `TenminuteOne` | 10minutemail.one | ✅ | SSR / JWT + Web API；`GenerateEmailOptions.domain` 可选 |
| `Linshiyou` | linshiyou.com | ✅ | `NEXUS_TOKEN` + Cookie；HTML 分段解析 |
| `Mffac` | mffac.com | ✅ | mailbox `id` token；`/api/emails/{id}` 详情补齐 `text` / `html`；REST 24h |
| `TempmailLol` | tempmail.lol | ✅ | 支持指定域名 |
| `ChatgptOrgUk` | mail.chatgpt.org.uk | ✅ | Inbox Token 等由 SDK 封装 |
| `TempMailIo` | temp-mail.io | ✅ | REST：动态读取 `mobileTestingHeader` 后调用 `api.internal.temp-mail.io/api/v3` |
| `MailCx` | mail.cx | ✅ | 匿名 Web API：`/v1/config` 获取系统域名，`/v1/inbox/{email}` 长轮询收信，内部保存 `X-Client-ID` |
| `Catchmail` | catchmail.io | - | 公开 REST：`/api/v1/mailbox?address=` + `/api/v1/message/{id}?mailbox=`；详情含 `body.text` / `body.html` |
| `CatchmailMailistry` | mailistry.com | - | Catchmail API 固定域名 `mailistry.com` |
| `CatchmailZeppost` | zeppost.com | - | Catchmail API 固定域名 `zeppost.com` |
| `Mailforspam` | mailforspam.com | - | 公开 REST：`/api/mailboxes/{email}/emails` + `/api/mailboxes/{email}/emails/{id}`；详情含 `body_text` / `body_html` |
| `MailforspamTempmailIo` | tempmail.io | - | MailForSpam API 固定域名 `tempmail.io` |
| `MailforspamDisposable` | disposable.email | - | MailForSpam API 固定域名 `disposable.email` |
| `Tempmailo` | tempmailo.com | ✅ | `GET /changemail` 建址，`POST /` 传 `mail` 拉信；返回对象直接含 `text` / `html` |
| `Tempmailc` | tempmailc.com | - | Public API：`GET /api/v1/new` 建址，`GET /api/v1/inbox` 拉列表，`GET /api/v1/message` 读取 `text` / `html` 正文 |
| `Mailnesia` | mailnesia.com | - | 任意 `{local}@mailnesia.com` 建址；HTML 列表 `tr.emailheader` + 详情 `text_plain_{id}` / `text_html_{id}` 正文 |
| `Throwawaymail` | throwawaymail.app | ✅ | Web API 建址并轮询收信；Token 由 SDK 内部维护 |
| `Inboxkitten` | inboxkitten.com | - | 公开 API 拉取收件箱列表与详情 |
| `Getnada` | getnada.net | ✅ | `POST /api/inbox/open` 建箱；`GET /api/inbox/messages` 列表；`GET /api/inbox/message` 详情含 `text_plain` / `html_sanitized` |
| `Mail123` | mail123.fr | - | `GET /api/v1/mailbox/new` 建址；`GET /api/v1/mailbox/{address}/messages?limit=50` 列表；详情含 `text` / `html` |
| `OneSecMail` | 1sec-mail.com | ✅ | CSRF + Cookie；`POST /get_messages` 拉列表；详情由 `content` / `html` 映射，缺失正文由 normalize 反向生成 |
| `Fakemail` | fakemail.net | ✅ | `/index/index` 建址，`/index/refresh` 拉列表，`/index/email` 详情；`telo` HTML 正文 |
| `Openinbox` | openinbox.io | ✅ | `POST /api/inbox` 建箱；`GET /emails/inbox/{id}` 列表；`GET /emails/{emailId}` 详情含 `textBody` / `htmlBody` |
| `Inboxes` | inboxes.com | - | 公开 v2：`GET /api/v2/domain` 域名，`GET /api/v2/inbox/{email}` 列表，`GET /api/v2/message/{uid}` 详情含 `text` / `html` |
| `Uncorreotemporal` | uncorreotemporal.com | ✅ | `POST /api/v1/mailboxes` 建箱；`X-Session-Token` 拉取列表和详情；详情含 `body_text` / `body_html` |
| `Awamail` | awamail.com | ✅ | Session Cookie 自动管理 |
| `MailTm` | mail.tm | ✅ | 自动注册账号获取 Bearer Token |
| `Dropmail` | dropmail.me | ✅ | GraphQL API |
| `GuerrillaMail` | guerrillamail.com | ✅ | 公开 JSON API |
| `GuerrillamailCom` | guerrillamail.com | ✅ | GuerrillaMail 裸域 JSON API 入口 |
| `Maildrop` | maildrop.cx | ✅ | REST：`suffixes.php` + `emails.php`；`description`→`text` |
| `SmailPw` | smail.pw | ✅ | `_root.data` + `__session`；解析以正则为主 |
| `Vip215` | vip.215.im | ✅ | `POST` 建箱 + WebSocket；无正文时 synthetic 兜底 |
| `FakeLegal` | fake.legal | - | `/api/domains` + `/api/inbox/new`；`GenerateEmailOptions.domain` 可选 |
| `Moakt` | moakt.com | ✅ | HTML 收件箱 + `tm_session`；`http_client_no_cookie_jar()`；`GenerateEmailOptions.domain` 可选语言路径 |
| `Email10min` | email10min.com | ✅ | Cookie + CSRF；`POST /messages` 获取邮箱与邮件 |
| `MjjCm` | mjj.cm | - | Socket.IO：`request shortid` / `set shortid` / `mail` |
| `LinshiCo` | linshi.co | - | Socket.IO 克隆站，协议同 `MjjCm` |
| `Harakirimail` | harakirimail.com | - | 公开 REST：`GET /api/v1/inbox/{name}` + `GET /api/v1/email/{id}` |
| `TempmailPlus` | tempmail.plus | - | 公开 REST：`GET /api/mails/?email=` 列表，`GET /api/mails/{id}?email=` 详情 |
| `MailGw` | mail.gw | ✅ | 自动注册账号获取 Bearer Token |
| `TempmailLolV2` | tempmail.lol | ✅ | `GET /generate` 返回 address+token，`GET /auth/{token}` 拉取收件箱 |
| `Sharklasers` | sharklasers.com | ✅ | GuerrillaMail 镜像，API 与 `GuerrillaMail` 相同 |
| `SharklasersCom` | sharklasers.com | ✅ | GuerrillaMail 裸域镜像，API 与 `GuerrillaMail` 相同 |
| `GrrLa` | grr.la | ✅ | GuerrillaMail 镜像，API 与 `GuerrillaMail` 相同 |
| `GrrLaCom` | grr.la | ✅ | GuerrillaMail 裸域镜像，API 与 `GuerrillaMail` 相同 |
| `GuerrillamailInfo` | guerrillamail.info | ✅ | GuerrillaMail 镜像，API 与 `GuerrillaMail` 相同 |
| `Spam4me` | spam4.me | ✅ | GuerrillaMail 镜像，API 与 `GuerrillaMail` 相同 |
| `GuerrillamailNet` | guerrillamail.net | ✅ | GuerrillaMail 镜像，API 与 `GuerrillaMail` 相同 |
| `GuerrillamailOrg` | guerrillamail.org | ✅ | GuerrillaMail 镜像，API 与 `GuerrillaMail` 相同 |
| `Guerrillamailblock` | guerrillamailblock.com | ✅ | GuerrillaMail 镜像，API 与 `GuerrillaMail` 相同 |
| `GuerrillamailComWww` | guerrillamail.com | ✅ | GuerrillaMail `www` JSON API 入口 |

## 快速开始

```rust
use tempmail_sdk::{generate_email, get_emails, GenerateEmailOptions, Channel};

fn main() {
    let opts = GenerateEmailOptions {
        channel: Some(Channel::GuerrillaMail),
        ..Default::default()
    };
    if let Some(info) = generate_email(&opts) {
        println!("邮箱: {}", info.email);
        // channel / email / token 由 SDK 从 EmailInfo 读取
        let result = get_emails(&info, None);
        println!("success={} count={}", result.success, result.emails.len());
    }
}
```

## 使用客户端类

```rust
use tempmail_sdk::{TempEmailClient, GenerateEmailOptions, Channel};

let mut client = TempEmailClient::new();
if client.generate(&GenerateEmailOptions {
    channel: Some(Channel::Maildrop),
    ..Default::default()
}).is_some() {
    let _result = client.get_emails(None).expect("get_emails");
}
```

## 代理与 HTTP 配置

SDK 支持全局配置代理、超时等 HTTP 客户端参数，也可通过环境变量零代码配置：

```rust
use tempmail_sdk::{set_config, SDKConfig};

// 一行跳过 SSL 验证
set_config(SDKConfig { insecure: true, ..Default::default() });

// 设置代理
set_config(SDKConfig {
    proxy: Some("http://127.0.0.1:7890".into()),
    ..Default::default()
});

// 设置代理 + 超时 + 跳过 SSL 验证
set_config(SDKConfig {
    proxy: Some("socks5://127.0.0.1:1080".into()),
    timeout_secs: 30,
    insecure: true,
});
```

**配置项（`SDKConfig`）：**

| 字段 | 类型 | 说明 |
|------|------|------|
| `proxy` | `Option<String>` | 代理 URL（http/https/socks5） |
| `timeout_secs` | `u64` | 全局超时秒数，默认 15 |
| `insecure` | `bool` | 跳过 SSL 验证（调试用） |
| `dropmail_auth_token` | `Option<String>` | DropMail `af_` 令牌；未设则自动申请 |
| `dropmail_disable_auto_token` | `bool` | 为 true 时不自动 generate/renew |
| `dropmail_renew_lifetime` | `Option<String>` | renew 的 lifetime，如 `1d` |
| `telemetry_enabled` | `Option<bool>` | `None` 默认开启匿名遥测；`Some(false)` 关闭 |
| `telemetry_url` | `Option<String>` | 非空时覆盖默认上报 URL |

**环境变量（无需修改代码）：**

```bash
export TEMPMAIL_PROXY="http://127.0.0.1:7890"
export TEMPMAIL_INSECURE=1
export TEMPMAIL_TIMEOUT=30
export DROPMAIL_AUTH_TOKEN="af_..."
export DROPMAIL_NO_AUTO_TOKEN=1
export DROPMAIL_RENEW_LIFETIME=1d
export TEMPMAIL_TELEMETRY_ENABLED=false
export TEMPMAIL_TELEMETRY_URL="https://example.com/v1/event"
```

## 匿名遥测

默认 **开启**：批量上报匿名事件（`schema_version: 2`），默认 URL 见 `src/telemetry.rs`。关闭：`TEMPMAIL_TELEMETRY_ENABLED=false` 或 `set_config(SDKConfig { telemetry_enabled: Some(false), .. })`；改 URL：`TEMPMAIL_TELEMETRY_URL` 或 `telemetry_url`。

## HTTP 重试

`GenerateEmailOptions` 与 `GetEmailsOptions` 均支持 `retry: Option<RetryConfig>`（最大次数、初始延迟等），与 npm / Go / Python 行为一致。

## 日志

需要配合 `env_logger` 或其他 `log` 实现：

```rust
env_logger::init();
tempmail_sdk::set_log_level(tempmail_sdk::LogLevelFilter::Debug);
```
