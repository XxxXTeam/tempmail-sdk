# tempmail-sdk

[![npm version](https://badge.fury.io/js/tempmail-sdk.svg)](https://www.npmjs.com/package/tempmail-sdk)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

临时邮箱 SDK（TypeScript/Node.js），支持 11 个邮箱服务提供商，所有渠道返回**统一标准化格式**。

## 安装

```bash
npm install tempmail-sdk
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
//   { channel: 'tempmail-la', name: 'TempMail LA', website: 'tempmail.la' },
//   { channel: 'temp-mail-io', name: 'Temp Mail IO', website: 'temp-mail.io' },
//   { channel: 'awamail', name: 'AwaMail', website: 'awamail.com' },
//   { channel: 'mail-tm', name: 'Mail.tm', website: 'mail.tm' },
//   { channel: 'dropmail', name: 'DropMail', website: 'dropmail.me' },
//   { channel: 'guerrillamail', name: 'Guerrilla Mail', website: 'guerrillamail.com' },
//   { channel: 'maildrop', name: 'Maildrop', website: 'maildrop.cc' }
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
```

#### 获取邮件

```typescript
import { getEmails } from 'tempmail-sdk';

// 不需要 Token 的渠道
const result = await getEmails({
  channel: 'tempmail',
  email: 'xxx@ibymail.com',
});
console.log(result.emails); // 标准化邮件数组

// 需要 Token 的渠道（token 由 generateEmail 返回）
const result2 = await getEmails({
  channel: 'mail-tm',
  email: emailInfo.email,
  token: emailInfo.token,    // Bearer Token
});

// 所有邮件使用统一格式，无需关心渠道差异
for (const email of result2.emails) {
  console.log(email.id);          // 邮件 ID
  console.log(email.from);        // 发件人
  console.log(email.to);          // 收件人
  console.log(email.subject);     // 主题
  console.log(email.text);        // 纯文本
  console.log(email.html);        // HTML
  console.log(email.date);        // ISO 日期
  console.log(email.isRead);      // 是否已读
  console.log(email.attachments); // 附件列表
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
| `duration` | `number` | 有效期分钟数（仅 `tempmail` 渠道） |
| `domain` | `string` | 指定域名（仅 `tempmail-lol` 渠道） |

**返回值:** `EmailInfo`

| 字段 | 类型 | 说明 |
|------|------|------|
| `channel` | `Channel` | 渠道标识 |
| `email` | `string` | 邮箱地址 |
| `token` | `string?` | 访问令牌（部分渠道返回） |
| `expiresAt` | `string \| number?` | 过期时间 |
| `createdAt` | `string?` | 创建时间 |

### getEmails(options)

获取邮件列表。

**参数:**

| 字段 | 类型 | 必填 | 说明 |
|------|------|:----:|------|
| `channel` | `Channel` | ✅ | 渠道标识 |
| `email` | `string` | ✅ | 邮箱地址 |
| `token` | `string` | 部分 | 访问令牌（`tempmail-lol`、`awamail`、`mail-tm`、`dropmail`、`guerrillamail`、`maildrop` 必填） |

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

## 环境要求

- Node.js 18+（需要原生 `fetch` 支持）
- TypeScript 5.0+（类型定义）

## 许可证

GPL-3.0
