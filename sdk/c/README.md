# tempmail-sdk (C)

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

临时邮箱 SDK（C），支持 11 个邮箱服务提供商，所有渠道返回**统一标准化格式**。

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

| 渠道 | 枚举值 | 服务商 | 需要 Token |
|------|--------|--------|:----------:|
| tempmail | `CHANNEL_TEMPMAIL` | tempmail.ing | - |
| linshi-email | `CHANNEL_LINSHI_EMAIL` | linshi-email.com | - |
| tempmail-lol | `CHANNEL_TEMPMAIL_LOL` | tempmail.lol | ✅ |
| chatgpt-org-uk | `CHANNEL_CHATGPT_ORG_UK` | mail.chatgpt.org.uk | - |
| tempmail-la | `CHANNEL_TEMPMAIL_LA` | tempmail.la | - |
| temp-mail-io | `CHANNEL_TEMP_MAIL_IO` | temp-mail.io | - |
| awamail | `CHANNEL_AWAMAIL` | awamail.com | ✅ |
| mail-tm | `CHANNEL_MAIL_TM` | mail.tm | ✅ |
| dropmail | `CHANNEL_DROPMAIL` | dropmail.me | ✅ |
| guerrillamail | `CHANNEL_GUERRILLAMAIL` | guerrillamail.com | ✅ |
| maildrop | `CHANNEL_MAILDROP` | maildrop.cc | ✅ |

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
