# 临时邮箱 SDK

[![CI](https://github.com/XxxXTeam/tempmail-sdk/actions/workflows/ci.yml/badge.svg)](https://github.com/XxxXTeam/tempmail-sdk/actions/workflows/ci.yml)
[![npm version](https://badge.fury.io/js/tempmail-sdk.svg)](https://www.npmjs.com/package/tempmail-sdk)
[![Go Reference](https://pkg.go.dev/badge/github.com/XxxXTeam/tempmail-sdk/sdk/go.svg)](https://pkg.go.dev/github.com/XxxXTeam/tempmail-sdk/sdk/go)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

支持 **9 个**临时邮箱服务商的 SDK，提供 npm (TypeScript) 和 Go 两种版本。所有渠道返回**统一标准化格式**，无需关心各服务商的接口差异。

## ✨ 特性

- 🌐 支持 9 个临时邮箱服务商，一套代码适配所有渠道
- 📐 **统一标准化返回格式** — 所有渠道的邮件数据结构完全一致
- 📦 提供 npm 和 Go 两种 SDK
- 🔄 支持邮箱生成和邮件轮询
- 📝 完整的 TypeScript 类型定义
- 🚀 简单易用的 API，开箱即用
- 🔌 Token/Session 自动管理（使用 `TempEmailClient` 类）

## 📋 支持的渠道

| 渠道 | 服务商 | 认证方式 | 说明 |
|------|--------|----------|------|
| `tempmail` | [tempmail.ing](https://tempmail.ing) | 邮箱地址 | 支持自定义有效期 |
| `linshi-email` | [linshi-email.com](https://linshi-email.com) | 邮箱地址 | |
| `tempmail-lol` | [tempmail.lol](https://tempmail.lol) | Token | 支持指定域名 |
| `chatgpt-org-uk` | [mail.chatgpt.org.uk](https://mail.chatgpt.org.uk) | 邮箱地址 | |
| `tempmail-la` | [tempmail.la](https://tempmail.la) | 邮箱地址 | 支持分页 |
| `temp-mail-io` | [temp-mail.io](https://temp-mail.io) | Token | |
| `awamail` | [awamail.com](https://awamail.com) | Session Cookie | 自动提取 Cookie |
| `mail-tm` | [mail.tm](https://mail.tm) | Bearer Token | REST API，自动注册账号 |
| `dropmail` | [dropmail.me](https://dropmail.me) | Session ID | GraphQL API |

> **提示：** 使用 `TempEmailClient` 类时，Token/Session 由 SDK 自动管理，无需手动处理。

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

## 📦 安装

### npm / TypeScript

```bash
npm install tempmail-sdk
```

### Go

```bash
go get github.com/XxxXTeam/tempmail-sdk/sdk/go
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
console.log('邮箱:', emailInfo.email);

// 2. 获取邮件（token 由 generateEmail 返回，部分渠道需要传递）
const result = await getEmails({
  channel: emailInfo.channel,
  email: emailInfo.email,
  token: emailInfo.token,
});
console.log(`收到 ${result.emails.length} 封邮件`);
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

## 📖 API 文档

详细 API 文档请参阅：
- [npm SDK 文档](./sdk/npm/README.md)
- [Go SDK 文档](./sdk/go/README.md)

## 🔧 项目结构

```
tempmail-sdk/
├── sdk/
│   ├── npm/                    # npm SDK (TypeScript)
│   │   ├── src/
│   │   │   ├── index.ts        # 入口文件
│   │   │   ├── types.ts        # 类型定义
│   │   │   ├── normalize.ts    # 标准化转换工具
│   │   │   └── providers/      # 各渠道实现
│   │   │       ├── tempmail.ts
│   │   │       ├── linshi-email.ts
│   │   │       ├── tempmail-lol.ts
│   │   │       ├── chatgpt-org-uk.ts
│   │   │       ├── tempmail-la.ts
│   │   │       ├── temp-mail-io.ts
│   │   │       ├── awamail.ts
│   │   │       ├── mail-tm.ts
│   │   │       └── dropmail.ts
│   │   ├── demo/               # 演示代码
│   │   └── test/               # 测试代码
│   └── go/                     # Go SDK
│       ├── client.go           # 入口文件
│       ├── types.go            # 类型定义
│       ├── normalize.go        # 标准化转换工具
│       ├── provider_*.go       # 各渠道实现
│       ├── demo/               # 演示代码
│       └── example/            # 示例代码
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
# 创建并推送 tag
git tag v1.0.0
git push origin v1.0.0
```

这将自动：
1. 构建并验证 npm SDK 和 Go SDK
2. 发布 npm 包到 npmjs.org
3. 创建 GitHub Release（自动生成变更日志）
4. Go 模块可通过 tag 直接使用

### 配置 Secrets

在 GitHub 仓库 Settings → Secrets and variables → Actions 中添加：

| Secret | 说明 |
|--------|------|
| `NPM_TOKEN` | npm 访问令牌（用于发布 npm 包） |

## 🛠️ 开发

### npm SDK

```bash
cd sdk/npm
npm install
npm run build

# 类型检查
npx tsc --noEmit

# 运行 demo（交互式选择渠道）
npx ts-node demo/poll-emails.ts

# 运行测试
npm test
```

### Go SDK

```bash
cd sdk/go

# 编译检查
go build ./...

# 代码格式检查
gofmt -d .

# 运行 demo
cd demo && go run poll_emails.go
```

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

### 添加新的渠道提供商

1. 在 `sdk/npm/src/providers/` 下新建提供商文件，导出 `generateEmail()` 和 `getEmails()`
2. 在 `sdk/go/` 下新建 `provider_xxx.go`，实现 `xxxGenerate()` 和 `xxxGetEmails()`
3. 在 `types` 中添加 Channel 类型
4. 在 `index.ts` / `client.go` 中注册新渠道
5. 使用 `normalizeEmail()` 标准化返回数据
6. 更新 README 文档

## 📄 许可证

本项目基于 [GPL-3.0](LICENSE) 许可证开源。

```
Copyright (C) 2026 XxxXTeam

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
```
