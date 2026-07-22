# tempmail-sdk-ruby

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

Ruby 语言临时邮箱 SDK，公开 **279** 个 `channel` 渠道标识，聚合 100+ 个第三方临时邮箱服务商，所有渠道返回**统一标准化格式**。渠道标识与顺序与 Go / npm / Rust / Python / C 等其余各端保持一致（十端同步）；随机生成邮箱时在本端独立打乱尝试顺序。

## 安装

需 Ruby 3.0+。

```bash
gem install tempmail_sdk
```

或在 `Gemfile` 中添加：

```ruby
gem "tempmail_sdk", "~> 1.0"
```

## 快速开始

### 使用 Client（推荐）

`TempEmailClient` 自动管理 `EmailInfo` 与内部 token，用户无需手动传递。

```ruby
require "tempmail_sdk"

client = TempmailSdk::TempEmailClient.new

# 创建临时邮箱（指定渠道，不指定则随机 fallback）
info = client.generate(TempmailSdk::GenerateEmailOptions.new(channel: "tempmail"))
puts "渠道: #{info.channel}"
puts "邮箱: #{info.email}"

# 获取邮件（Token 等由 SDK 内部自动维护）
result = client.get_emails
puts "收到 #{result.emails.size} 封邮件"

result.emails.each do |email|
  puts "发件人: #{email.from_addr}"
  puts "主题: #{email.subject}"
  puts "内容: #{email.text}"
  puts "时间: #{email.date}"
  puts "已读: #{email.is_read}"
  puts "附件: #{email.attachments.size} 个"
end
```

### 使用函数式 API

```ruby
require "tempmail_sdk"

# 列出所有渠道
TempmailSdk.list_channels.each do |ch|
  puts "渠道: #{ch.channel}, 名称: #{ch.name}, 网站: #{ch.website}"
end

# 从随机渠道创建邮箱
info = TempmailSdk.generate_email

# 从指定渠道创建邮箱
info2 = TempmailSdk.generate_email(TempmailSdk::GenerateEmailOptions.new(channel: "mail-tm"))

# 自定义有效期（仅 tempmail 渠道，单位分钟）
info3 = TempmailSdk.generate_email(
  TempmailSdk::GenerateEmailOptions.new(channel: "tempmail", duration: 60)
)

# 获取邮件列表
result = TempmailSdk.get_emails(info)
```

> **提示：** Token 等认证信息由 SDK 内部自动维护，用户无需关心。

## 标准化邮件格式

所有渠道返回的邮件均映射为统一的 `Email` 对象：

| 属性 | 类型 | 说明 |
|------|------|------|
| `id` | `String` | 邮件唯一标识 |
| `from_addr` | `String` | 发件人地址 |
| `to` | `String` | 收件人地址 |
| `subject` | `String` | 邮件主题 |
| `text` | `String` | 纯文本内容 |
| `html` | `String` | HTML 内容 |
| `date` | `String` | ISO 8601 格式日期 |
| `is_read` | `Boolean` | 是否已读 |
| `attachments` | `Array<EmailAttachment>` | 附件列表 |

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

也可通过代码设置：

```ruby
TempmailSdk.set_config(proxy: "http://127.0.0.1:7890", timeout: 30, insecure: true)
```

## 许可证

GPL-3.0
