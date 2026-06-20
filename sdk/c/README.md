# tempmail-sdk (C)

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

临时邮箱 SDK（C），支持 **55** 个邮箱服务提供商，所有渠道返回**统一标准化格式**。

- **`tm_list_channels()`** 返回顺序与 `client.c` 中 `g_channel_infos` / `g_channel_try_order` 一致，并与 **Go `allChannels` / 其他 SDK 的 `listChannels`** 对齐。
- **`tm_channel_t` 枚举常量**的数值顺序为历史兼容布局，与上表行序**不一致**；`tm_channel_name()` 按枚举值映射字符串。新增渠道时以 `tempmail_sdk.h` 为准。

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

下表按 **`tm_channel_t` 枚举数值**排列（便于查 `CHANNEL_*` 常量），**不是** `tm_list_channels()` 的展示顺序。

| 渠道 | 枚举值 | 服务商 | 需要 Token | 说明 |
|------|--------|--------|:----------:|------|
| tempmail | `CHANNEL_TEMPMAIL` | tempmail.ing | - | 支持自定义有效期 |
| linshiyou | `CHANNEL_LINSHIYOU` | linshiyou.com | ✅ | `NEXUS_TOKEN` + Cookie；HTML 分段解析 |
| tempmail-lol | `CHANNEL_TEMPMAIL_LOL` | tempmail.lol | ✅ | 支持指定域名 |
| chatgpt-org-uk | `CHANNEL_CHATGPT_ORG_UK` | mail.chatgpt.org.uk | ✅ | Inbox Token 等由 SDK 封装 |
| awamail | `CHANNEL_AWAMAIL` | awamail.com | ✅ | Session Cookie 自动管理 |
| mail-tm | `CHANNEL_MAIL_TM` | mail.tm | ✅ | 自动注册，Bearer Token |
| dropmail | `CHANNEL_DROPMAIL` | dropmail.me | ✅ | GraphQL |
| guerrillamail | `CHANNEL_GUERRILLAMAIL` | guerrillamail.com | ✅ | 公开 JSON API |
| maildrop | `CHANNEL_MAILDROP` | maildrop.cx | ✅ | REST：`suffixes.php` + `emails.php`；`description`→`text` |
| smail-pw | `CHANNEL_SMAIL_PW` | smail.pw | ✅ | `_root.data` + `__session` |
| catchmail-mailistry | `CHANNEL_CATCHMAIL_MAILISTRY` | mailistry.com | - | Catchmail API 固定域名 `mailistry.com` |
| catchmail-zeppost | `CHANNEL_CATCHMAIL_ZEPPOST` | zeppost.com | - | Catchmail API 固定域名 `zeppost.com` |
| vip-215 | `CHANNEL_VIP_215` | vip.215.im | ✅ | WebSocket 收信 |
| mailforspam-tempmail-io | `CHANNEL_MAILFORSPAM_TEMPMAIL_IO` | tempmail.io | - | MailForSpam API 固定域名 `tempmail.io` |
| fake-legal | `CHANNEL_FAKE_LEGAL` | fake.legal | - | `/api/domains` + `/api/inbox/new`；`tm_generate_options_t.domain` 可选 |
| mffac | `CHANNEL_MFFAC` | mffac.com | ✅ | mailbox `id`；`/api/emails/{id}` 详情补齐 `text` / `html`；REST 24h |
| tempmail-cn | `CHANNEL_TEMPMAIL_CN` | tempmail.cn | - | Socket.IO：`request shortid` / `set shortid` / `mail`；`tm_generate_options_t.domain` 可指定自定义接入域名 |
| ta-easy | `CHANNEL_TA_EASY` | ta-easy.com | ✅ | REST `api-endpoint.ta-easy.com`；需 `info->token` 拉信 |
| mailforspam-disposable | `CHANNEL_MAILFORSPAM_DISPOSABLE` | disposable.email | - | MailForSpam API 固定域名 `disposable.email` |
| moakt | `CHANNEL_MOAKT` | moakt.com | ✅ | HTML 收件箱 + `tm_session`；`domain` 可选语言路径（如 `zh`） |
| 10minute-one | `CHANNEL_10MINUTE_ONE` | 10minutemail.one | ✅ | SSR / JWT + Web API；`tm_generate_options_t.domain` 可选 |
| guerrillamail-com | `CHANNEL_GUERRILLAMAIL_COM` | guerrillamail.com | ✅ | GuerrillaMail 裸域 JSON API 入口 |
| sharklasers-com | `CHANNEL_SHARKLASERS_COM` | sharklasers.com | ✅ | GuerrillaMail 裸域镜像，API 与 `guerrillamail` 相同 |
| email10min | `CHANNEL_EMAIL10MIN` | email10min.com | ✅ | Cookie + CSRF；`POST /messages` 获取邮箱与邮件 |
| mjj-cm | `CHANNEL_MJJ_CM` | mjj.cm | - | Socket.IO 渠道；当前 C SDK 不支持生成/收信 |
| grr-la-com | `CHANNEL_GRR_LA_COM` | grr.la | ✅ | GuerrillaMail 裸域镜像，API 与 `guerrillamail` 相同 |
| linshi-co | `CHANNEL_LINSHI_CO` | linshi.co | - | Socket.IO 克隆站；当前 C SDK 不支持生成/收信 |
| harakirimail | `CHANNEL_HARAKIRIMAIL` | harakirimail.com | - | 公开 REST：`GET /api/v1/inbox/{name}` + `GET /api/v1/email/{id}` |
| tempmail-plus | `CHANNEL_TEMPMAIL_PLUS` | tempmail.plus | - | 公开 REST：`GET /api/mails/?email=` 列表，`GET /api/mails/{id}?email=` 详情 |
| mail-gw | `CHANNEL_MAIL_GW` | mail.gw | ✅ | 自动注册，JWT Bearer Token |
| tempmail-lol-v2 | `CHANNEL_TEMPMAIL_LOL_V2` | tempmail.lol | ✅ | `GET /generate` 返回 address+token，`GET /auth/{token}` 拉取收件箱 |
| sharklasers | `CHANNEL_SHARKLASERS` | sharklasers.com | ✅ | GuerrillaMail 镜像，API 与 `guerrillamail` 相同 |
| grr-la | `CHANNEL_GRR_LA` | grr.la | ✅ | GuerrillaMail 镜像，API 与 `guerrillamail` 相同 |
| guerrillamail-info | `CHANNEL_GUERRILLAMAIL_INFO` | guerrillamail.info | ✅ | GuerrillaMail 镜像，API 与 `guerrillamail` 相同 |
| temp-mail-io | `CHANNEL_TEMP_MAIL_IO` | temp-mail.io | ✅ | REST：动态读取 `mobileTestingHeader` 后调用 `api.internal.temp-mail.io/api/v3` |
| mail-cx | `CHANNEL_MAIL_CX` | mail.cx | ✅ | 匿名 Web API：`/v1/config` 获取系统域名，`/v1/inbox/{email}` 长轮询收信，内部保存 `X-Client-ID` |
| catchmail | `CHANNEL_CATCHMAIL` | catchmail.io | - | 公开 REST：`/api/v1/mailbox?address=` + `/api/v1/message/{id}?mailbox=`；详情含 `body.text` / `body.html` |
| mailforspam | `CHANNEL_MAILFORSPAM` | mailforspam.com | - | 公开 REST：`/api/mailboxes/{email}/emails` + `/api/mailboxes/{email}/emails/{id}`；详情含 `body_text` / `body_html` |
| tempmailo | `CHANNEL_TEMPMAILO` | tempmailo.com | ✅ | `GET /changemail` 建址，`POST /` 传 `mail` 拉信；返回对象直接含 `text` / `html` |
| tempmailc | `CHANNEL_TEMPMAILC` | tempmailc.com | - | Public API：`GET /api/v1/new` 建址，`GET /api/v1/inbox` 拉列表，`GET /api/v1/message` 读取 `text` / `html` 正文 |
| mailnesia | `CHANNEL_MAILNESIA` | mailnesia.com | - | 任意 `{local}@mailnesia.com` 建址；HTML 列表 `tr.emailheader` + 详情 `text_plain_{id}` / `text_html_{id}` 正文 |
| throwawaymail | `CHANNEL_THROWAWAYMAIL` | throwawaymail.app | ✅ | Web API 建址并轮询收信；Token 由 SDK 内部维护 |
| inboxkitten | `CHANNEL_INBOXKITTEN` | inboxkitten.com | - | 公开 API 拉取收件箱列表与详情 |
| getnada | `CHANNEL_GETNADA` | getnada.net | ✅ | `POST /api/inbox/open` 建箱；`GET /api/inbox/messages` 列表；`GET /api/inbox/message` 详情含 `text_plain` / `html_sanitized` |
| mail123 | `CHANNEL_MAIL123` | mail123.fr | - | `GET /api/v1/mailbox/new` 建址；`GET /api/v1/mailbox/{address}/messages?limit=50` 列表；详情含 `text` / `html` |
| spam4me | `CHANNEL_SPAM4ME` | spam4.me | ✅ | GuerrillaMail 镜像，API 与 `guerrillamail` 相同 |
| guerrillamail-net | `CHANNEL_GUERRILLAMAIL_NET` | guerrillamail.net | ✅ | GuerrillaMail 镜像，API 与 `guerrillamail` 相同 |
| guerrillamail-org | `CHANNEL_GUERRILLAMAIL_ORG` | guerrillamail.org | ✅ | GuerrillaMail 镜像，API 与 `guerrillamail` 相同 |
| guerrillamailblock | `CHANNEL_GUERRILLAMAILBLOCK` | guerrillamailblock.com | ✅ | GuerrillaMail 镜像，API 与 `guerrillamail` 相同 |
| guerrillamail-com-www | `CHANNEL_GUERRILLAMAIL_COM_WWW` | guerrillamail.com | ✅ | GuerrillaMail `www` JSON API 入口 |
| 1sec-mail | `CHANNEL_ONE_SEC_MAIL` | 1sec-mail.com | ✅ | CSRF + Cookie；`POST /get_messages` 拉列表；详情由 `content` / `html` 映射，缺失正文由 normalize 反向生成 |
| fakemail | `CHANNEL_FAKEMAIL` | fakemail.net | ✅ | `/index/index` 建址，`/index/refresh` 拉列表，`/index/email` 详情；`telo` HTML 正文 |
| openinbox | `CHANNEL_OPENINBOX` | openinbox.io | ✅ | `POST /api/inbox` 建箱；`GET /emails/inbox/{id}` 列表；`GET /emails/{emailId}` 详情含 `textBody` / `htmlBody` |
| inboxes | `CHANNEL_INBOXES` | inboxes.com | - | 公开 v2：`GET /api/v2/domain` 域名，`GET /api/v2/inbox/{email}` 列表，`GET /api/v2/message/{uid}` 详情含 `text` / `html` |
| uncorreotemporal | `CHANNEL_UNCORREOTEMPORAL` | uncorreotemporal.com | ✅ | `POST /api/v1/mailboxes` 建箱；`X-Session-Token` 拉取列表和详情；详情含 `body_text` / `body_html` |

## 快速开始

```c
#include <stdio.h>
#include "tempmail_sdk.h"

int main(void) {
    tm_init();
    tm_set_log_level(TM_LOG_INFO);

    // 创建临时邮箱
    tm_generate_options_t opts = {0};
    opts.channel = CHANNEL_MAIL_GW;
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
