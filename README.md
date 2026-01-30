# 临时邮箱 SDK

[![CI](https://github.com/XxxXTeam/tempmail-sdk/actions/workflows/ci.yml/badge.svg)](https://github.com/XxxXTeam/tempmail-sdk/actions/workflows/ci.yml)
[![npm version](https://badge.fury.io/js/tempmail-sdk.svg)](https://www.npmjs.com/package/tempmail-sdk)
[![Go Reference](https://pkg.go.dev/badge/github.com/XxxXTeam/tempmail-sdk/sdk/go.svg)](https://pkg.go.dev/github.com/XxxXTeam/tempmail-sdk/sdk/go)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

支持多个临时邮箱服务商的 SDK，提供 npm (TypeScript) 和 Go 两种版本。

## ✨ 特性

- 🌐 支持多个临时邮箱服务商
- 📦 提供 npm 和 Go 两种 SDK
- 🔄 支持邮箱生成和邮件轮询
- 📝 完整的 TypeScript 类型定义
- 🚀 简单易用的 API

## 📋 支持的渠道

| 渠道 | 服务商 | 状态 |
|------|--------|------|
| `tempmail` | tempmail.ing | ✅ |
| `linshi-email` | linshi-email.com | ✅ |
| `tempmail-lol` | tempmail.lol | ✅ |
| `chatgpt-org-uk` | mail.chatgpt.org.uk | ✅ |

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

### npm

```typescript
import { listChannels, generateEmail, getEmails } from 'tempmail-sdk';

// 列出所有渠道
const channels = listChannels();
console.log(channels);

// 获取临时邮箱
const emailInfo = await generateEmail({ channel: 'tempmail' });
console.log('邮箱:', emailInfo.email);

// 获取邮件
const result = await getEmails({
  channel: emailInfo.channel,
  email: emailInfo.email,
});
console.log('邮件:', result.emails);
```

### Go

```go
package main

import (
    "fmt"
    tempemail "github.com/XxxXTeam/tempmail-sdk/sdk/go"
)

func main() {
    // 列出所有渠道
    channels := tempemail.ListChannels()
    fmt.Println(channels)

    // 获取临时邮箱
    emailInfo, err := tempemail.GenerateEmail(&tempemail.GenerateEmailOptions{
        Channel: tempemail.ChannelTempmail,
    })
    if err != nil {
        panic(err)
    }
    fmt.Println("邮箱:", emailInfo.Email)

    // 获取邮件
    result, err := tempemail.GetEmails(tempemail.GetEmailsOptions{
        Channel: emailInfo.Channel,
        Email:   emailInfo.Email,
    })
    if err != nil {
        panic(err)
    }
    fmt.Println("邮件:", result.Emails)
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
│   ├── npm/              # npm SDK (TypeScript)
│   │   ├── src/          # 源代码
│   │   ├── demo/         # 演示代码
│   │   └── README.md     # npm SDK 文档
│   └── go/               # Go SDK
│       ├── demo/         # 演示代码
│       └── README.md     # Go SDK 文档
├── .github/
│   └── workflows/
│       ├── ci.yml        # CI 工作流
│       └── release.yml   # 发布工作流
├── LICENSE               # GPL-3.0 许可证
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
1. ✅ 发布 npm 包到 npmjs.org
2. ✅ 创建 GitHub Release
3. ✅ Go 模块可通过 tag 直接使用

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

# 运行 demo
npx ts-node demo/poll-emails.ts
```

### Go SDK

```bash
cd sdk/go

# 运行 demo
cd demo && go run poll_emails.go
```

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

## 📄 许可证

本项目基于 [GPL-3.0](LICENSE) 许可证开源。

```
Copyright (C) 2026 XxxXTeam

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
```
