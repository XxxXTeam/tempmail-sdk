# 临时邮箱 SDK

[![CI](https://github.com/XxxXTeam/tempmail-sdk/actions/workflows/ci.yml/badge.svg)](https://github.com/XxxXTeam/tempmail-sdk/actions/workflows/ci.yml)
[![npm version](https://badge.fury.io/js/tempmail-sdk.svg)](https://www.npmjs.com/package/tempmail-sdk)
[![Go Reference](https://pkg.go.dev/badge/github.com/XxxXTeam/tempmail-sdk/sdk/go.svg)](https://pkg.go.dev/github.com/XxxXTeam/tempmail-sdk/sdk/go)
[![PyPI version](https://badge.fury.io/py/tempemail-sdk.svg)](https://pypi.org/project/tempemail-sdk/)
[![crates.io](https://img.shields.io/crates/v/tempmail-sdk.svg)](https://crates.io/crates/tempmail-sdk)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

支持 **25** 个通用临时邮箱渠道（**npm / Rust / Python / C**）；**Go** SDK 另含 **`tempmailg`**，共 **26** 个渠道标识。提供 **Go、npm (TypeScript)、Rust、Python、C** 五种版本（C 的 `tm_channel_t` 枚举顺序与下表略有不同，见 `sdk/c/README.md`）。所有渠道返回**统一标准化格式**，无需关心各服务商的接口差异。

## ✨ 特性

- 🌐 **npm、Rust、Python、C** 各 **25** 个渠道；**Go** **26** 个（多 `tempmailg`）；跨语言标识一致（C 枚举顺序见头文件与 C README）
- 📐 **统一标准化返回格式** — 所有渠道的邮件数据结构完全一致
- 📦 提供 Go、npm、Rust、Python、C 五种 SDK
- 🔄 支持邮箱生成和邮件轮询
- 📝 完整的类型定义（TypeScript / Rust / Go）
- 🚀 简单易用的 API，开箱即用
- 🔌 Token/Session 自动管理（使用 Client 类）
- 🔁 创建邮箱 / 拉取邮件支持可配置 **HTTP 重试**（退避与最大次数等，各语言字段见子目录 README）
- 📊 **匿名遥测**默认开启：批量上报操作成败与重试元数据（不含邮件内容，错误串中的邮箱会脱敏），可用环境变量或全局配置关闭或改端点
- 🏗️ 多平台预编译二进制（Go / Rust / C）
- 📡 所有包均可通过 GitHub 托管安装，无需第三方注册表

## 📋 支持的渠道

下列顺序与各 SDK 的 `listChannels` / 随机尝试顺序一致。

| 渠道 | 服务商 | 认证方式 | 说明 |
|------|--------|----------|------|
| `tempmail` | [tempmail.ing](https://tempmail.ing) | 邮箱地址 | 支持自定义有效期 |
| `tempmail-cn` | [tempmail.cn](https://tempmail.cn) | 邮箱地址 | Socket.IO 事件协议：`request shortid` / `set shortid` / `mail`；`domain` 可指定 `tempmail.cn` 或已解析到该服务的自定义域名 |
| `tmpmails` | [tmpmails.com](https://tmpmails.com) | Token（`locale` + `user_sign` + Next.js `Next-Action`） | GET 首页下发 Cookie 与页面内地址；收信为 `POST` Server Action（`text/x-component`）；`domain` 可选为站点语言路径（如 `zh`、`en`） |
| `tempmailg` | [tempmailg.com](https://tempmailg.com) | Token（Base64 会话：`locale` + Cookie + CSRF） | **仅 Go SDK。** 无 Cookie 罐 `GET /public/{locale}` + `POST /public/get_messages`；`domain` 可选语言路径；换新邮箱须重新 `Generate` |
| `ta-easy` | [ta-easy.com](https://www.ta-easy.com) | Token（会话 UUID） | `POST https://api-endpoint.ta-easy.com/temp-email/address/new` 建址；`POST .../temp-email/inbox/list` 拉信；`expiresAt` 为毫秒时间戳；上游字段如 `mail_sender` / `mail_title` / `mail_body_*` 由 SDK 归一化到统一 `Email` |
| `10mail-wangtz` | [10mail.wangtz.cn](https://10mail.wangtz.cn) | - | `POST /api/tempMail`、`POST /api/emailList`；邮箱后缀固定 `wangtz.cn`；`domain` 可选作为 `emailName`（本地部分）；**各 SDK 对该站点默认跳过 TLS 证书校验**（与 `curl -k` 一致） |
| `linshi-email` | [linshi-email.com](https://linshi-email.com) | 邮箱地址 | |
| `linshiyou` | [linshiyou.com](https://linshiyou.com) | Token（`NEXUS_TOKEN`） | 创建邮箱时 Set-Cookie 下发 `NEXUS_TOKEN`；收信需携带该 Token 与 `tmail-emails` 等 Cookie；列表与正文由 HTML 分段 / iframe 解析 |
| `mffac` | [mffac.com](https://www.mffac.com) | Token（mailbox `id`） | REST：`POST /api/mailboxes` 创建，`GET /api/mailboxes/{local}/emails` 收信；默认 24h |
| `tempmail-lol` | [tempmail.lol](https://tempmail.lol) | Token | 支持指定域名 |
| `chatgpt-org-uk` | [mail.chatgpt.org.uk](https://mail.chatgpt.org.uk) | Inbox Token | 官网在 HTML 注入 `__BROWSER_AUTH`；npm 已随首页一并解析并用于创建邮箱 |
| `temp-mail-io` | [temp-mail.io](https://temp-mail.io) | Token | |
| `awamail` | [awamail.com](https://awamail.com) | Session Cookie | 自动提取 Cookie |
| `temporary-email-org` | [temporary-email.org](https://www.temporary-email.org) | Session Cookie | 首次 `GET /zh/messages` 下发 `email`、`locale`、`XSRF-TOKEN`、`temporaryemail_session`；拉取邮件需带完整 Cookie 与 `X-Requested-With: XMLHttpRequest` |
| `mail-tm` | [mail.tm](https://mail.tm) / [api.mail.tm](https://api.mail.tm) | Bearer Token | REST API，自动注册账号；npm 实现与 **Internxt** 等前端一致（如 `GET /domains?page=1`、常见浏览器请求头） |
| `mail-cx` | [mail.cx](https://mail.cx) / [api.mail.cx](https://api.mail.cx) | Bearer JWT（`POST /api/accounts`） | 公开 OpenAPI：`GET /api/domains`、`POST /api/accounts`（响应含 JWT）、`GET /api/messages`；可选 `domain` 指定系统域名 |
| `dropmail` | [dropmail.me](https://dropmail.me) | Session ID | GraphQL API |
| `guerrillamail` | [guerrillamail.com](https://guerrillamail.com) | Session | 公开 JSON API |
| `maildrop` | [maildrop.cx](https://maildrop.cx) | Token（完整邮箱） | REST：`GET /api/suffixes.php` 获取后缀，**排除** `transformer.edu.kg`，再随机生成本地部分；可选 `domain` 指定后缀（须仍在列表中）；`GET /api/emails.php?addr=` 拉列表；列表字段 `description` 映射为统一结构中的 `text`，无单封 MIME/HTML 全文 |
| `smail-pw` | [smail.pw](https://smail.pw) | `__session` Cookie | React Router `_root.data`（RSC/Flight）；列表侧为元数据，**npm / Python** 已解析 D1 行对象与引用下标 |
| `boomlify` | [boomlify.com](https://boomlify.com) | - | `GET /domains/public` 取域名后 `POST /emails/public/create`（`domainId`）登记收件箱；地址为 `{邮箱 UUID}@{域名}`，REST 拉信，无额外 token |
| `minmail` | [minmail.app](https://minmail.app) | 内部 Token（JSON） | `GET /api/mail/address` 返回 `visitorId` 与 `ck`；收信需请求头 `visitor-id` 与 `ck`。SDK 将二者序列化为 JSON 存入 token，兼容旧版仅 UUID |
| `vip-215` | [vip.215.im](https://vip.215.im) | WebSocket Token | `POST` 建箱 + `wss` 收 `message.new`；推送无正文时各 SDK 使用 **synthetic-v1** 统一生成 `text` / `html`（C 收信依赖 libcurl WebSocket，版本过低会降级） |
| `anonbox` | [anonbox.net](https://anonbox.net/en/) | 路径 Token（`{收件箱}/{密钥}`） | `GET /en/` 解析 HTML（合并隐藏 `span`）得到 `{local}@{收件箱}.anonbox.net`；收信 `GET` mbox 明文 URL |
| `fake-legal` | [fake.legal](https://fake.legal) | - | `GET /api/domains` + `GET /api/inbox/new?domain=` 建址；`GET /api/inbox/{encodeURIComponent(邮箱)}` 拉信；可选 `domain` |
| `moakt` | [moakt.com](https://www.moakt.com) | Token（`mok1:` + Base64 JSON：`locale` + 合并 Cookie，须含 `tm_session`） | HTML：`GET /{locale}` → `GET /{locale}/inbox` 解析 `#email-address`；收信解析 `href` 中 `/email/{uuid}` 并逐封 `GET .../html`；`domain` 可选语言路径（如 `zh`）；各 SDK 以独立会话或显式 `Cookie` 头维护，避免与全局 Cookie 混用 |

> **提示：** 使用 Client 类时，Token/Session 由 SDK 自动管理，无需手动处理。C SDK 中 `tm_list_channels` / `tm_channel_t` 的**枚举顺序**与上表（Go/npm 等随机列表顺序）不同，以 `tempmail_sdk.h` 与 `sdk/c/README.md` 为准。

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

> **说明：** 个别渠道的上游接口只提供列表或摘要（例如 `maildrop` 的 `description`），此时 `text` 可能仅为预览片段，`html` 可能为空字符串，属预期行为。各语言 `normalize` 模块会将常见别名字段映射到上述结构（例如 ta-easy 的 `mail_sender`→`from`、`mail_title`→`subject`、`mail_body_text` / `mail_body_html`→`text` / `html`，数字型 `received_at`→ISO 日期）。

## 📦 包获取渠道

每个 SDK 均有多种获取方式，下表汇总所有可用渠道：

| SDK | 渠道 | 安装方式 | 认证 |
|-----|------|---------|:----:|
| **Go** | GitHub (git tag) | `go get github.com/XxxXTeam/tempmail-sdk/sdk/go` | - |
| **npm** | [npmjs.org](https://www.npmjs.com/package/tempmail-sdk) | `npm install tempmail-sdk` | - |
| **npm** | [GitHub Packages](https://github.com/XxxXTeam/tempmail-sdk/pkgs/npm/tempmail-sdk) | `npm install @XxxXTeam/tempmail-sdk` | 🔑 |
| **Rust** | [crates.io](https://crates.io/crates/tempmail-sdk) | `tempmail-sdk = "1.1.0"` | - |
| **Rust** | GitHub (git) | `tempmail-sdk = { git = "...", subdirectory = "sdk/rust" }` | - |
| **Python** | [PyPI](https://pypi.org/project/tempemail-sdk/) | `pip install tempemail-sdk` | - |
| **Python** | [GitHub Release](https://github.com/XxxXTeam/tempmail-sdk/releases) | `pip install <wheel URL>` | - |
| **C** | [GitHub Release](https://github.com/XxxXTeam/tempmail-sdk/releases) | 下载预编译 zip 包 | - |
| **C** | 源码编译 | CMake 构建 | - |

> 🔑 = 需要认证。GitHub Packages (npm) 需要配置 GitHub PAT，详见下方说明。

## 📦 安装

### Go

```bash
go get github.com/XxxXTeam/tempmail-sdk/sdk/go
```

### npm / TypeScript

```bash
# 从 npmjs.org（推荐，无需认证）
npm install tempmail-sdk
```

<details>
<summary>从 GitHub Packages 安装（需认证）</summary>

GitHub Packages 的 npm 包即使是公开仓库也需要认证：

```bash
# 1. 创建 GitHub PAT: Settings → Developer settings → Personal access tokens → 勾选 read:packages
# 2. 配置 .npmrc
echo "@XxxXTeam:registry=https://npm.pkg.github.com" >> ~/.npmrc
echo "//npm.pkg.github.com/:_authToken=YOUR_GITHUB_TOKEN" >> ~/.npmrc
# 3. 安装
npm install @XxxXTeam/tempmail-sdk
```

</details>

### Rust

```toml
# 从 crates.io（推荐）
[dependencies]
tempmail-sdk = "1.1.0"

# 从 GitHub（始终获取最新代码）
[dependencies]
tempmail-sdk = { git = "https://github.com/XxxXTeam/tempmail-sdk", subdirectory = "sdk/rust" }
```

### Python

```bash
# 从 PyPI（推荐）
pip install tempemail-sdk

# 从 GitHub Release（wheel 直链）
pip install https://github.com/XxxXTeam/tempmail-sdk/releases/latest/download/tempemail_sdk-1.1.0-py3-none-any.whl
```

### C

从 [GitHub Releases](https://github.com/XxxXTeam/tempmail-sdk/releases) 下载预编译包：

| 包名 | 平台 |
|------|------|
| `c-sdk-linux-amd64.zip` | Linux x64 |
| `c-sdk-darwin-arm64.zip` | macOS ARM64 |
| `c-sdk-windows-amd64.zip` | Windows x64 |

或源码编译：

```bash
cd sdk/c
curl -L -o vendor/cJSON.h https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.h
curl -L -o vendor/cJSON.c https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.c
cmake -B build -S . && cmake --build build
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

### npm — 仓库内示例脚本

在 `sdk/npm` 目录下（需先 `npm install`，部分 demo 另需 `nodemailer`）：

| 脚本 | 说明 |
|------|------|
| `demo/poll-emails.ts` | 交互或 **SMTP 自动探针**（设置 `SMTP_HOST` 等）；可用 `POLL_CHANNELS=smail-pw` 限定渠道 |

常用环境变量：`TEMPMAIL_PROXY`、`TEMPMAIL_TIMEOUT`、`TEMPMAIL_INSECURE`；DropMail 见各 SDK 文档中的 `DROPMAIL_*` 说明。

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

## 匿名遥测与 HTTP 重试

### 匿名遥测（可关闭）

五语言 SDK **默认开启**匿名用量统计：在进程内将事件入队，定时或满批后合并为一次 **`POST`**，JSON 体为 **`schema_version: 2`**，包含 `sdk_language`、`sdk_version`、`os`、`arch` 以及 `events[]`（如 `operation`、`channel`、`success`、`attempt_count`、`channels_tried`、脱敏后的 `error`、`ts_ms` 等）。**不包含**邮件正文或 Token；错误文案里形似邮箱的片段会替换为 `[redacted]`。

| 环境变量 | 说明 |
|---------|------|
| `TEMPMAIL_TELEMETRY_ENABLED` | `true` / `1` / `yes` 开启（默认），`false` / `0` / `no` 关闭 |
| `TEMPMAIL_TELEMETRY_URL` | 覆盖默认上报 URL（内置默认一般为 `https://sdk-1.openel.top/v1/event`，以各 SDK 源码为准） |

也可在代码里通过全局配置关闭或指定 URL：**Go** `SDKConfig.TelemetryEnabled` / `TelemetryEndpoint`；**npm** `telemetryEnabled` / `telemetryUrl`；**Rust** `telemetry_enabled` / `telemetry_url`；**Python** `SDKConfig.telemetry_enabled` / `telemetry_url`；**C** `tm_config_t.telemetry_enabled` / `telemetry_url`。

本仓库下的 **`telemetry-server/`** 为可选的收集服务示例（`POST /v1/event` 写入 SQLite 等）；**SDK 上报不需要令牌**，与自建服务的管理端鉴权无关。

### HTTP 重试

各语言在「生成邮箱」「拉取邮件」上均支持 **Retry**（如最大次数、超时、退避），具体字段名见对应 SDK 的 `README.md` 与类型定义（如 npm 的 `retry`、`Go` 的 `RetryOptions` 等）。

## 📖 API 文档

详细 API 文档请参阅各 SDK 目录：

| SDK | 文档 | 注册表 |
|-----|------|--------|
| Go | [sdk/go/README.md](./sdk/go/README.md) | [pkg.go.dev](https://pkg.go.dev/github.com/XxxXTeam/tempmail-sdk/sdk/go) |
| npm | [sdk/npm/README.md](./sdk/npm/README.md) | [npmjs.org](https://www.npmjs.com/package/tempmail-sdk) |
| Rust | [sdk/rust/README.md](./sdk/rust/README.md) | [crates.io](https://crates.io/crates/tempmail-sdk) |
| Python | [sdk/python/README.md](./sdk/python/README.md) | [PyPI](https://pypi.org/project/tempemail-sdk/) |
| C | [sdk/c/README.md](./sdk/c/README.md) | [GitHub Releases](https://github.com/XxxXTeam/tempmail-sdk/releases) |

## 🔧 项目结构

```
tempmail-sdk/
├── sdk/
│   ├── go/                     # Go SDK
│   │   ├── client.go           # 入口文件
│   │   ├── types.go            # 类型定义
│   │   ├── normalize.go        # 标准化转换
│   │   └── provider/*.go       # 各渠道实现（如 ta_easy.go、wangtz_10mail.go、tmpmails.go）
│   ├── npm/                    # npm SDK (TypeScript)
│   │   ├── src/
│   │   │   ├── index.ts        # 入口文件
│   │   │   ├── types.ts        # 类型定义
│   │   │   └── providers/      # 各渠道实现
│   │   ├── demo/               # 示例与探针脚本
│   │   └── test/               # 测试代码
│   ├── rust/                   # Rust SDK
│   │   ├── src/
│   │   │   ├── lib.rs          # 库入口
│   │   │   ├── client.rs       # Client 实现
│   │   │   └── providers/      # 各渠道实现
│   │   └── examples/           # 示例代码
│   ├── python/                 # Python SDK
│   │   ├── tempmail_sdk/
│   │   │   ├── __init__.py     # 入口
│   │   │   ├── client.py       # Client 实现
│   │   │   └── providers/      # 各渠道实现
│   │   └── pyproject.toml      # 包配置
│   └── c/                      # C SDK
│       ├── include/            # 公共头文件
│       ├── src/                # 源码
│       │   └── providers/      # 各渠道实现
│       └── CMakeLists.txt      # 构建配置
├── telemetry-server/           # 可选：遥测收集服务（Gin + SQLite 等，见目录内实现）
├── .github/
│   └── workflows/
│       ├── ci.yml              # CI 工作流
│       └── release.yml         # 发布工作流
├── LICENSE
└── README.md
```

## 🚢 发布

### 自动发布

推送 tag 触发自动发布：

```bash
git tag v1.1.0
git push origin v1.1.0
```

这将自动：
1. 验证全部 5 种 SDK 构建
2. 发布 npm 包到 npmjs.org 和 GitHub Packages
3. 发布 Rust crate 到 crates.io
4. 发布 Python wheel 到 PyPI
5. 构建 Go / Rust / C 多平台二进制
6. 创建 GitHub Release（附带全部构建产物 + 变更日志）

### 配置 Secrets

在 GitHub 仓库 Settings → Secrets and variables → Actions 中添加：

| Secret | 说明 |
|--------|------|
| `NPM_TOKEN` | npm 访问令牌 |
| `CARGO_REGISTRY_TOKEN` | crates.io API Token |
| `PYPI_TOKEN` | PyPI API Token |

> `GITHUB_TOKEN` 由 GitHub Actions 自动提供，无需手动配置。

## 🛠️ 开发

### Go SDK

```bash
cd sdk/go
go build ./...
gofmt -d .
```

### npm SDK

```bash
cd sdk/npm
npm install
npm run build
npx tsc --noEmit
npm test
```

### Rust SDK

```bash
cd sdk/rust
cargo build
cargo test
cargo clippy
```

### Python SDK

```bash
cd sdk/python
pip install -e ".[dev]"
pytest
```

### C SDK

```bash
cd sdk/c
# 下载 cJSON 依赖
curl -L -o vendor/cJSON.h https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.h
curl -L -o vendor/cJSON.c https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.c
cmake -B build -S . && cmake --build build
```

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
3. 在各 SDK 的 Channel 类型/枚举中注册新渠道
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
