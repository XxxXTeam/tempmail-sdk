# tempmail-sdk (C)

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

临时邮箱 SDK（C），支持 **19** 个邮箱服务提供商，所有渠道返回**统一标准化格式**。下列顺序与 `client.c` 中 `g_channel_infos` / `tm_list_channels` 一致（`temporary-email-org` 在 `vip-215` 之后；`mffac` 在 `fake-legal` 之后，与枚举一致）。

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

        // 获取邮件
        tm_get_emails_options_t get_opts = {0};
        get_opts.channel = info->channel;
        get_opts.email = info->email;
        get_opts.token = info->token;

        tm_get_emails_result_t *result = tm_get_emails(&get_opts);
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

**配置项：**

| 字段 | 类型 | 说明 |
|------|------|------|
| `proxy` | `const char*` | 代理 URL（http/https/socks5） |
| `timeout_secs` | `int` | 全局超时秒数，默认 15 |
| `insecure` | `bool` | 跳过 SSL 验证（调试用） |

**环境变量（无需修改代码，`tm_init()` 时自动读取）：**

```bash
export TEMPMAIL_PROXY="http://127.0.0.1:7890"
export TEMPMAIL_INSECURE=1
export TEMPMAIL_TIMEOUT=30
```

## 内存管理

所有返回的指针需要手动释放：
- `tm_free_email_info()` 释放邮箱信息
- `tm_free_get_emails_result()` 释放邮件结果
- `tm_free_email()` 释放单封邮件
