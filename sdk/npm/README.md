# tempmail-sdk

[![npm version](https://badge.fury.io/js/tempmail-sdk.svg)](https://www.npmjs.com/package/tempmail-sdk)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

临时邮箱 SDK（TypeScript/Node.js），支持 **15** 个邮箱服务提供商，所有渠道返回**统一标准化格式**。

## 安装

```bash
# 从 npmjs.org
npm install tempmail-sdk

# 从 GitHub Packages
npm install @XxxXTeam/tempmail-sdk --registry=https://npm.pkg.github.com
```

## 支持的渠道

| 渠道 | 服务商 | 需要 Token | 说明 |
|------|--------|:----------:|------|
| `tempmail` | tempmail.ing | - | 支持自定义有效期 |
| `linshi-email` | linshi-email.com | - | |
| `tempmail-lol` | tempmail.lol | ✅ | 支持指定域名 |
| `chatgpt-org-uk` | mail.chatgpt.org.uk | ✅ | 首页注入 `__BROWSER_AUTH`，创建邮箱时须带 `X-Inbox-Token` + `gm_sid`（已自动处理） |
| `temp-mail-io` | temp-mail.io | - | |
| `awamail` | awamail.com | ✅ | Session Cookie 自动管理 |
| `mail-tm` | mail.tm / api.mail.tm | ✅ | 自动注册账号；请求与 **Internxt** 等站点前端一致（`GET /domains?page=1`、`GET /messages?page=1` 及常见浏览器头） |
| `dropmail` | dropmail.me | ✅ | GraphQL API |
| `guerrillamail` | guerrillamail.com | ✅ | 公开 JSON API |
| `maildrop` | maildrop.cc | ✅ | GraphQL；`data` 为 MIME 源码，解析 plain/multipart/Base64/QP，HTML 兜底 `text` |
| `smail-pw` | smail.pw | ✅ | `POST/GET https://smail.pw/_root.data`，`__session` Cookie；解析 RSC/Flight 中的 **D1 邮件行对象**（`subject`/`time` 等） |
| `boomlify` | boomlify.com | - | `domains/public` + `emails/public/create`；地址 `{UUID}@{域名}` |
| `minmail` | minmail.app | ✅ | `visitor-id` / `ck` 等序列化在 token（JSON） |
| `mffac` | mffac.com | - | REST API，24h 有效期 |
| `vip-215` | vip.215.im | ✅ | `POST` 建箱 + `wss`；无正文时 synthetic 兜底 |

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
// [
//   { channel: 'tempmail', name: 'TempMail', website: 'tempmail.ing' },
//   { channel: 'linshi-email', name: '临时邮箱', website: 'linshi-email.com' },
//   { channel: 'tempmail-lol', name: 'TempMail LOL', website: 'tempmail.lol' },
//   { channel: 'chatgpt-org-uk', name: 'ChatGPT Mail', website: 'mail.chatgpt.org.uk' },
//   { channel: 'temp-mail-io', name: 'Temp Mail IO', website: 'temp-mail.io' },
//   { channel: 'awamail', name: 'AwaMail', website: 'awamail.com' },
//   { channel: 'mail-tm', name: 'Mail.tm', website: 'mail.tm' },
//   { channel: 'dropmail', name: 'DropMail', website: 'dropmail.me' },
//   { channel: 'guerrillamail', name: 'Guerrilla Mail', website: 'guerrillamail.com' },
//   { channel: 'maildrop', name: 'Maildrop', website: 'maildrop.cc' },
//   { channel: 'smail-pw', name: 'Smail.pw', website: 'smail.pw' }
// ]

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
const emailInfo2 = await generateEmail({ channel: 'linshi-email' });

// tempmail 渠道支持自定义有效期（分钟）
const emailInfo3 = await generateEmail({ channel: 'tempmail', duration: 60 });

// tempmail-lol 渠道支持指定域名
const emailInfo4 = await generateEmail({ channel: 'tempmail-lol', domain: 'example.com' });

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
| `domain` | `string` | 指定域名（仅 `tempmail-lol` 渠道） |
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

**环境变量（无需修改代码）：**

```bash
export TEMPMAIL_PROXY="http://127.0.0.1:7890"
export TEMPMAIL_INSECURE=1
export TEMPMAIL_TIMEOUT=30000
```

> **提示：** Node.js 原生 fetch 不支持代理，推荐通过 `customFetch` + `undici` 的 `ProxyAgent` 实现代理支持。

DropMail 自动令牌等更多配置见源码 `src/config.ts` 注释（`DROPMAIL_*` 环境变量）。

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
