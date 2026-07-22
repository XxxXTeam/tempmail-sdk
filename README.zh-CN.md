# 临时邮箱 SDK

[English](README.md) | [中文](README.zh-CN.md)

[![CI](https://github.com/XxxXTeam/tempmail-sdk/actions/workflows/ci.yml/badge.svg)](https://github.com/XxxXTeam/tempmail-sdk/actions/workflows/ci.yml)
[![npm version](https://badge.fury.io/js/tempmail-sdk.svg)](https://www.npmjs.com/package/tempmail-sdk)
[![Go Reference](https://pkg.go.dev/badge/github.com/XxxXTeam/tempmail-sdk/sdk/go.svg)](https://pkg.go.dev/github.com/XxxXTeam/tempmail-sdk/sdk/go)
[![PyPI version](https://badge.fury.io/py/tempemail-sdk.svg)](https://pypi.org/project/tempemail-sdk/)
[![crates.io](https://img.shields.io/crates/v/tempmail-sdk.svg)](https://crates.io/crates/tempmail-sdk)
[![NuGet](https://img.shields.io/nuget/v/XxxXTeam.TempMail.svg)](https://www.nuget.org/packages/XxxXTeam.TempMail/)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

## 📖 概述

十端 SDK（**Go、npm、Rust、Python、C、Java、C#、Ruby、PHP、Kotlin**）共同公开 **279** 个 `channel` 标识，按独立服务商合并为约 **110** 个 provider。固定域名、裸域、镜像域名和同一 API 的多域名只算同一个独立服务商。所有 SDK 共享相同的渠道标识字符串、数量与顺序（与 `.baseline_channels.txt` 对齐，且与 Go `allChannels` 一致）。C 语言中 `tm_list_channels()` 返回顺序亦与基线一致；`tm_channel_t` 的**枚举数值顺序**仍为历史兼容布局，与列表顺序不同，见 `sdk/c/README.md`。所有渠道返回**统一标准化格式**，无需关心各服务商的接口差异。

## ✨ 特性

- 🌐 **十端共同公开 279 个 `channel` 标识，合并为约 110 个独立服务商**；C 的 `tm_channel_t` 枚举下标与 `tm_list_channels` 顺序不同（见 `sdk/c/README.md`）
- 📐 **统一标准化返回格式** — 所有渠道的邮件数据结构完全一致
- 📦 提供 Go、npm、Rust、Python、C、Java、C#、Ruby、PHP、Kotlin 十种 SDK
- 🔄 支持邮箱生成和邮件轮询
- 📝 完整的类型定义（TypeScript / Rust / Go / Java / C# / Kotlin）
- 🚀 简单易用的 API，开箱即用
- 🔌 Token/Session 自动管理（使用 Client 类）
- 🔁 创建邮箱 / 拉取邮件支持可配置 **HTTP 重试**（退避与最大次数等，各语言字段见子目录 README）
- 📊 **匿名遥测**默认开启：批量上报操作成败与重试元数据（不含邮件内容，错误串中的邮箱会脱敏），可用环境变量或全局配置关闭或改端点
- 🏗️ 多平台预编译二进制（Go / Rust / C）
- 📡 所有包均可通过主流注册表或 GitHub 托管安装

## 📦 安装

### Go

```bash
go get github.com/XxxXTeam/tempmail-sdk/sdk/go
```

### npm / TypeScript

```bash
npm install tempmail-sdk
```

### Rust

```toml
# Cargo.toml
[dependencies]
tempmail-sdk = "1.0.3"
```

### Python

```bash
pip install tempemail-sdk
```

### C

从 [GitHub Releases](https://github.com/XxxXTeam/tempmail-sdk/releases) 下载预编译包，或源码编译：

```bash
cd sdk/c
curl -L -o vendor/cJSON.h https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.h
curl -L -o vendor/cJSON.c https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.c
cmake -B build -S . && cmake --build build
```

### Java (Maven)

```xml
<dependency>
  <groupId>io.github.xxxxteam</groupId>
  <artifactId>tempmail-sdk</artifactId>
  <version>1.0.0</version>
</dependency>
```

### C# / .NET

```bash
dotnet add package XxxXTeam.TempMail
```

### Ruby

```bash
gem install tempmail_sdk
```

### PHP

PHP SDK 未发布到 Composer / Packagist（SDK 位于本 monorepo 的 `sdk/php/` 子目录，而 Composer 只能解析 `composer.json` 位于仓库根目录的包）。请改用以下任一方式：

```bash
# 方式一：从 GitHub Releases 下载打包 zip（release 页的 tempmail-sdk-php-*.zip），引入其 autoloader
unzip tempmail-sdk-php-*.zip -d tempmail-sdk-php
# 代码中：require 'tempmail-sdk-php/vendor/autoload.php';

# 方式二：clone 仓库后直接加载 sdk/php
git clone https://github.com/XxxXTeam/tempmail-sdk.git
composer install -d tempmail-sdk/sdk/php
# 代码中：require 'tempmail-sdk/sdk/php/vendor/autoload.php';
```

### Kotlin (Gradle)

```kotlin
// build.gradle.kts
implementation("io.github.xxxxteam:tempmail-sdk:0.1.0")
```

## 🚀 快速开始

### npm — 使用 TempEmailClient（推荐）

Token/Session 自动管理，适用于所有渠道：

```typescript
import { TempEmailClient } from 'tempmail-sdk';

const client = new TempEmailClient();

// 1. 获取临时邮箱（可指定渠道，不指定则随机）
const emailInfo = await client.generate({ channel: 'tempmail' });
console.log('邮箱:', emailInfo.email);

// 2. 轮询获取邮件
const result = await client.getEmails();
for (const email of result.emails) {
  console.log(`发件人: ${email.from}`);
  console.log(`主题: ${email.subject}`);
  console.log(`内容: ${email.text}`);
  console.log(`时间: ${email.date}`);
}
```

### npm — 使用函数式 API

```typescript
import { generateEmail, getEmails } from 'tempmail-sdk';

// 1. 获取临时邮箱
const emailInfo = await generateEmail({ channel: 'mail-tm' });
if (!emailInfo) {
  // 未指定 channel 时可能多渠道路径全部失败
  throw new Error('创建失败');
}
console.log('邮箱:', emailInfo.email);

// 2. 获取邮件（Token 由 SDK 内部与 EmailInfo 绑定，勿自行传 token）
const result = await getEmails(emailInfo);
console.log(`收到 ${result.emails.length} 封邮件`, result.success);

// 仅探测某一渠道、失败时不 Fallback 到其他渠道：
const only = await generateEmail({ channel: 'smail-pw', channelFallback: false });
```

### Go — 使用 Client

```go
package main

import (
    "fmt"
    tempemail "github.com/XxxXTeam/tempmail-sdk/sdk/go"
)

func main() {
    client := tempemail.NewClient()

    // 1. 获取临时邮箱
    emailInfo, err := client.Generate(&tempemail.GenerateEmailOptions{
        Channel: tempemail.ChannelTempmail,
    })
    if err != nil {
        panic(err)
    }
    fmt.Println("邮箱:", emailInfo.Email)

    // 2. 获取邮件
    result, err := client.GetEmails()
    if err != nil {
        panic(err)
    }
    for _, email := range result.Emails {
        fmt.Printf("发件人: %s\n", email.From)
        fmt.Printf("主题: %s\n", email.Subject)
        fmt.Printf("内容: %s\n", email.Text)
        fmt.Printf("时间: %s\n", email.Date)
    }
}
```

## 📐 统一邮件格式

无论使用哪个渠道，返回的邮件数据结构完全一致：

```typescript
interface Email {
  id: string;            // 邮件唯一标识
  from: string;          // 发件人邮箱地址
  to: string;            // 收件人邮箱地址
  subject: string;       // 邮件主题
  text: string;          // 纯文本内容
  html: string;          // HTML 内容
  date: string;          // ISO 8601 格式日期
  isRead: boolean;       // 是否已读
  attachments: {         // 附件列表
    filename: string;
    size?: number;
    contentType?: string;
    url?: string;
  }[];
}
```

> **说明：** 个别渠道的上游接口只提供列表或摘要（例如 `maildrop` 的 `description`），此时 `text` 可能仅为预览片段。各语言 `normalize` 模块会将常见别名字段映射到上述结构（例如 ta-easy 的 `mail_sender`→`from`、`mail_title`→`subject`、`mail_body_text` / `mail_body_html`→`text` / `html`，数字型 `received_at`→ISO 日期）。如果上游详情只返回 `html` 或只返回 `text`，SDK 会从已有正文反向生成缺失字段：`html` 会去标签生成 `text`，纯文本会转义并包进基础 HTML。

## 📋 支持的渠道

十端共同公开 **279** 个渠道，标识字符串、数量、顺序完全一致（以 `.baseline_channels.txt` 为准）。随机生成邮箱时，各 SDK 会在本端独立打乱尝试顺序，不承诺跨 SDK 随机顺序一致。渠道统计以独立服务商为准，同一 provider 的固定域名、裸域、镜像域名和同 API 多域名不重复计入服务商数量。

<details>
<summary>点击展开渠道示例（前 30 个，完整列表见 <code>.baseline_channels.txt</code>）</summary>

| 渠道 | 服务商 | 认证方式 | 说明 |
|------|--------|----------|------|
| `tempmail` | [tempmail.ing](https://tempmail.ing) | 邮箱地址 | 支持自定义有效期 |
| `tempmail-cn` | [tempmail.cn](https://tempmail.cn) | 邮箱地址 | Socket.IO 事件协议：`request shortid` / `set shortid` / `mail`；`domain` 可指定 `tempmail.cn` 或已解析到该服务的自定义域名 |
| `ta-easy` | [ta-easy.com](https://www.ta-easy.com) | Token（会话 UUID） | `POST .../temp-email/address/new` 建址；`POST .../temp-email/inbox/list` 拉信；`expiresAt` 为毫秒时间戳；上游字段如 `mail_sender` / `mail_title` / `mail_body_*` 由 SDK 归一化到统一 `Email` |
| `10minute-one` | [10minutemail.one](https://10minutemail.one) | Token | 站点 SSR / JWT + Web API 建邮与收信；`domain` 可选接入参数（见各 SDK 说明） |
| `xghff-com` | [xghff.com](https://xghff.com) | Token | 10minutemail.one API 固定域名 `xghff.com`；生成和收信接口与 `10minute-one` 相同 |
| `oqqaj-com` | [oqqaj.com](https://oqqaj.com) | Token | 10minutemail.one API 固定域名 `oqqaj.com`；生成和收信接口与 `10minute-one` 相同 |
| `psovv-com` | [psovv.com](https://psovv.com) | Token | 10minutemail.one API 固定域名 `psovv.com`；生成和收信接口与 `10minute-one` 相同 |
| `dbwot-com` | [dbwot.com](https://dbwot.com) | Token | 10minutemail.one API 固定域名 `dbwot.com`；生成和收信接口与 `10minute-one` 相同 |
| `ygwpr-com` | [ygwpr.com](https://ygwpr.com) | Token | 10minutemail.one API 固定域名 `ygwpr.com`；生成和收信接口与 `10minute-one` 相同 |
| `imxwe-com` | [imxwe.com](https://imxwe.com) | Token | 10minutemail.one API 固定域名 `imxwe.com`；生成和收信接口与 `10minute-one` 相同 |
| `linshiyou` | [linshiyou.com](https://linshiyou.com) | Token（`NEXUS_TOKEN`） | 创建邮箱时 Set-Cookie 下发 `NEXUS_TOKEN`；收信需携带该 Token 与 `tmail-emails` 等 Cookie；列表与正文由 HTML 分段 / iframe 解析 |
| `mffac` | [mffac.com](https://www.mffac.com) | Token（mailbox `id`） | REST：`POST /api/mailboxes` 创建，`GET /api/mailboxes/{local}/emails` 列表，`GET /api/emails/{id}` 详情；详情 `textContent` / `htmlContent` 映射为统一正文；默认 24h |
| `tempmail-lol` | [tempmail.lol](https://tempmail.lol) | Token | 支持指定域名 |
| `chatgpt-org-uk` | [mail.chatgpt.org.uk](https://mail.chatgpt.org.uk) | Inbox Token | 官网在 HTML 注入 `__BROWSER_AUTH`；npm 已随首页一并解析并用于创建邮箱 |
| `temp-mail-io` | [temp-mail.io](https://temp-mail.io) | Token | REST：动态读取 `mobileTestingHeader` 后调用 `api.internal.temp-mail.io/api/v3`；生成接口返回的 token 由 SDK 内部维护 |
| `mail-cx` | [mail.cx](https://mail.cx) | Client ID | 匿名 Web API：`GET /v1/config` 获取系统域名，隐式邮箱地址 `{local}@{domain}`；`GET /v1/inbox/{email}` 长轮询收信，详情通过 `/v1/email/{id}` 拉取 |
| `ddker-com` | [ddker.com](https://ddker.com) | Client ID | mail.cx API 固定域名 `ddker.com`；生成和收信接口与 `mail-cx` 相同 |
| `catchmail` | [catchmail.io](https://catchmail.io) | - | 公开 REST：`GET api.catchmail.io/api/v1/mailbox?address=` 列表，`GET /message/{id}?mailbox=` 详情；详情 `body.text` / `body.html` 映射为统一 `text` / `html` |
| `catchmail-mailistry` | [mailistry.com](https://mailistry.com) | - | Catchmail API 固定域名 `mailistry.com`；收信接口与 `catchmail` 相同 |
| `catchmail-zeppost` | [zeppost.com](https://zeppost.com) | - | Catchmail API 固定域名 `zeppost.com`；收信接口与 `catchmail` 相同 |
| `mailforspam` | [mailforspam.com](https://mailforspam.com) | - | 公开 REST：`GET api.mailforspam.com/api/mailboxes/{email}/emails` 列表，`GET .../{id}` 详情；`body_text` / `body_html` 映射为统一 `text` / `html` |
| `mailforspam-tempmail-io` | [tempmail.io](https://tempmail.io) | - | MailForSpam API 固定域名 `tempmail.io`；收信接口与 `mailforspam` 相同 |
| `mailforspam-disposable` | [disposable.email](https://disposable.email) | - | MailForSpam API 固定域名 `disposable.email`；收信接口与 `mailforspam` 相同 |
| `tempmailc` | [tempmailc.com](https://tempmailc.com) | - | Public API：`GET /api/v1/new` 建址，`GET /api/v1/inbox` 拉列表，`GET /api/v1/message` 读取 `text` / `html` 正文 |
| `mailnesia` | [mailnesia.com](https://mailnesia.com) | - | 任意 `{local}@mailnesia.com` 建址；`GET /mailbox/{local}` 解析 `tr.emailheader` 列表，`GET /mailbox/{local}/{id}` 读取正文 |
| `throwawaymail` | [throwawaymail.app](https://throwawaymail.app) | Token | Web API 建址并轮询收信；Token 由 SDK 内部维护 |
| `shitty-email` | [shitty.email](https://shitty.email) | Token | `POST /api/inbox` 建址；`X-Session-Token` + `GET /api/inbox` 拉列表，`GET /api/email/{id}` 读取 `text` / `html` 正文 |
| `tempmailpro` | [tempmailpro.us](https://tempmailpro.us) | Token | `POST /api/v1/mailbox/create` 建箱；`GET /api/v1/mailbox/{token}/emails` 拉列表，详情字段 `body_text` / `body_html` 映射统一正文 |
| `devmail-uk` | [devmail.uk](https://devmail.uk) | 邮箱地址 | `GET /api/new` 建址；`GET /api/inbox/{mailbox}?detail=true` 拉列表；生成接口返回的 `email` / `mailbox` 字段均兼容解析 |
| `inboxkitten` | [inboxkitten.com](https://inboxkitten.com) | - | 公开 API 拉取收件箱列表与详情 |

</details>

> **提示：** 使用 Client 类时，Token/Session 由 SDK 自动管理，无需手动处理。C SDK 中 `tm_list_channels()` 的**返回顺序**与基线一致；若按 `tm_channel_t` **枚举常量**编程，其数值顺序与列表不同，以 `tempmail_sdk.h` 与 `sdk/c/README.md` 为准。

## ⚙️ 环境变量

| 变量 | 说明 |
|------|------|
| `TEMPMAIL_PROXY` | HTTP/SOCKS5 代理 URL |
| `TEMPMAIL_TIMEOUT` | 超时秒数 |
| `TEMPMAIL_INSECURE` | `1`/`true` 跳过 TLS 验证 |
| `TEMPMAIL_TELEMETRY_ENABLED` | `false`/`0`/`no` 关闭遥测 |
| `TEMPMAIL_TELEMETRY_URL` | 自定义遥测端点 |
| `DROPMAIL_AUTH_TOKEN` | DropMail GraphQL af_ 令牌 |
| `APIHZ_ID` | apihz（接口盒子）渠道凭据 id |
| `APIHZ_KEY` | apihz（接口盒子）渠道凭据 key |

## 📊 匿名遥测与 HTTP 重试

### 匿名遥测（可关闭）

十端 SDK **默认开启**匿名用量统计：在进程内将事件入队，定时或满批后合并为一次 **`POST`**，JSON 体为 **`schema_version: 2`**，包含 `sdk_language`、`sdk_version`、`os`、`arch` 以及 `events[]`（如 `operation`、`channel`、`success`、`attempt_count`、`channels_tried`、脱敏后的 `error`、`ts_ms` 等）。**不包含**邮件正文或 Token；错误文案里形似邮箱的片段会替换为 `[redacted]`。

| 环境变量 | 说明 |
|---------|------|
| `TEMPMAIL_TELEMETRY_ENABLED` | `true` / `1` / `yes` 开启（默认），`false` / `0` / `no` 关闭 |
| `TEMPMAIL_TELEMETRY_URL` | 覆盖默认上报 URL（内置默认一般为 `https://sdk-1.openel.top/v1/event`，以各 SDK 源码为准） |

也可在代码里通过全局配置关闭或指定 URL：**Go** `SDKConfig.TelemetryEnabled` / `TelemetryEndpoint`；**npm** `telemetryEnabled` / `telemetryUrl`；**Rust** `telemetry_enabled` / `telemetry_url`；**Python** `SDKConfig.telemetry_enabled` / `telemetry_url`；**C** `tm_config_t.telemetry_enabled` / `telemetry_url`；其余语言字段见对应 SDK 的 `README.md`。

本仓库下的 **`telemetry-server/`** 为可选的收集服务示例（`POST /v1/event` 写入 SQLite 等）；**SDK 上报不需要令牌**，与自建服务的管理端鉴权无关。

### HTTP 重试

各语言在「生成邮箱」「拉取邮件」上均支持 **Retry**（如最大次数、超时、退避），具体字段名见对应 SDK 的 `README.md` 与类型定义（如 npm 的 `retry`、Go 的 `RetryOptions` 等）。

## 🔧 项目结构

```
tempmail-sdk/
├── sdk/
│   ├── go/                     # Go SDK
│   ├── npm/                    # npm SDK (TypeScript)
│   ├── rust/                   # Rust SDK
│   ├── python/                 # Python SDK
│   ├── c/                      # C SDK
│   ├── java/                   # Java SDK
│   ├── csharp/                 # C# / .NET SDK
│   ├── ruby/                   # Ruby SDK
│   ├── php/                    # PHP SDK
│   └── kotlin/                 # Kotlin SDK
├── telemetry-server/           # 可选：遥测收集服务（Gin + SQLite 等）
├── .baseline_channels.txt      # 279 个渠道标识基线（十端顺序对齐）
├── .github/
│   └── workflows/
│       ├── ci.yml              # CI 工作流
│       └── release.yml         # 发布工作流
├── LICENSE
├── README.md
└── README.zh-CN.md
```

各 SDK 内部遵循一致的分层结构：`Client`（状态管理）→ 入口函数（`generate` / `getEmails`，支持渠道 fallback）→ `providers/`（每渠道一个实现）→ `normalize`（统一邮件格式）→ `config` / `retry` / `telemetry`。

## 🔄 CI/CD

GitHub Actions（`.github/workflows/ci.yml`）在 push/PR 到 main 时对全部十端运行构建、格式检查与静态分析：

- **Go**：gofmt + go vet + go build
- **npm**：tsc --noEmit + npm build + 产物验证（Node 18/20/22）
- **Rust**：clippy + cargo build + cargo test
- **Python**：pip install + python -m build（3.10/3.11/3.12）
- **C**：libcurl + cJSON + cmake build
- **Java / C# / Ruby / PHP / Kotlin**：各自工具链的构建与检查

## 🚢 发布

推送 tag 触发自动发布：

```bash
git tag v1.0.3
git push origin v1.0.3
```

这将自动：

1. 验证全部 10 种 SDK 构建
2. 发布 npm 包到 npmjs.org 和 GitHub Packages
3. 发布 Rust crate 到 crates.io
4. 发布 Python wheel 到 PyPI
5. 发布 C# 包到 NuGet、Java 包到 Maven Central、Ruby gem 到 RubyGems、Kotlin 包到 Maven Central；PHP SDK 打包为 zip 附加到 GitHub Release（不发布到 Packagist）
6. 构建 Go / Rust / C 多平台二进制
7. 创建 GitHub Release（附带全部构建产物 + 变更日志）

### 配置 Secrets

在 GitHub 仓库 Settings → Secrets and variables → Actions 中添加：

| Secret | 说明 |
|--------|------|
| `NPM_TOKEN` | npm 访问令牌 |
| `CARGO_REGISTRY_TOKEN` | crates.io API Token |
| `PYPI_TOKEN` | PyPI API Token |
| `NUGET_TOKEN` | NuGet API Key |

> `GITHUB_TOKEN` 由 GitHub Actions 自动提供，无需手动配置。

## ⭐ Star 历史

<a href="https://star-history.com/#XxxXTeam/tempmail-sdk&Date">
 <picture>
   <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=XxxXTeam/tempmail-sdk&type=Date&theme=dark" />
   <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=XxxXTeam/tempmail-sdk&type=Date" />
   <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=XxxXTeam/tempmail-sdk&type=Date" />
 </picture>
</a>

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！请阅读 [贡献指南](CONTRIBUTING.md) 了解详情。

### 快速开始

1. Fork 本仓库
2. 创建功能分支：`git checkout -b feat/your-feature`
3. 提交代码并推送
4. 创建 Pull Request

### 添加新的渠道提供商

1. 在各 SDK 的 `providers/` 目录下新建提供商文件
2. 实现 `generateEmail()` 和 `getEmails()` 两个核心函数
3. 在各 SDK 的 Channel 类型/枚举中注册新渠道（十端必须同步，标识字符串与数量须与 `.baseline_channels.txt` 一致）
4. 使用 `normalizeEmail()` 标准化返回数据
5. 所有 HTTP 请求使用全局共享客户端（支持代理/TLS 配置）
6. 更新 README 文档

详见 [CONTRIBUTING.md](CONTRIBUTING.md) 中的完整指南和代码示例。

## 📄 许可证

本项目基于 [GPL-3.0](LICENSE) 许可证开源。

```
Copyright (C) 2026 XxxXTeam

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
```
