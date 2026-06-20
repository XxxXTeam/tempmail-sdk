# tempmail-sdk (Python)

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

临时邮箱 SDK（Python），支持 **55** 个邮箱服务提供商，顺序与 `client.py` 中 `ALL_CHANNELS` 一致，返回格式与根目录 README 描述一致，并与 Go / npm / Rust / C 对齐。

## 安装

```bash
# 从 PyPI
pip install tempemail-sdk

# 从 GitHub Release（wheel）
pip install https://github.com/XxxXTeam/tempmail-sdk/releases/latest/download/tempemail_sdk-1.1.0-py3-none-any.whl
```

## 支持的渠道

| 渠道 | 服务商 | 需要 Token | 说明 |
|------|--------|:----------:|------|
| `tempmail` | tempmail.ing | - | 支持自定义有效期 |
| `tempmail-cn` | tempmail.cn | - | Socket.IO：`request shortid` / `set shortid` / `mail`；`GenerateEmailOptions.domain` 可指定自定义接入域名 |
| `ta-easy` | ta-easy.com | ✅ | REST `api-endpoint.ta-easy.com` |
| `10minute-one` | 10minutemail.one | ✅ | SSR / JWT + Web API；`GenerateEmailOptions.domain` 可选 |
| `linshiyou` | linshiyou.com | ✅ | `NEXUS_TOKEN` + Cookie；HTML 分段解析 |
| `mffac` | mffac.com | ✅ | mailbox `id`；`/api/emails/{id}` 详情补齐 `text` / `html`；REST 24h |
| `tempmail-lol` | tempmail.lol | ✅ | 支持指定域名 |
| `chatgpt-org-uk` | mail.chatgpt.org.uk | ✅ | Inbox Token 等由 SDK 封装 |
| `temp-mail-io` | temp-mail.io | ✅ | REST：动态读取 `mobileTestingHeader` 后调用 `api.internal.temp-mail.io/api/v3` |
| `mail-cx` | mail.cx | ✅ | 匿名 Web API：`/v1/config` 获取系统域名，`/v1/inbox/{email}` 长轮询收信，内部保存 `X-Client-ID` |
| `catchmail` | catchmail.io | - | 公开 REST：`/api/v1/mailbox?address=` + `/api/v1/message/{id}?mailbox=`；详情含 `body.text` / `body.html` |
| `catchmail-mailistry` | mailistry.com | - | Catchmail API 固定域名 `mailistry.com` |
| `catchmail-zeppost` | zeppost.com | - | Catchmail API 固定域名 `zeppost.com` |
| `mailforspam` | mailforspam.com | - | 公开 REST：`/api/mailboxes/{email}/emails` + `/api/mailboxes/{email}/emails/{id}`；详情含 `body_text` / `body_html` |
| `mailforspam-tempmail-io` | tempmail.io | - | MailForSpam API 固定域名 `tempmail.io` |
| `mailforspam-disposable` | disposable.email | - | MailForSpam API 固定域名 `disposable.email` |
| `tempmailo` | tempmailo.com | ✅ | `GET /changemail` 建址，`POST /` 传 `mail` 拉信；返回对象直接含 `text` / `html` |
| `tempmailc` | tempmailc.com | - | Public API：`GET /api/v1/new` 建址，`GET /api/v1/inbox` 拉列表，`GET /api/v1/message` 读取 `text` / `html` 正文 |
| `mailnesia` | mailnesia.com | - | 任意 `{local}@mailnesia.com` 建址；HTML 列表 `tr.emailheader` + 详情 `text_plain_{id}` / `text_html_{id}` 正文 |
| `throwawaymail` | throwawaymail.app | ✅ | Web API 建址并轮询收信；Token 由 SDK 内部维护 |
| `inboxkitten` | inboxkitten.com | - | 公开 API 拉取收件箱列表与详情 |
| `getnada` | getnada.net | ✅ | `POST /api/inbox/open` 建箱；`GET /api/inbox/messages` 列表；`GET /api/inbox/message` 详情含 `text_plain` / `html_sanitized` |
| `mail123` | mail123.fr | - | `GET /api/v1/mailbox/new` 建址；`GET /api/v1/mailbox/{address}/messages?limit=50` 列表；详情含 `text` / `html` |
| `1sec-mail` | 1sec-mail.com | ✅ | CSRF + Cookie；`POST /get_messages` 拉列表；详情由 `content` / `html` 映射，缺失正文由 normalize 反向生成 |
| `fakemail` | fakemail.net | ✅ | `/index/index` 建址，`/index/refresh` 拉列表，`/index/email` 详情；`telo` HTML 正文 |
| `openinbox` | openinbox.io | ✅ | `POST /api/inbox` 建箱；`GET /emails/inbox/{id}` 列表；`GET /emails/{emailId}` 详情含 `textBody` / `htmlBody` |
| `inboxes` | inboxes.com | - | 公开 v2：`GET /api/v2/domain` 域名，`GET /api/v2/inbox/{email}` 列表，`GET /api/v2/message/{uid}` 详情含 `text` / `html` |
| `uncorreotemporal` | uncorreotemporal.com | ✅ | `POST /api/v1/mailboxes` 建箱；`X-Session-Token` 拉取列表和详情；详情含 `body_text` / `body_html` |
| `awamail` | awamail.com | ✅ | Session Cookie 自动管理 |
| `mail-tm` | mail.tm | ✅ | 自动注册账号获取 Bearer Token |
| `dropmail` | dropmail.me | ✅ | GraphQL API |
| `guerrillamail` | guerrillamail.com | ✅ | 公开 JSON API |
| `guerrillamail-com` | guerrillamail.com | ✅ | GuerrillaMail 裸域 JSON API 入口 |
| `maildrop` | maildrop.cx | ✅ | REST：`suffixes.php` + `emails.php`；`description`→`text` |
| `smail-pw` | smail.pw | ✅ | `__session` Cookie；正则 + JSON 遍历解析 Flight 中的邮件行对象 |
| `vip-215` | vip.215.im | ✅ | `POST` 建箱 + WebSocket；无正文时 synthetic 兜底 |
| `fake-legal` | fake.legal | - | `/api/domains` + `/api/inbox/new`；可选 `GenerateEmailOptions.domain` |
| `moakt` | moakt.com | ✅ | HTML 收件箱 + `tm_session`；`domain` 可选语言路径；独立 `requests` 请求避免污染全局 Session Cookie |
| `email10min` | email10min.com | ✅ | Cookie + CSRF；`POST /messages` 获取邮箱与邮件 |
| `mjj-cm` | mjj.cm | ✅ | Socket.IO：`request shortid` / `set shortid` / `mail` |
| `linshi-co` | linshi.co | ✅ | Socket.IO 克隆站，协议同 `mjj-cm` |
| `harakirimail` | harakirimail.com | - | 公开 REST：`GET /api/v1/inbox/{name}` + `GET /api/v1/email/{id}` |
| `tempmail-plus` | tempmail.plus | - | 公开 REST：`GET /api/mails/?email=` 列表，`GET /api/mails/{id}?email=` 详情 |
| `mail-gw` | mail.gw | ✅ | 自动注册账号获取 Bearer Token |
| `tempmail-lol-v2` | tempmail.lol | ✅ | `GET /generate` 返回 address+token，`GET /auth/{token}` 拉取收件箱 |
| `sharklasers` | sharklasers.com | ✅ | GuerrillaMail 镜像，API 与 `guerrillamail` 相同 |
| `sharklasers-com` | sharklasers.com | ✅ | GuerrillaMail 裸域镜像，API 与 `guerrillamail` 相同 |
| `grr-la` | grr.la | ✅ | GuerrillaMail 镜像，API 与 `guerrillamail` 相同 |
| `grr-la-com` | grr.la | ✅ | GuerrillaMail 裸域镜像，API 与 `guerrillamail` 相同 |
| `guerrillamail-info` | guerrillamail.info | ✅ | GuerrillaMail 镜像，API 与 `guerrillamail` 相同 |
| `spam4me` | spam4.me | ✅ | GuerrillaMail 镜像，API 与 `guerrillamail` 相同 |
| `guerrillamail-net` | guerrillamail.net | ✅ | GuerrillaMail 镜像，API 与 `guerrillamail` 相同 |
| `guerrillamail-org` | guerrillamail.org | ✅ | GuerrillaMail 镜像，API 与 `guerrillamail` 相同 |
| `guerrillamailblock` | guerrillamailblock.com | ✅ | GuerrillaMail 镜像，API 与 `guerrillamail` 相同 |
| `guerrillamail-com-www` | guerrillamail.com | ✅ | GuerrillaMail `www` JSON API 入口 |

