# tempmail-sdk (Kotlin)

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

Kotlin 语言临时邮箱 SDK，聚合 **279** 个第三方临时邮箱渠道标识，与 Go / npm / Rust / Python / C / PHP 等其余各端保持一致；所有渠道返回**统一标准化邮件格式**。基于 Kotlin 协程（`suspend` 函数）与 Ktor 客户端，序列化使用 `kotlinx.serialization`。

## 安装

要求 JDK 17+。通过 Gradle 添加依赖：

```kotlin
dependencies {
    implementation("io.github.xxxxteam:tempmail-sdk:0.1.0")
}
```

包名为 `com.xxxxteam.tempmail`。

## 快速开始

所有网络操作均为 `suspend` 函数，需在协程作用域内调用。

### 使用 Client（推荐）

`Client` 自动缓存邮箱信息与内部 token，用户无需手动传递。

```kotlin
import com.xxxxteam.tempmail.TempMail
import kotlinx.coroutines.runBlocking

fun main() = runBlocking {
    val client = TempMail.client("tempmail")

    // 创建临时邮箱
    val info = client.generate()
    println("渠道: ${info.channel}")
    println("邮箱: ${info.email}")

    // 获取邮件（token 由 SDK 内部维护）
    val emails = client.getEmails()
    println("收到 ${emails.size} 封邮件")

    for (mail in emails) {
        println("发件人: ${mail.from}")
        println("主题:   ${mail.subject}")
        println("正文:   ${mail.body}")
        println("时间:   ${mail.date}")
    }
}
```

### 使用函数式 API

```kotlin
import com.xxxxteam.tempmail.TempMail
import kotlinx.coroutines.runBlocking

fun main() = runBlocking {
    // 列出所有渠道
    for (ch in TempMail.listChannels()) {
        println("渠道: ${ch.channel}, 名称: ${ch.name}, 网站: ${ch.website}")
    }

    // 生成邮箱：channel 为空时随机选择
    val info = TempMail.generateEmail("mail-tm")

    // 指定渠道失败后随机 fallback 尝试其他渠道
    val withFallback = TempMail.generateWithFallback("tempmail", maxFallback = 3)

    // 获取邮件
    val emails = TempMail.getEmails(info)
    for (mail in emails) {
        println("${mail.id} ${mail.subject}")
    }
}
```

### 标准化邮件格式（`Email`）

| 字段 | 类型 | 说明 |
|------|------|------|
| `id` | `String` | 邮件唯一标识 |
| `from` | `String` | 发件人地址 |
| `to` | `String` | 收件人地址 |
| `subject` | `String` | 邮件主题 |
| `body` | `String` | 邮件正文（纯文本或 HTML） |
| `date` | `String` | 收信时间 |

## 配置

支持环境变量零代码配置，也可用 DSL 风格在代码中设置（代码配置优先级高于环境变量）：

```kotlin
TempMail.configure {
    proxy = "http://127.0.0.1:7890"
    timeout = 30
    insecure = false
}
```

### 环境变量

| 变量 | 说明 |
|------|------|
| `TEMPMAIL_PROXY` | HTTP/SOCKS5 代理 URL |
| `TEMPMAIL_TIMEOUT` | 超时秒数（默认 15） |
| `TEMPMAIL_INSECURE` | `1`/`true`/`yes` 跳过 TLS 验证 |
| `TEMPMAIL_TELEMETRY_ENABLED` | `false`/`0`/`no` 关闭匿名遥测 |
| `TEMPMAIL_TELEMETRY_URL` | 自定义遥测端点 |
| `DROPMAIL_AUTH_TOKEN` | DropMail GraphQL `af_` 令牌 |
| `APIHZ_ID` / `APIHZ_KEY` | apihz（接口盒子）渠道凭据 |

## 许可证

GPL-3.0
