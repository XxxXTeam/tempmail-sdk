# tempmail-sdk-go

[![Go Reference](https://pkg.go.dev/badge/github.com/XxxXTeam/tempmail-sdk/sdk/go.svg)](https://pkg.go.dev/github.com/XxxXTeam/tempmail-sdk/sdk/go)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

Go 语言临时邮箱 SDK，支持 **18** 个邮箱服务提供商，所有渠道返回**统一标准化格式**。

## 安装

```bash
go get github.com/XxxXTeam/tempmail-sdk/sdk/go
```

## 支持的渠道

| 渠道 | 服务商 | 常量 | 说明 |
|------|--------|------|------|
| `tempmail` | tempmail.ing | `ChannelTempmail` | 支持自定义有效期 |
| `linshi-email` | linshi-email.com | `ChannelLinshiEmail` | |
| `tempmail-lol` | tempmail.lol | `ChannelTempmailLol` | 支持指定域名 |
| `chatgpt-org-uk` | mail.chatgpt.org.uk | `ChannelChatgptOrgUk` | Inbox Token 等请求头已封装 |
| `temp-mail-io` | temp-mail.io | `ChannelTempMailIO` | |
| `awamail` | awamail.com | `ChannelAwamail` | Session Cookie 自动管理 |
| `temporary-email-org` | temporary-email.org | `ChannelTemporaryEmailOrg` | `GET /zh/messages` Cookie + XHR 收信 |
| `mail-tm` | mail.tm | `ChannelMailTm` | 自动注册（`api.mail.tm`），Bearer Token |
| `dropmail` | dropmail.me | `ChannelDropmail` | GraphQL，Session ID |
| `guerrillamail` | guerrillamail.com | `ChannelGuerrillaMail` | 公开 JSON API |
| `maildrop` | maildrop.cc | `ChannelMaildrop` | GraphQL；`data` 为 MIME 源码，解析 plain/multipart/Base64/QP，HTML 兜底 `text` |
| `smail-pw` | smail.pw | `ChannelSmailPw` | `_root.data` + `__session`；RSC/Flight 解析 |
| `boomlify` | boomlify.com | `ChannelBoomlify` | `domains/public` + `emails/public/create`，地址 `{UUID}@{域名}` |
| `minmail` | minmail.app | `ChannelMinmail` | Token 为 JSON（visitorId + ck 等） |
| `vip-215` | vip.215.im | `ChannelVip215` | `POST` 建箱 + WebSocket；无正文时 synthetic 兜底 |
| `anonbox` | anonbox.net | `ChannelAnonbox` | `GET /en/` 解析 HTML + mbox 收信 |
| `fake-legal` | fake.legal | `ChannelFakeLegal` | `/api/domains` + `/api/inbox/new`；可选域名 |

> **提示：** Token 等认证信息由 SDK 内部自动维护，用户无需关心。

## 快速开始

### 使用 Client（推荐）

```go
package main

import (
    "fmt"
    tempemail "github.com/XxxXTeam/tempmail-sdk/sdk/go"
)

func main() {
    client := tempemail.NewClient()

    // 获取临时邮箱（可指定渠道，不指定则随机）
    emailInfo, err := client.Generate(&tempemail.GenerateEmailOptions{
        Channel: tempemail.ChannelTempmail,
    })
    if err != nil {
        panic(err)
    }
    fmt.Printf("渠道: %s\n", emailInfo.Channel)
    fmt.Printf("邮箱: %s\n", emailInfo.Email)

    // 获取邮件（Token 等由 SDK 内部自动维护）
    result, err := client.GetEmails(nil)
    if err != nil {
        panic(err)
    }
    fmt.Printf("收到 %d 封邮件\n", len(result.Emails))

    for _, email := range result.Emails {
        fmt.Printf("发件人: %s\n", email.From)
        fmt.Printf("主题: %s\n", email.Subject)
        fmt.Printf("内容: %s\n", email.Text)
        fmt.Printf("时间: %s\n", email.Date)
        fmt.Printf("已读: %v\n", email.IsRead)
        fmt.Printf("附件: %d 个\n", len(email.Attachments))
    }
}
```

### 使用函数式 API

#### 列出所有渠道

```go
package main

import (
    "fmt"
    tempemail "github.com/XxxXTeam/tempmail-sdk/sdk/go"
)

func main() {
    channels := tempemail.ListChannels()
    for _, ch := range channels {
        fmt.Printf("渠道: %s, 名称: %s, 网站: %s\n", ch.Channel, ch.Name, ch.Website)
    }

    info, ok := tempemail.GetChannelInfo(tempemail.ChannelTempmail)
    if ok {
        fmt.Printf("渠道: %s, 网站: %s\n", info.Name, info.Website)
    }
}
```

#### 获取邮箱

```go
// 从随机渠道获取邮箱
emailInfo, err := tempemail.GenerateEmail(nil)

// 从指定渠道获取邮箱
emailInfo, err := tempemail.GenerateEmail(&tempemail.GenerateEmailOptions{
    Channel: tempemail.ChannelLinshiEmail,
})

// 自定义有效期（仅 tempmail 渠道）
emailInfo, err := tempemail.GenerateEmail(&tempemail.GenerateEmailOptions{
    Channel:  tempemail.ChannelTempmail,
    Duration: 60, // 60 分钟
})
```

#### 获取邮件

```go
// 方式1：通过 EmailInfo 方法获取（推荐）
emailInfo, _ := tempemail.GenerateEmail(&tempemail.GenerateEmailOptions{
    Channel: tempemail.ChannelMailTm,
})
result, err := emailInfo.GetEmails(nil)

// 方式2：通过函数式 API 获取
result, err := tempemail.GetEmails(emailInfo, nil)

// 方式3：自定义重试配置
result, err := tempemail.GetEmails(emailInfo, &tempemail.GetEmailsOptions{
    Retry: &tempemail.RetryOptions{MaxRetries: 3},
})

// 所有邮件使用统一格式
for _, email := range result.Emails {
    fmt.Println(email.ID)          // 邮件 ID
    fmt.Println(email.From)        // 发件人
    fmt.Println(email.To)          // 收件人
    fmt.Println(email.Subject)     // 主题
    fmt.Println(email.Text)        // 纯文本
    fmt.Println(email.HTML)        // HTML
    fmt.Println(email.Date)        // ISO 日期
    fmt.Println(email.IsRead)      // 是否已读
    fmt.Println(email.Attachments) // 附件列表
}
```

## 代理与 HTTP 配置

SDK 支持全局配置代理、超时等 HTTP 客户端参数，也可通过环境变量零代码配置：

```go
import (
    "time"
    tempemail "github.com/XxxXTeam/tempmail-sdk/sdk/go"
)

// 一行跳过 SSL 验证
tempemail.SetConfig(tempemail.SDKConfig{Insecure: true})

// 设置代理
tempemail.SetConfig(tempemail.SDKConfig{
    Proxy: "http://127.0.0.1:7890",
})

// 设置代理 + 超时 + 跳过 SSL 验证
tempemail.SetConfig(tempemail.SDKConfig{
    Proxy:    "socks5://127.0.0.1:1080",
    Timeout:  30 * time.Second,
    Insecure: true,
})
```

**配置项：**

| 字段 | 类型 | 说明 |
|------|------|------|
| `Proxy` | `string` | 代理 URL（http/https/socks5） |
| `Timeout` | `time.Duration` | 全局超时，默认 15s |
| `Insecure` | `bool` | 跳过 SSL 验证（调试用） |

**环境变量（无需修改代码）：**

```bash
export TEMPMAIL_PROXY="http://127.0.0.1:7890"
export TEMPMAIL_INSECURE=1
export TEMPMAIL_TIMEOUT=30
```

## API 参考

### ListChannels()

获取所有支持的渠道列表。

**返回值:** `[]ChannelInfo`

### GetChannelInfo(channel)

获取指定渠道信息。

**返回值:** `(ChannelInfo, bool)`

### GenerateEmail(opts)

生成临时邮箱地址。

**参数 (`*GenerateEmailOptions`):**

| 字段 | 类型 | 说明 |
|------|------|------|
| `Channel` | `Channel` | 指定渠道（可选，不指定则随机） |
| `Duration` | `int` | 有效期分钟数（仅 `tempmail` 渠道） |
| `Domain` | `*string` | 指定域名（仅 `tempmail-lol` 渠道） |

**返回值:** `*EmailInfo`

| 字段 | 类型 | 说明 |
|------|------|------|
| `Channel` | `Channel` | 渠道标识 |
| `Email` | `string` | 邮箱地址 |
| `ExpiresAt` | `any` | 过期时间 |
| `CreatedAt` | `string` | 创建时间 |

> Token 等认证信息由 SDK 内部维护，不对外暴露。

### GetEmails(info, opts)

获取邮件列表。Channel/Email/Token 等由 SDK 从 `EmailInfo` 中自动获取。

**参数:**

| 参数 | 类型 | 说明 |
|------|------|------|
| `info` | `*EmailInfo` | `GenerateEmail()` 返回的邮箱信息（必填） |
| `opts` | `*GetEmailsOptions` | 可选配置，`nil` 使用默认值 |

**`GetEmailsOptions` 字段:**

| 字段 | 类型 | 说明 |
|------|------|------|
| `Retry` | `*RetryOptions` | 重试配置，nil 使用默认值 |

**返回值:** `*GetEmailsResult`

| 字段 | 类型 | 说明 |
|------|------|------|
| `Channel` | `Channel` | 渠道标识 |
| `Email` | `string` | 邮箱地址 |
| `Emails` | `[]Email` | 标准化邮件切片 |
| `Success` | `bool` | 是否成功 |

### 标准化邮件格式

所有渠道返回的邮件均使用统一的 `Email` 结构体：

```go
type Email struct {
    ID          string            `json:"id"`          // 邮件唯一标识
    From        string            `json:"from"`        // 发件人邮箱地址
    To          string            `json:"to"`          // 收件人邮箱地址
    Subject     string            `json:"subject"`     // 邮件主题
    Text        string            `json:"text"`        // 纯文本内容
    HTML        string            `json:"html"`        // HTML 内容
    Date        string            `json:"date"`        // ISO 8601 格式日期
    IsRead      bool              `json:"isRead"`      // 是否已读
    Attachments []EmailAttachment `json:"attachments"` // 附件列表
}

type EmailAttachment struct {
    Filename    string `json:"filename"`              // 文件名
    Size        int64  `json:"size,omitempty"`        // 文件大小（字节）
    ContentType string `json:"contentType,omitempty"` // MIME 类型
    URL         string `json:"url,omitempty"`         // 下载地址
}
```

## 环境要求

- Go 1.21+

## 许可证

GPL-3.0
