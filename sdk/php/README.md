# tempmail-sdk (PHP)

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

PHP 语言临时邮箱 SDK，聚合 **279** 个第三方临时邮箱渠道标识，与 Go / npm / Rust / Python / C / Kotlin 等其余各端保持一致；所有渠道返回**统一标准化邮件格式**。

## 安装

要求 PHP >= 8.1，依赖 `guzzlehttp/guzzle ^7.5` 与 `ext-json`。

> 本 SDK 未发布到 Composer / Packagist —— 它位于本 monorepo 的 `sdk/php/` 子目录，而 Composer 只能解析 `composer.json` 位于仓库根目录的包。请改用以下任一方式：

```bash
# 方式一：从 GitHub Releases 下载 tempmail-sdk-php-*.zip，解压后引入其 autoloader
unzip tempmail-sdk-php-*.zip -d tempmail-sdk-php
# 代码中：require 'tempmail-sdk-php/vendor/autoload.php';

# 方式二：clone 仓库后在 sdk/php 安装依赖
git clone https://github.com/XxxXTeam/tempmail-sdk.git
composer install -d tempmail-sdk/sdk/php
# 代码中：require 'tempmail-sdk/sdk/php/vendor/autoload.php';
```

命名空间为 `ChanhanzhanX\TempMail`（PSR-4，`src/`）。

## 快速开始

### 使用 Client（推荐）

`Client` 自动缓存邮箱信息与内部 token，用户无需手动传递。

```php
<?php

require 'vendor/autoload.php';

use ChanhanzhanX\TempMail\Client;
use ChanhanzhanX\TempMail\GenerateEmailOptions;

$client = new Client();

// 创建临时邮箱（可指定渠道，不指定则随机 fallback 尝试）
$info = $client->generate(new GenerateEmailOptions(channel: 'tempmail'));
if ($info === null) {
    exit("所有渠道均不可用\n");
}
printf("渠道: %s\n邮箱: %s\n", $info->channel, $info->email);

// 获取邮件（token 由 SDK 内部维护）
$result = $client->getEmails();
printf("收到 %d 封邮件\n", count($result->emails));

foreach ($result->emails as $email) {
    printf("发件人: %s\n", $email->fromAddr);
    printf("主题:   %s\n", $email->subject);
    printf("正文:   %s\n", $email->text);
    printf("时间:   %s\n", $email->date);
}
```

### 使用函数式 API

```php
<?php

use ChanhanzhanX\TempMail\TempMail;
use ChanhanzhanX\TempMail\GenerateEmailOptions;

// 列出所有渠道
foreach (TempMail::listChannels() as $ch) {
    printf("渠道: %s, 名称: %s, 网站: %s\n", $ch->channel, $ch->name, $ch->website);
}

// 生成邮箱（随机渠道）
$info = TempMail::generateEmail();

// 指定渠道
$info = TempMail::generateEmail(new GenerateEmailOptions(channel: 'mail-tm'));

// 获取邮件
$result = TempMail::getEmails($info);
foreach ($result->emails as $email) {
    echo $email->id, ' ', $email->subject, PHP_EOL;
}
```

### 标准化邮件格式（`Email`）

| 字段 | 类型 | 说明 |
|------|------|------|
| `id` | `string` | 邮件唯一标识 |
| `fromAddr` | `string` | 发件人地址 |
| `to` | `string` | 收件人地址 |
| `subject` | `string` | 邮件主题 |
| `text` | `string` | 纯文本内容 |
| `html` | `string` | HTML 内容 |
| `date` | `string` | 接收日期（ISO 8601） |
| `isRead` | `bool` | 是否已读 |
| `attachments` | `EmailAttachment[]` | 附件列表 |

## 配置

支持通过环境变量零代码配置，也可用 `Config` + `ConfigStore` 在代码中设置：

```php
<?php

use ChanhanzhanX\TempMail\Config;
use ChanhanzhanX\TempMail\ConfigStore;

ConfigStore::set(new Config(
    proxy: 'socks5://127.0.0.1:1080',
    timeout: 30,
    insecure: true,
));
```

### 环境变量

| 变量 | 说明 |
|------|------|
| `TEMPMAIL_PROXY` | HTTP/SOCKS5 代理 URL |
| `TEMPMAIL_TIMEOUT` | 超时秒数（默认 15） |
| `TEMPMAIL_INSECURE` | `1`/`true`/`yes` 跳过 SSL 验证 |
| `TEMPMAIL_TELEMETRY_ENABLED` | `false`/`0`/`no` 关闭匿名遥测 |
| `TEMPMAIL_TELEMETRY_URL` | 自定义遥测端点 |
| `DROPMAIL_AUTH_TOKEN` | DropMail GraphQL `af_` 令牌（别名 `DROPMAIL_API_TOKEN`） |
| `APIHZ_ID` / `APIHZ_KEY` | apihz（接口盒子）渠道凭据 |

## 许可证

GPL-3.0
