# tempmail-sdk

[![npm version](https://badge.fury.io/js/tempmail-sdk.svg)](https://www.npmjs.com/package/tempmail-sdk)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

临时邮箱 SDK（TypeScript/Node.js），支持 **55** 个邮箱服务提供商，所有渠道返回**统一标准化格式**。

## 安装

```bash
# 从 npmjs.org
npm install tempmail-sdk

# 从 GitHub Packages
npm install @XxxXTeam/tempmail-sdk --registry=https://npm.pkg.github.com
```

## 支持的渠道

共 **55** 个，顺序与 `listChannels()` / 随机尝试顺序一致（与 `src/index.ts` 中 `allChannels` 相同），并与 Go / Rust / Python / C 对齐。

| 渠道 | 服务商 | 需要 Token | 说明 |
|------|--------|:----------:|------|
| `tempmail` | tempmail.ing | - | 支持自定义有效期 |
| `tempmail-cn` | tempmail.cn | - | Socket.IO：`request shortid` / `set shortid` / `mail`；`domain` 可指定 `tempmail.cn` 或自定义接入域名 |
| `ta-easy` | ta-easy.com | ✅ | REST 创建 + 收件箱列表；`expiresAt` 毫秒时间戳 |
| `10minute-one` | 10minutemail.one | ✅ | SSR / JWT + Web API；`domain` 可选 |
| `linshiyou` | linshiyou.com | ✅ | `NEXUS_TOKEN` + `tmail-emails` 等 Cookie；HTML 分段解析 |
| `mffac` | mffac.com | ✅ | `POST /api/mailboxes`；`GET /api/mailboxes/{local}/emails` 列表，`GET /api/emails/{id}` 详情补齐 `text` / `html`；token 为 mailbox `id`；24h |
| `tempmail-lol` | tempmail.lol | ✅ | 支持指定域名 |
| `chatgpt-org-uk` | mail.chatgpt.org.uk | ✅ | 首页注入 `__BROWSER_AUTH`，创建邮箱时须带 `X-Inbox-Token` + `gm_sid`（已自动处理） |
| `temp-mail-io` | temp-mail.io | ✅ | REST：动态读取 `mobileTestingHeader` 后调用 `api.internal.temp-mail.io/api/v3` |
| `mail-cx` | mail.cx | ✅ | 匿名 Web API：`/v1/config` 获取系统域名，`/v1/inbox/{email}` 长轮询收信，内部保存 `X-Client-ID` |
| `catchmail` | catchmail.io | - | 公开 REST：`GET /api/v1/mailbox?address=` 列表，`GET /api/v1/message/{id}?mailbox=` 详情；详情 `body.text` / `body.html` 映射为统一正文 |
| `catchmail-mailistry` | mailistry.com | - | Catchmail API 固定域名 `mailistry.com` |
| `catchmail-zeppost` | zeppost.com | - | Catchmail API 固定域名 `zeppost.com` |
| `mailforspam` | mailforspam.com | - | 公开 REST：`GET /api/mailboxes/{email}/emails` 列表，`GET /api/mailboxes/{email}/emails/{id}` 详情；详情 `body_text` / `body_html` 映射为统一正文 |
| `mailforspam-tempmail-io` | tempmail.io | - | MailForSpam API 固定域名 `tempmail.io` |
| `mailforspam-disposable` | disposable.email | - | MailForSpam API 固定域名 `disposable.email` |
| `tempmailo` | tempmailo.com | ✅ | `GET /changemail` 建址，`POST /` 传 `mail` 拉信；返回对象直接含 `text` / `html` 正文 |
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
| `mail-tm` | mail.tm / api.mail.tm | ✅ | 自动注册账号；请求与 **Internxt** 等站点前端一致（`GET /domains?page=1`、`GET /messages?page=1` 及常见浏览器头） |
| `dropmail` | dropmail.me | ✅ | GraphQL API |
| `guerrillamail` | guerrillamail.com | ✅ | 公开 JSON API |
| `guerrillamail-com` | guerrillamail.com | ✅ | GuerrillaMail 裸域 JSON API 入口 |
| `maildrop` | maildrop.cx | ✅ | REST：`suffixes.php`（排除 `transformer.edu.kg`）+ 随机前缀；`emails.php` 列表，`description`→`text` |
| `smail-pw` | smail.pw | ✅ | `POST/GET https://smail.pw/_root.data`，`__session` Cookie；解析 RSC/Flight 中的 **D1 邮件行对象**（`subject`/`time` 等） |
| `vip-215` | vip.215.im | ✅ | `POST` 建箱 + `wss`；无正文时 synthetic 兜底 |
| `fake-legal` | fake.legal | - | `/api/domains` + `/api/inbox/new?domain=`；`/api/inbox/{encodeURIComponent(邮箱)}` 拉信；可选 `domain` |
| `moakt` | moakt.com | ✅ | HTML 收件箱 + `tm_session`；`domain` 可选语言路径；Token 为 `mok1:` + Base64 JSON |
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

> **提示：** 使用 `TempEmailClient` 类时无需手动处理 Token，SDK 自动管理。

## 快速开始

### 使用 TempEmailClient（推荐）

```typescript
import { TempEmailClient } from 'tempmail-sdk';

const client = new TempEmailClient();

// 获取临时邮箱（可指定渠道，不指定则随机）
const emailInfo = await client.generate({ channel: 'tempmail' });
console.log(`渠道: ${emailInfo.channel}`);
console.log(`邮箱: ${emailInfo.email}`);
if (emailInfo.expiresAt) console.log(`过期时间: ${emailInfo.expiresAt}`);

// 获取邮件（Token 自动传递，无需手动处理）
const result = await client.getEmails();
console.log(`收到 ${result.emails.length} 封邮件`);

for (const email of result.emails) {
  console.log(`发件人: ${email.from}`);
  console.log(`主题: ${email.subject}`);
  console.log(`内容: ${email.text}`);
  console.log(`时间: ${email.date}`);
  console.log(`已读: ${email.isRead}`);
  console.log(`附件: ${email.attachments.length} 个`);
}
```

### 使用函数式 API

#### 列出所有渠道

```typescript
import { listChannels, getChannelInfo } from 'tempmail-sdk';

const channels = listChannels();
console.log(channels);

const info = getChannelInfo('tempmail');
// { channel: 'tempmail', name: 'TempMail', website: 'tempmail.ing' }
```

#### 获取邮箱

```typescript
import { generateEmail } from 'tempmail-sdk';

// 从随机渠道获取邮箱
const emailInfo = await generateEmail();
console.log(emailInfo);
// { channel: 'tempmail', email: 'xxx@ibymail.com', expiresAt: '...' }

// 从指定渠道获取邮箱
const emailInfo2 = await generateEmail({ channel: 'mail-gw' });

// tempmail 渠道支持自定义有效期（分钟）
const emailInfo3 = await generateEmail({ channel: 'tempmail', duration: 60 });

// tempmail-lol 渠道支持指定域名
const emailInfo4 = await generateEmail({ channel: 'tempmail-lol', domain: 'example.com' });

// tempmail-cn 渠道支持指定 tempmail.cn 自定义接入域名
const emailInfo5 = await generateEmail({ channel: 'tempmail-cn', domain: 'mail.example.com' });

// 只尝试指定渠道（探测可用性、写自动化时用）
const probe = await generateEmail({ channel: 'smail-pw', channelFallback: false });
```

#### 获取邮件

`getEmails` 的第一个参数必须是 **`generateEmail` 返回的 `EmailInfo`**（或与 `TempEmailClient` 缓存的相同结构）。Token / Session 由 SDK 内部绑定到该对象，**不要**再手动传入 `token` 字段。

```typescript
import { generateEmail, getEmails } from 'tempmail-sdk';

const emailInfo = await generateEmail({ channel: 'tempmail' });
if (!emailInfo) throw new Error('创建失败');
const result = await getEmails(emailInfo);
console.log(result.success, result.emails.length);

const mailTm = await generateEmail({ channel: 'mail-tm' });
if (mailTm) {
  const r2 = await getEmails(mailTm, { retry: { maxRetries: 3, timeout: 20000 } });
  for (const email of r2.emails) {
    console.log(email.id, email.from, email.subject, email.date);
  }
}
```

## API 参考

### listChannels()

获取所有支持的渠道列表。

**返回值:** `ChannelInfo[]`

| 字段 | 类型 | 说明 |
|------|------|------|
| `channel` | `Channel` | 渠道标识 |
| `name` | `string` | 渠道显示名称 |
| `website` | `string` | 服务商网站 |

### getChannelInfo(channel)

获取指定渠道信息。

**返回值:** `ChannelInfo | undefined`

### generateEmail(options?)

生成临时邮箱地址。

**参数:**

| 字段 | 类型 | 说明 |
|------|------|------|
| `channel` | `Channel` | 指定渠道（可选，不指定则随机） |
| `channelFallback` | `boolean` | 默认 `true`：指定渠道失败会继续尝试其他渠道；设为 `false` 时仅尝试 `channel` |
| `duration` | `number` | 有效期分钟数（仅 `tempmail` 渠道） |
| `domain` | `string` | 指定域名或接入参数（`tempmail-cn`、`tempmail-lol`、`maildrop`、`fake-legal`、`catchmail` / `mailforspam` 固定域名、`moakt` 语言路径等） |
| `retry` | `RetryConfig` | 创建邮箱时的重试（超时、5xx、网络错误等） |

**返回值:** `EmailInfo`

| 字段 | 类型 | 说明 |
|------|------|------|
| `channel` | `Channel` | 渠道标识 |
| `email` | `string` | 邮箱地址 |
| `token` | `string?` | 访问令牌（部分渠道返回） |
| `expiresAt` | `string \| number?` | 过期时间 |
| `createdAt` | `string?` | 创建时间 |

### getEmails(info, options?)

获取邮件列表。

**参数:**

| 参数 | 类型 | 说明 |
|------|------|------|
| `info` | `EmailInfo` | 由 `generateEmail()` 或 `TempEmailClient.generate()` 返回；SDK 从中读取 `channel`、`email` 及内部 Token |
| `options?.retry` | `RetryConfig` | 拉取邮件时的重试配置（可选） |

**返回值:** `GetEmailsResult`

| 字段 | 类型 | 说明 |
|------|------|------|
| `channel` | `Channel` | 渠道标识 |
| `email` | `string` | 邮箱地址 |
| `emails` | `Email[]` | 标准化邮件数组 |
| `success` | `boolean` | 是否成功 |

### 标准化邮件格式

所有渠道返回的邮件均使用统一的 `Email` 格式：

```typescript
interface Email {
  id: string;                      // 邮件唯一标识
  from: string;                    // 发件人邮箱地址
  to: string;                      // 收件人邮箱地址
  subject: string;                 // 邮件主题
  text: string;                    // 纯文本内容
  html: string;                    // HTML 内容
  date: string;                    // ISO 8601 格式日期
  isRead: boolean;                 // 是否已读
  attachments: EmailAttachment[];  // 附件列表
}

interface EmailAttachment {
  filename: string;      // 文件名
  size?: number;         // 文件大小（字节）
  contentType?: string;  // MIME 类型
  url?: string;          // 下载地址
}
```

### TempEmailClient 类

封装了 Token 自动管理的便捷客户端：

```typescript
const client = new TempEmailClient();
await client.generate(options?);     // 生成邮箱
await client.getEmails();            // 获取邮件（自动传递 token）
client.getEmailInfo();               // 获取当前邮箱信息
```

## 代理与 HTTP 配置

SDK 支持全局配置代理、超时等 HTTP 客户端参数，也可通过环境变量零代码配置：

```typescript
import { setConfig } from 'tempmail-sdk';

// 一行跳过 SSL 验证
setConfig({ insecure: true });

// 设置全局超时
setConfig({ timeout: 30000 });

// 使用自定义 fetch（如 undici 代理）
import { ProxyAgent, fetch as undiciFetch } from 'undici';
const agent = new ProxyAgent('http://127.0.0.1:7890');
setConfig({
  customFetch: (url, init) => undiciFetch(url, { ...init, dispatcher: agent }) as any,
});
```

**配置项：**

| 字段 | 类型 | 说明 |
|------|------|------|
| `proxy` | `string?` | 代理 URL（http/https/socks5） |
| `timeout` | `number?` | 全局超时毫秒数，默认 15000 |
| `insecure` | `boolean?` | 跳过 SSL 验证（调试用） |
| `customFetch` | `typeof fetch?` | 自定义 fetch 函数，用于代理等高级场景 |
| `telemetryEnabled` | `boolean?` | 未设则默认开启匿名遥测；`false` 关闭 |
| `telemetryUrl` | `string?` | 覆盖默认上报 URL |

**环境变量（无需修改代码）：**

```bash
export TEMPMAIL_PROXY="http://127.0.0.1:7890"
export TEMPMAIL_INSECURE=1
export TEMPMAIL_TIMEOUT=30000
export TEMPMAIL_TELEMETRY_ENABLED=false
export TEMPMAIL_TELEMETRY_URL="https://example.com/v1/event"
```

> **提示：** Node.js 原生 fetch 不支持代理，推荐通过 `customFetch` + `undici` 的 `ProxyAgent` 实现代理支持。

DropMail 自动令牌等更多配置见源码 `src/config.ts` 注释（`DROPMAIL_*` 环境变量）。

## 匿名遥测

默认 **开启**：在进程内排队，定时或满批向默认端点 `POST` 匿名事件（`schema_version: 2`，含 SDK 语言/版本、OS、操作类型、渠道、成败与重试次数等；错误串脱敏）。关闭：`TEMPMAIL_TELEMETRY_ENABLED=false`（或 `0` / `no`）或 `setConfig({ telemetryEnabled: false })`；改 URL：`TEMPMAIL_TELEMETRY_URL` 或 `telemetryUrl`（内置默认见 `src/telemetry.ts`）。

## 示例脚本（本仓库）

在 `sdk/npm` 目录执行：

| 命令 / 文件 | 说明 |
|-------------|------|
| `npx ts-node demo/poll-emails.ts` | 未配置 SMTP 时为交互式选渠道并轮询；配置 `SMTP_HOST` 等后可自动向各渠道发探针并轮询（`POLL_CHANNELS`、`POLL_INTERVAL_MS`、`POLL_MAX_ROUNDS`） |

`poll-emails` 使用 SMTP 时需安装：`npm install nodemailer @types/nodemailer`（若尚未加入 devDependencies）。

## 环境要求

- Node.js 18+（需要原生 `fetch` 支持）
- TypeScript 5.0+（类型定义）

## 许可证

GPL-3.0
