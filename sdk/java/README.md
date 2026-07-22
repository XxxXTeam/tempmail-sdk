# tempmail-sdk-java

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

Java 语言临时邮箱 SDK，公开 **279** 个 `channel` 渠道标识，聚合 100+ 个第三方临时邮箱服务商，所有渠道返回**统一标准化格式**。渠道标识与顺序与 Go / npm / Rust / Python / C 等其余各端保持一致（十端同步）；随机生成邮箱时在本端独立打乱尝试顺序。

## 安装

需 Java 17+，构建工具为 Maven。

在 `pom.xml` 中添加依赖：

```xml
<dependency>
    <groupId>io.github.xxxxteam</groupId>
    <artifactId>tempmail-sdk</artifactId>
    <version>0.1.0</version>
</dependency>
```

本地构建与安装到本地仓库：

```bash
cd sdk/java
mvn package            # 构建 jar
mvn install            # 安装到本地 Maven 仓库
```

## 快速开始

### 使用 Client（推荐）

`TempEmailClient` 自动管理 `EmailInfo` 与内部 token，用户无需手动传递。

```java
import com.xxxxteam.tempmail.*;

public class Demo {
    public static void main(String[] args) {
        TempEmailClient client = new TempEmailClient();

        // 创建临时邮箱（指定渠道，不指定则随机 fallback）
        EmailInfo info = client.generate(
                new GenerateEmailOptions().setChannel("tempmail"));
        System.out.println("渠道: " + info.getChannel());
        System.out.println("邮箱: " + info.getEmail());

        // 获取邮件（Token 等由 SDK 内部自动维护）
        GetEmailsResult result = client.getEmails();
        System.out.println("收到 " + result.getEmails().size() + " 封邮件");

        for (Email email : result.getEmails()) {
            System.out.println("发件人: " + email.getFrom());
            System.out.println("主题: " + email.getSubject());
            System.out.println("内容: " + email.getText());
            System.out.println("时间: " + email.getDate());
            System.out.println("已读: " + email.isRead());
            System.out.println("附件: " + email.getAttachments().size() + " 个");
        }
    }
}
```

### 使用函数式 API

```java
import com.xxxxteam.tempmail.*;
import java.util.List;

// 列出所有渠道
List<ChannelInfo> channels = TempMail.listChannels();
for (ChannelInfo ch : channels) {
    System.out.printf("渠道: %s, 名称: %s, 网站: %s%n",
            ch.getChannel(), ch.getName(), ch.getWebsite());
}

// 从随机渠道创建邮箱
EmailInfo info = TempMail.generateEmail();

// 从指定渠道创建邮箱
EmailInfo info2 = TempMail.generateEmail(
        new GenerateEmailOptions().setChannel("mail-tm"));

// 自定义有效期（仅 tempmail 渠道，单位分钟）
EmailInfo info3 = TempMail.generateEmail(
        new GenerateEmailOptions().setChannel("tempmail").setDuration(60));

// 获取邮件列表
GetEmailsResult result = TempMail.getEmails(info);
```

> **提示：** Token 等认证信息由 SDK 内部自动维护，用户无需关心。

## 标准化邮件格式

所有渠道返回的邮件均映射为统一的 `Email` 对象：

| 字段 | 类型 | 说明 |
|------|------|------|
| `getId()` | `String` | 邮件唯一标识 |
| `getFrom()` | `String` | 发件人地址 |
| `getTo()` | `String` | 收件人地址 |
| `getSubject()` | `String` | 邮件主题 |
| `getText()` | `String` | 纯文本内容 |
| `getHtml()` | `String` | HTML 内容 |
| `getDate()` | `String` | ISO 8601 格式日期 |
| `isRead()` | `boolean` | 是否已读 |
| `getAttachments()` | `List<EmailAttachment>` | 附件列表 |

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
