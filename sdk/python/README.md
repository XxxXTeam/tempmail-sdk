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

**配置项：**

| 参数 | 类型 | 说明 |
|------|------|------|
| `proxy` | `str` | 代理 URL（http/https/socks5） |
| `timeout` | `int` | 全局超时秒数，默认 15 |
| `insecure` | `bool` | 跳过 SSL 验证（调试用） |
| `headers` | `dict` | 自定义请求头 |

**环境变量（无需修改代码）：**

```bash
export TEMPMAIL_PROXY="http://127.0.0.1:7890"
export TEMPMAIL_INSECURE=1
export TEMPMAIL_TIMEOUT=30
```

## 重试配置

```python
from tempmail_sdk import generate_email, GenerateEmailOptions, RetryConfig

info = generate_email(GenerateEmailOptions(
    channel="temp-mail-io",
    retry=RetryConfig(max_retries=3, initial_delay=2.0),
))
```
