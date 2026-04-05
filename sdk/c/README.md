# tempmail-sdk (C)

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

临时邮箱 SDK（C），支持 **25** 个邮箱服务提供商，所有渠道返回**统一标准化格式**。下列顺序与 `client.c` 中 `g_channel_infos` / `tm_list_channels` 及 `tempmail_sdk.h` 中 `tm_channel_t` **枚举下标**一致（与 Go 的 `allChannels` 顺序不同，且 **无** `tempmailg`，与 npm/Rust/Python 一致，属正常）。

## 依赖

- **libcurl** - HTTP 请求
- **cJSON** - JSON 解析（需下载到 `vendor/` 目录）

## 安装

### 预编译包（推荐）

从 [GitHub Releases](https://github.com/XxxXTeam/tempmail-sdk/releases) 下载对应平台的 zip 包：

| 包名 | 平台 |
|------|------|
| `c-sdk-linux-amd64.zip` | Linux x64 |
| `c-sdk-darwin-arm64.zip` | macOS ARM64 |
| `c-sdk-windows-amd64.zip` | Windows x64 |

解压后包含 `include/`、`lib/`、`bin/` 目录，可直接链接使用。

### 源码构建

```bash
# 1. 下载 cJSON
cd vendor
curl -L -o cJSON.h https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.h
curl -L -o cJSON.c https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.c
cd ..

# 2. CMake 构建
cmake -B build -S .
cmake --build build
```

## 支持的渠道

| 渠道 | 枚举值 | 服务商 | 需要 Token | 说明 |
|------|--------|--------|:----------:|------|
| tempmail | `CHANNEL_TEMPMAIL` | tempmail.ing | - | 支持自定义有效期 |
| linshi-email | `CHANNEL_LINSHI_EMAIL` | linshi-email.com | - | |
| linshiyou | `CHANNEL_LINSHIYOU` | linshiyou.com | ✅ | `NEXUS_TOKEN` + Cookie；HTML 分段解析 |
| tempmail-lol | `CHANNEL_TEMPMAIL_LOL` | tempmail.lol | ✅ | 支持指定域名 |
| chatgpt-org-uk | `CHANNEL_CHATGPT_ORG_UK` | mail.chatgpt.org.uk | ✅ | Inbox Token 等由 SDK 封装 |
| temp-mail-io | `CHANNEL_TEMP_MAIL_IO` | temp-mail.io | - | |
| awamail | `CHANNEL_AWAMAIL` | awamail.com | ✅ | Session Cookie 自动管理 |
| mail-tm | `CHANNEL_MAIL_TM` | mail.tm | ✅ | 自动注册，Bearer Token |
| mail-cx | `CHANNEL_MAIL_CX` | mail.cx | ✅ | `api.mail.cx` OpenAPI；`tm_generate_options_t.domain` 可选 |
| dropmail | `CHANNEL_DROPMAIL` | dropmail.me | ✅ | GraphQL |
| guerrillamail | `CHANNEL_GUERRILLAMAIL` | guerrillamail.com | ✅ | 公开 JSON API |
| maildrop | `CHANNEL_MAILDROP` | maildrop.cx | ✅ | REST：`suffixes.php` + `emails.php`；`description`→`text` |
| smail-pw | `CHANNEL_SMAIL_PW` | smail.pw | ✅ | `_root.data` + `__session` |
| boomlify | `CHANNEL_BOOMLIFY` | boomlify.com | - | `domains/public` + `emails/public/create`，`{UUID}@{域名}` |
| minmail | `CHANNEL_MINMAIL` | minmail.app | ✅ | Token 为 JSON（visitorId + ck） |
| vip-215 | `CHANNEL_VIP_215` | vip.215.im | ✅ | WebSocket 收信 |
| temporary-email-org | `CHANNEL_TEMPORARY_EMAIL_ORG` | temporary-email.org | ✅ | `GET /zh/messages` Cookie + XHR |
| anonbox | `CHANNEL_ANONBOX` | anonbox.net | ✅ | `GET /en/` 解析 HTML + mbox 收信 |
| fake-legal | `CHANNEL_FAKE_LEGAL` | fake.legal | - | `/api/domains` + `/api/inbox/new`；`tm_generate_options_t.domain` 可选 |
| mffac | `CHANNEL_MFFAC` | mffac.com | ✅ | mailbox `id`；REST 24h |
| tempmail-cn | `CHANNEL_TEMPMAIL_CN` | tempmail.cn | - | Socket.IO：`request shortid` / `set shortid` / `mail`；`tm_generate_options_t.domain` 可指定自定义接入域名 |
| ta-easy | `CHANNEL_TA_EASY` | ta-easy.com | ✅ | REST `api-endpoint.ta-easy.com`；需 `info->token` 拉信 |
| tmpmails | `CHANNEL_TMPMAILS` | tmpmails.com | ✅ | Next.js Server Action；`domain` 可选语言路径 |
| 10mail-wangtz | `CHANNEL_10MAIL_WANGTZ` | 10mail.wangtz.cn | - | REST `/api/tempMail`、`/api/emailList`；`tm_http_request_insecure` 跳过证书校验 |
| moakt | `CHANNEL_MOAKT` | moakt.com | ✅ | HTML 收件箱 + `tm_session`；`domain` 可选语言路径（如 `zh`） |

## 快速开始

```c
#include <stdio.h>
#include "tempmail_sdk.h"

int main(void) {
    tm_init();
    tm_set_log_level(TM_LOG_INFO);

    // 创建临时邮箱
    tm_generate_options_t opts = {0};
    opts.channel = CHANNEL_GUERRILLAMAIL;
    tm_email_info_t *info = tm_generate_email(&opts);

    if (info) {
        printf("邮箱: %s\n", info->email);

        /* 获取邮件：渠道 / 邮箱 / token 均从 tm_email_info_t 读取 */
        tm_get_emails_result_t *result = tm_get_emails(info, NULL);
        if (result) {
            printf("邮件数: %d\n", result->email_count);
            tm_free_get_emails_result(result);
        }

        tm_free_email_info(info);
    }

    tm_cleanup();
    return 0;
}
```

## 代理与 HTTP 配置

SDK 支持全局配置代理、超时等 HTTP 客户端参数，也可通过环境变量零代码配置：

```c
#include "tempmail_sdk.h"

// 一行跳过 SSL 验证
tm_config_t c1 = {0}; c1.insecure = 1; tm_set_config(&c1);

// 设置代理
tm_config_t config = {0};
config.proxy = "http://127.0.0.1:7890";
tm_set_config(&config);

// 设置代理 + 跳过 SSL 验证
tm_config_t config2 = {0};
config2.proxy = "socks5://127.0.0.1:1080";
config2.insecure = 1;
tm_set_config(&config2);
```

**配置项（`tm_config_t`）：**

| 字段 | 类型 | 说明 |
|------|------|------|
| `proxy` | `const char*` | 代理 URL（http/https/socks5） |
| `timeout_secs` | `int` | 全局超时秒数，默认 15 |
| `insecure` | `bool` | 跳过 SSL 验证（调试用） |
| `telemetry_enabled` | `bool` | 默认 `true`：发送匿名用量上报；`false` 关闭 |
| `telemetry_url` | `const char*` | 非 NULL 时作为上报 URL；NULL 则用环境变量或内置默认 |

**环境变量（无需修改代码，`tm_init()` 时自动读取）：**

```bash
export TEMPMAIL_PROXY="http://127.0.0.1:7890"
export TEMPMAIL_INSECURE=1
export TEMPMAIL_TIMEOUT=30
export TEMPMAIL_TELEMETRY_ENABLED=false
export TEMPMAIL_TELEMETRY_URL="https://example.com/v1/event"
```

## 匿名遥测

默认 **开启**：事件在 SDK 内排队后批量上报（内置默认 URL 见 `telemetry.c`）。`tm_set_config` 可设置 `telemetry_enabled` / `telemetry_url`；`tm_cleanup()` 时会尽量刷完队列。关闭：环境变量 `TEMPMAIL_TELEMETRY_ENABLED=false`（或 `0` / `no`）或配置 `telemetry_enabled = false`。

## HTTP 重试

`tm_generate_options_t` 与 `tm_get_emails_options_t` 均可设置 `retry` 指向 `tm_retry_config_t`（如 `max_retries`、超时等），`NULL` 使用默认；详见 `tempmail_sdk.h`。

## 内存管理

所有返回的指针需要手动释放：
- `tm_free_email_info()` 释放邮箱信息
- `tm_free_get_emails_result()` 释放邮件结果
- `tm_free_email()` 释放单封邮件
