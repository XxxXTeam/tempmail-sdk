# temp-email-sdk-go

Go 语言临时邮箱 SDK，支持多个邮箱服务提供商。

## 许可证

GPL-3.0

## 安装

```bash
go get github.com/XxxXTeam/tempmail-sdk/sdk/go
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

```go
package main

import (
    "fmt"
    tempemail "github.com/temp-email/sdk-go"
)

func main() {
    // 获取所有支持的渠道
    channels := tempemail.ListChannels()
    for _, ch := range channels {
        fmt.Printf("渠道: %s, 名称: %s, 网站: %s\n", ch.Channel, ch.Name, ch.Website)
    }

    // 获取指定渠道信息
    info, ok := tempemail.GetChannelInfo(tempemail.ChannelTempmail)
    if ok {
        fmt.Printf("渠道: %s, 网站: %s\n", info.Name, info.Website)
    }
}
```

### 获取邮箱

```go
package main

import (
    "fmt"
    tempemail "github.com/temp-email/sdk-go"
)

func main() {
    // 从随机渠道获取邮箱
    emailInfo, err := tempemail.GenerateEmail(nil)
    if err != nil {
        panic(err)
    }
    fmt.Printf("渠道: %s, 邮箱: %s\n", emailInfo.Channel, emailInfo.Email)

    // 从指定渠道获取邮箱
    emailInfo2, err := tempemail.GenerateEmail(&tempemail.GenerateEmailOptions{
        Channel: tempemail.ChannelLinshiEmail,
    })
    if err != nil {
        panic(err)
    }
    fmt.Printf("渠道: %s, 邮箱: %s\n", emailInfo2.Channel, emailInfo2.Email)
}
```

### 获取邮件

```go
package main

import (
    "fmt"
    tempemail "github.com/temp-email/sdk-go"
)

func main() {
    // 查询邮件（需要传递渠道和邮箱）
    result, err := tempemail.GetEmails(tempemail.GetEmailsOptions{
        Channel: tempemail.ChannelTempmail,
        Email:   "xxx@ibymail.com",
    })
    if err != nil {
        panic(err)
    }
    fmt.Printf("邮件数量: %d\n", len(result.Emails))

    // tempmail-lol 渠道需要传递 token
    result2, err := tempemail.GetEmails(tempemail.GetEmailsOptions{
        Channel: tempemail.ChannelTempmailLol,
        Email:   "xxx@chessgameland.com",
        Token:   "your-token-here",
    })
    if err != nil {
        panic(err)
    }
    fmt.Printf("邮件数量: %d\n", len(result2.Emails))
}
```

### 使用 Client

```go
package main

import (
    "fmt"
    tempemail "github.com/temp-email/sdk-go"
)

func main() {
    client := tempemail.NewClient()

    // 获取邮箱
    emailInfo, err := client.Generate(&tempemail.GenerateEmailOptions{
        Channel: tempemail.ChannelTempmail,
    })
    if err != nil {
        panic(err)
    }
    fmt.Printf("渠道: %s\n", emailInfo.Channel)
    fmt.Printf("邮箱: %s\n", emailInfo.Email)

    // 获取邮件
    result, err := client.GetEmails()
    if err != nil {
        panic(err)
    }
    fmt.Printf("收到 %d 封邮件\n", len(result.Emails))
}
```

## API

### ListChannels()

获取所有支持的渠道列表。

**返回值:** `[]ChannelInfo`
- `Channel` - 渠道标识
- `Name` - 渠道名称
- `Website` - 服务商网站

### GetChannelInfo(channel)

获取指定渠道信息。

**参数:**
- `channel` - 渠道标识

**返回值:** `(ChannelInfo, bool)`

### GenerateEmail(opts)

生成临时邮箱地址。

**参数:**
- `Channel` - 指定渠道（可选，不指定则随机选择）
- `Duration` - 邮箱有效期，单位分钟（仅 `tempmail` 渠道支持）
- `Domain` - 指定域名（仅 `tempmail-lol` 渠道支持）

**返回值:** `*EmailInfo`
- `Channel` - 渠道名称
- `Email` - 邮箱地址
- `Token` - 访问收件箱的令牌（仅 `tempmail-lol` 渠道返回）
- `ExpiresAt` - 过期时间

### GetEmails(opts)

获取指定邮箱的邮件列表。

**参数（必填）:**
- `Channel` - 渠道名称
- `Email` - 邮箱地址
- `Token` - 令牌（`tempmail-lol` 渠道必填）

**返回值:** `*GetEmailsResult`
- `Channel` - 渠道名称
- `Email` - 邮箱地址
- `Emails` - 邮件切片
- `Success` - 是否成功

## 环境要求

- Go 1.21+