## 快速开始

```python
from tempmail_sdk import generate_email, get_emails, GenerateEmailOptions

# 创建临时邮箱
info = generate_email(GenerateEmailOptions(channel="guerrillamail"))
if info:
    print(f"邮箱: {info.email}")

    # 获取邮件（channel / email / token 由 SDK 从 EmailInfo 读取，无需手动传入）
    result = get_emails(info)
    if result.success:
        print(f"收到 {len(result.emails)} 封邮件")
```

## 使用客户端类

```python
from tempmail_sdk import TempEmailClient, GenerateEmailOptions

client = TempEmailClient()
info = client.generate(GenerateEmailOptions(channel="maildrop"))
result = client.get_emails()
```

## 日志

```python
from tempmail_sdk import set_log_level, LOG_DEBUG

set_log_level(LOG_DEBUG)  # 开启所有日志
```

## 代理与 HTTP 配置

SDK 支持全局配置代理、超时等 HTTP 客户端参数，也可通过环境变量零代码配置：

```python
from tempmail_sdk import set_config

# 一行跳过 SSL 验证
set_config(insecure=True)

# 设置代理
set_config(proxy="http://127.0.0.1:7890")

# 设置代理 + 超时 + 跳过 SSL 验证
set_config(proxy="socks5://127.0.0.1:1080", timeout=30, insecure=True)

# 添加自定义请求头
set_config(headers={"X-Custom": "value"})
```

