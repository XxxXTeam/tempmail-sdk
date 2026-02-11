# tempmail-sdk (C)

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

临时邮箱 SDK（C），支持 11 个邮箱服务提供商，所有渠道返回**统一标准化格式**。

## 依赖

- **libcurl** - HTTP 请求
- **cJSON** - JSON 解析（需下载到 `vendor/` 目录）

## 构建

```bash
# 1. 下载 cJSON
cd vendor
curl -L -o cJSON.h https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.h
curl -L -o cJSON.c https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.c
cd ..

# 2. CMake 构建
mkdir build && cd build
cmake ..
make
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

## 内存管理

所有返回的指针需要手动释放：
- `tm_free_email_info()` 释放邮箱信息
- `tm_free_get_emails_result()` 释放邮件结果
- `tm_free_email()` 释放单封邮件
