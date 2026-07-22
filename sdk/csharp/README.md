# tempmail-sdk-csharp

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

.NET 临时邮箱 SDK，公开 **279** 个 `channel` 渠道标识，聚合 100+ 个第三方临时邮箱服务商，所有渠道返回**统一标准化格式**。渠道标识与顺序与 Go / npm / Rust / Python / C 等其余各端保持一致（十端同步）；随机生成邮箱时在本端独立打乱尝试顺序。

## 安装

目标框架 `net8.0`。

```bash
dotnet add package XxxXTeam.TempMail --version 0.1.0
```

本地构建：

```bash
cd sdk/csharp/src
dotnet build
```

## 快速开始

### 使用 Client（推荐）

`TempEmailClient` 自动管理 `EmailInfo` 与内部 token，用户无需手动传递。

```csharp
using XxxXTeam.TempMail;

var client = new TempEmailClient();

// 创建临时邮箱（指定渠道，不指定则随机 fallback）
var info = client.Generate(new GenerateEmailOptions { Channel = "tempmail" });
Console.WriteLine($"渠道: {info!.Channel}");
Console.WriteLine($"邮箱: {info.Email}");

// 获取邮件（Token 等由 SDK 内部自动维护）
var result = client.GetEmails();
Console.WriteLine($"收到 {result.Emails.Count} 封邮件");

foreach (var email in result.Emails)
{
    Console.WriteLine($"发件人: {email.From}");
    Console.WriteLine($"主题: {email.Subject}");
    Console.WriteLine($"内容: {email.Text}");
    Console.WriteLine($"时间: {email.Date}");
    Console.WriteLine($"已读: {email.IsRead}");
    Console.WriteLine($"附件: {email.Attachments.Count} 个");
}
```

### 使用函数式 API

```csharp
using XxxXTeam.TempMail;

// 列出所有渠道
foreach (var ch in TempMail.ListChannels())
    Console.WriteLine($"渠道: {ch.Channel}, 名称: {ch.Name}, 网站: {ch.Website}");

// 从随机渠道创建邮箱
var info = TempMail.GenerateEmail();

// 从指定渠道创建邮箱
var info2 = TempMail.GenerateEmail(new GenerateEmailOptions { Channel = "mail-tm" });

// 自定义有效期（仅 tempmail 渠道，单位分钟）
var info3 = TempMail.GenerateEmail(
    new GenerateEmailOptions { Channel = "tempmail", Duration = 60 });

// 获取邮件列表
var result = TempMail.GetEmails(info!);
```

> **提示：** Token 等认证信息由 SDK 内部自动维护，用户无需关心。

## 标准化邮件格式

所有渠道返回的邮件均映射为统一的 `Email` 对象：

| 属性 | 类型 | 说明 |
|------|------|------|
| `Id` | `string` | 邮件唯一标识 |
| `From` | `string` | 发件人地址 |
| `To` | `string` | 收件人地址 |
| `Subject` | `string` | 邮件主题 |
| `Text` | `string` | 纯文本内容 |
| `Html` | `string` | HTML 内容 |
| `Date` | `string` | ISO 8601 格式日期 |
| `IsRead` | `bool` | 是否已读 |
| `Attachments` | `List<EmailAttachment>` | 附件列表 |

## 配置

支持通过环境变量零代码配置，作用于所有 HTTP 请求：

| 变量 | 说明 |
|------|------|
| `TEMPMAIL_PROXY` | HTTP/SOCKS5 代理 URL |
| `TEMPMAIL_TIMEOUT` | 超时秒数（默认 15） |
| `TEMPMAIL_INSECURE` | `1`/`true` 跳过 TLS 验证 |
| `TEMPMAIL_TELEMETRY_ENABLED` | `false`/`0`/`no` 关闭匿名遥测 |
| `TEMPMAIL_TELEMETRY_URL` | 自定义遥测上报端点 |
| `DROPMAIL_AUTH_TOKEN` | DropMail GraphQL `af_` 令牌 |
| `APIHZ_ID` / `APIHZ_KEY` | apihz（接口盒子）渠道凭据，默认使用公共账号 |

## 许可证

GPL-3.0
