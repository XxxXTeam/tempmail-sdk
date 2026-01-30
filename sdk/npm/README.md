# temp-email-sdk

临时邮箱 SDK，支持多个邮箱服务提供商。

## 许可证

GPL-3.0

## 安装

```bash
npm install tempmail-sdk
```

## 支持的渠道

| 渠道 | 服务商 |
|------|--------|
| `tempmail` | tempmail.ing |
| `linshi-email` | linshi-email.com |
| `tempmail-lol` | tempmail.lol |
| `chatgpt-org-uk` | mail.chatgpt.org.uk |

## 使用方法

### 列出所有提供商

```typescript
import { listChannels, getChannelInfo } from 'temp-email-sdk';

// 获取所有支持的渠道
const channels = listChannels();
console.log(channels);
// [
//   { channel: 'tempmail', name: 'TempMail', website: 'tempmail.ing' },
//   { channel: 'linshi-email', name: '临时邮箱', website: 'linshi-email.com' },
//   { channel: 'tempmail-lol', name: 'TempMail LOL', website: 'tempmail.lol' },
//   { channel: 'chatgpt-org-uk', name: 'ChatGPT Mail', website: 'mail.chatgpt.org.uk' }
// ]

// 获取指定渠道信息
const info = getChannelInfo('tempmail');
console.log(info);
// { channel: 'tempmail', name: 'TempMail', website: 'tempmail.ing' }
```

### 获取邮箱

```typescript
import { generateEmail } from 'temp-email-sdk';

// 从随机渠道获取邮箱
const emailInfo = await generateEmail();
console.log(emailInfo);
// { channel: 'tempmail', email: 'xxx@ibymail.com', expiresAt: '...' }

// 从指定渠道获取邮箱
const emailInfo2 = await generateEmail({ channel: 'linshi-email' });
console.log(emailInfo2);
// { channel: 'linshi-email', email: 'xxx@iwatermail.com', expiresAt: 1768283576295 }
```

### 获取邮件

```typescript
import { getEmails } from 'temp-email-sdk';

// 查询邮件（需要传递渠道和邮箱）
const result = await getEmails({
  channel: 'tempmail',
  email: 'xxx@ibymail.com'
});
console.log(result);
// { channel: 'tempmail', email: 'xxx@ibymail.com', emails: [...], success: true }

// tempmail-lol 渠道需要传递 token
const result2 = await getEmails({
  channel: 'tempmail-lol',
  email: 'xxx@chessgameland.com',
  token: 'your-token-here'
});
```

### 使用 TempEmailClient 类

```typescript
import { TempEmailClient } from 'temp-email-sdk';

const client = new TempEmailClient();

// 获取邮箱
const emailInfo = await client.generate({ channel: 'tempmail' });
console.log(`渠道: ${emailInfo.channel}`);
console.log(`邮箱: ${emailInfo.email}`);

// 获取邮件
const result = await client.getEmails();
console.log(`收到 ${result.emails.length} 封邮件`);
```

## API

### listChannels()

获取所有支持的渠道列表。

**返回值:** `ChannelInfo[]`
- `channel` - 渠道标识
- `name` - 渠道名称
- `website` - 服务商网站

### getChannelInfo(channel)

获取指定渠道信息。

**参数:**
- `channel` - 渠道标识

**返回值:** `ChannelInfo | undefined`

### generateEmail(options?)

生成临时邮箱地址。

**参数:**
- `channel` - 指定渠道（可选，不指定则随机选择）
- `duration` - 邮箱有效期，单位分钟（仅 `tempmail` 渠道支持）
- `domain` - 指定域名（仅 `tempmail-lol` 渠道支持）

**返回值:** `EmailInfo`
- `channel` - 渠道名称
- `email` - 邮箱地址
- `token` - 访问收件箱的令牌（仅 `tempmail-lol` 渠道返回）
- `expiresAt` - 过期时间

### getEmails(options)

获取指定邮箱的邮件列表。

**参数（必填）:**
- `channel` - 渠道名称
- `email` - 邮箱地址
- `token` - 令牌（`tempmail-lol` 渠道必填）

**返回值:** `GetEmailsResult`
- `channel` - 渠道名称
- `email` - 邮箱地址
- `emails` - 邮件数组
- `success` - 是否成功

## 环境要求

- Node.js 18+（需要原生 fetch 支持）
