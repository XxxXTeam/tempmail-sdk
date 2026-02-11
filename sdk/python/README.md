# tempmail-sdk (Python)

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

临时邮箱 SDK（Python），支持 11 个邮箱服务提供商，所有渠道返回**统一标准化格式**。

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
| `linshi-email` | linshi-email.com | - | |
| `tempmail-lol` | tempmail.lol | ✅ | 支持指定域名 |
| `chatgpt-org-uk` | mail.chatgpt.org.uk | - | |
| `tempmail-la` | tempmail.la | - | 支持分页 |
| `temp-mail-io` | temp-mail.io | - | |
| `awamail` | awamail.com | ✅ | Session Cookie 自动管理 |
| `mail-tm` | mail.tm | ✅ | 自动注册账号获取 Bearer Token |
| `dropmail` | dropmail.me | ✅ | GraphQL API |
| `guerrillamail` | guerrillamail.com | ✅ | 公开 JSON API |
| `maildrop` | maildrop.cc | ✅ | GraphQL API，自带反垃圾 |

## 快速开始

```python
from tempmail_sdk import generate_email, get_emails, GenerateEmailOptions, GetEmailsOptions

# 创建临时邮箱
info = generate_email(GenerateEmailOptions(channel="guerrillamail"))
print(f"邮箱: {info.email}")

# 获取邮件
result = get_emails(GetEmailsOptions(
    channel=info.channel,
    email=info.email,
    token=info.token,
))
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

## 重试配置

```python
from tempmail_sdk import generate_email, GenerateEmailOptions, RetryConfig

info = generate_email(GenerateEmailOptions(
    channel="temp-mail-io",
    retry=RetryConfig(max_retries=3, initial_delay=2.0),
))
```