**配置项（`SDKConfig` / `set_config`）：**

| 参数 | 类型 | 说明 |
|------|------|------|
| `proxy` | `str` | 代理 URL（http/https/socks5） |
| `timeout` | `int` | 全局超时秒数，默认 15 |
| `insecure` | `bool` | 跳过 SSL 验证（调试用） |
| `headers` | `dict` | 自定义请求头 |
| `dropmail_auth_token` 等 | 见 `config.py` | DropMail `DROPMAIL_*` 环境变量亦可 |
| `telemetry_enabled` | `Optional[bool]` | `None` 默认开启；`False` 关闭匿名遥测 |
| `telemetry_url` | `Optional[str]` | 覆盖默认上报 URL |

**环境变量（无需修改代码）：**

```bash
export TEMPMAIL_PROXY="http://127.0.0.1:7890"
export TEMPMAIL_INSECURE=1
export TEMPMAIL_TIMEOUT=30
export DROPMAIL_AUTH_TOKEN="af_..."
export DROPMAIL_NO_AUTO_TOKEN=1
export TEMPMAIL_TELEMETRY_ENABLED=false
export TEMPMAIL_TELEMETRY_URL="https://example.com/v1/event"
```

## 匿名遥测

默认 **开启**：批量 `POST` 匿名事件（`schema_version: 2`），内置默认 URL 见 `telemetry.py`。关闭：`TEMPMAIL_TELEMETRY_ENABLED=false`（或 `0` / `no`）或 `set_config(telemetry_enabled=False)`；改 URL：`TEMPMAIL_TELEMETRY_URL` 或 `telemetry_url`。

## 重试配置

```python
from tempmail_sdk import generate_email, GenerateEmailOptions, RetryConfig

info = generate_email(GenerateEmailOptions(
    channel="mail-gw",
    retry=RetryConfig(max_retries=3, initial_delay=2.0),
))
```

拉取邮件同样可传 `GetEmailsOptions(retry=RetryConfig(...))`。
