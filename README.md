# TempMail SDK

[English](README.md) | [中文](README.zh-CN.md)

[![CI](https://github.com/XxxXTeam/tempmail-sdk/actions/workflows/ci.yml/badge.svg)](https://github.com/XxxXTeam/tempmail-sdk/actions/workflows/ci.yml)
[![npm version](https://badge.fury.io/js/tempmail-sdk.svg)](https://www.npmjs.com/package/tempmail-sdk)
[![Go Reference](https://pkg.go.dev/badge/github.com/XxxXTeam/tempmail-sdk/sdk/go.svg)](https://pkg.go.dev/github.com/XxxXTeam/tempmail-sdk/sdk/go)
[![PyPI version](https://badge.fury.io/py/tempemail-sdk.svg)](https://pypi.org/project/tempemail-sdk/)
[![crates.io](https://img.shields.io/crates/v/tempmail-sdk.svg)](https://crates.io/crates/tempmail-sdk)
[![NuGet](https://img.shields.io/nuget/v/XxxXTeam.TempMail.svg)](https://www.nuget.org/packages/XxxXTeam.TempMail/)
[![Maven Central](https://img.shields.io/maven-central/v/io.github.xxxxteam/tempmail-sdk.svg)](https://central.sonatype.com/artifact/io.github.xxxxteam/tempmail-sdk)
[![Gem Version](https://badge.fury.io/rb/tempmail_sdk.svg)](https://rubygems.org/gems/tempmail_sdk)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

A ten-language temporary-email SDK (**Go, npm, Rust, Python, C, Java, C#, Ruby, PHP, Kotlin**) that exposes **279** `channel` identifiers, consolidated into roughly **110** independent providers. Fixed domains, bare domains, mirror domains and multiple domains of the same API all count as a single provider. Every SDK shares the same channel identifier strings, count and order (aligned with `.baseline_channels.txt`). All channels return a **unified, normalized email format**, so you never have to deal with the differences between individual upstream APIs.

## ✨ Features

- 🌐 **279 `channel` identifiers shared across all ten SDKs, consolidated into ~110 independent providers** — identifier strings, count and list order are identical on every platform (aligned with `.baseline_channels.txt`)
- 📐 **Unified normalized response format** — the email data structure is identical regardless of channel
- 📦 Ten SDKs: Go, npm (TypeScript), Rust, Python, C, Java, C#, Ruby, PHP, Kotlin
- 🔄 Email generation and inbox polling
- 📝 Full type definitions (TypeScript / Rust / Go / Java / C# / Kotlin)
- 🚀 Simple, ready-to-use API
- 🔌 Automatic token/session management (via the Client class)
- 🔁 Configurable **HTTP retry** for both email creation and inbox polling (backoff, max attempts, etc.; see each SDK's README for field names)
- 📊 **Anonymous telemetry** enabled by default: batches operation outcomes and retry metadata (no email content; email addresses in error strings are redacted); can be disabled or repointed via environment variable or global config
- 🏗️ Multi-platform prebuilt binaries (Go / Rust / C)
- 📡 All packages installable from public registries and GitHub

## 📋 Supported Channels

All SDKs share the same public channel set, count and list order (returned by `listChannels` / `ListChannels` / `list_channels` / `tm_list_channels()` and their equivalents). When generating a random email, each SDK independently shuffles its own attempt order — cross-SDK random order is neither required nor promised. Channel counts are measured by independent provider: fixed domains, bare domains, mirror domains and multiple domains of the same API are not counted separately.

Below are 30 representative channels. The complete list of all 279 identifiers is in [`.baseline_channels.txt`](.baseline_channels.txt).

<details>
<summary>Show 30 representative channels</summary>

| Channel | Provider | Auth | Notes |
|---------|----------|------|-------|
| `tempmail` | [tempmail.ing](https://tempmail.ing) | Email address | Supports custom expiry |
| `tempmail-cn` | [tempmail.cn](https://tempmail.cn) | Email address | Socket.IO event protocol; `domain` may be `tempmail.cn` or a custom domain resolving to the service |
| `ta-easy` | [ta-easy.com](https://www.ta-easy.com) | Token (session UUID) | REST create/list; upstream fields such as `mail_sender` / `mail_title` / `mail_body_*` are normalized into the unified `Email` |
| `10minute-one` | [10minutemail.one](https://10minutemail.one) | Token | SSR/JWT + Web API create and receive; `domain` is an optional parameter |
| `linshiyou` | [linshiyou.com](https://linshiyou.com) | Token (`NEXUS_TOKEN`) | `NEXUS_TOKEN` issued via Set-Cookie on creation; inbox and body parsed from HTML segments / iframe |
| `mffac` | [mffac.com](https://www.mffac.com) | Token (mailbox `id`) | REST create/list/detail; `textContent` / `htmlContent` mapped to unified body; 24h default |
| `tempmail-lol` | [tempmail.lol](https://tempmail.lol) | Token | Supports specifying a domain |
| `chatgpt-org-uk` | [mail.chatgpt.org.uk](https://mail.chatgpt.org.uk) | Inbox token | Site injects `__BROWSER_AUTH` into the HTML, parsed and used to create the mailbox |
| `temp-mail-io` | [temp-mail.io](https://temp-mail.io) | Token | REST via `api.internal.temp-mail.io/api/v3`; token maintained internally by the SDK |
| `mail-cx` | [mail.cx](https://mail.cx) | Client ID | Anonymous Web API; implicit address `{local}@{domain}`, long-poll inbox |
| `catchmail` | [catchmail.io](https://catchmail.io) | - | Public REST; `body.text` / `body.html` mapped to unified `text` / `html` |
| `mailforspam` | [mailforspam.com](https://mailforspam.com) | - | Public REST; `body_text` / `body_html` mapped to unified `text` / `html` |
| `tempmailc` | [tempmailc.com](https://tempmailc.com) | - | Public API `GET /api/v1/new` create, `.../inbox` list, `.../message` body |
| `mailnesia` | [mailnesia.com](https://mailnesia.com) | - | Any `{local}@mailnesia.com`; inbox and body parsed from HTML |
| `throwawaymail` | [throwawaymail.app](https://throwawaymail.app) | Token | Web API create and poll; token maintained internally |
| `mail-tm` | [mail.tm](https://mail.tm) | Bearer token | REST API with automatic account registration |
| `dropmail` | [dropmail.me](https://dropmail.me) | Session ID | GraphQL API |
| `guerrillamail` | [guerrillamail.com](https://guerrillamail.com) | Session | Public JSON API |
| `maildrop` | [maildrop.cx](https://maildrop.cx) | Token (full address) | REST; suffix list via `/api/suffixes.php`, optional `domain`; list-only, no per-message MIME |
| `moakt` | [moakt.com](https://www.moakt.com) | Token (`mok1:` + Base64 JSON) | HTML flow: create mailbox then parse messages per `/email/{uuid}` |
| `tempmail-plus` | [tempmail.plus](https://tempmail.plus) | - | Public REST; `mailto.plus` domain |
| `emailnator` | [emailnator.com](https://www.emailnator.com) | XSRF + Cookie | Generates Gmail/GoogleMail alias addresses; reads HTML body via `messageID` |
| `mailinator` | [mailinator.com](https://mailinator.com) | - | Public REST public inboxes and messages |
| `harakirimail` | [harakirimail.com](https://harakirimail.com) | - | Public REST; per-message body via `/api/v1/email/{id}` |
| `inboxes` | [inboxes.com](https://inboxes.com) | - | Public v2 API; `text` / `html` in message detail |
| `getnada` | [getnada.net](https://getnada.net) | Token | `POST /api/inbox/open`; detail includes `text_plain` / `html_sanitized` |
| `fakemail` | [fakemail.net](https://fakemail.net) | Token | `/index/index` create, `/index/refresh` list, `/index/email` detail |
| `temp-mail-org` | [temp-mail.org](https://temp-mail.org) | Token | REST + XSRF; `POST /en/option/change` create, `.../refresh` receive |
| `m2u` | [m2u.io](https://m2u.io) | Token | `POST /v1/mailboxes/auto`; `token` + `viewToken` form the internal token |
| `duckmail` | [duckmail.sbs](https://duckmail.sbs) | Bearer token | `GET /domains` + `POST /accounts` + `POST /token`; `GET /messages` list |

</details>

> **Tip:** When using the Client class, tokens/sessions are managed automatically. In the C SDK, the return order of `tm_list_channels()` matches the shared list, but the numeric order of the `tm_channel_t` enum constants differs — see `sdk/c/README.md`.

## 📦 Installation

### Go

```bash
go get github.com/XxxXTeam/tempmail-sdk/sdk/go
```

### npm / TypeScript

```bash
npm install tempmail-sdk
```

### Rust

```toml
[dependencies]
tempmail-sdk = "1.0.3"
```

### Python

```bash
pip install tempemail-sdk
```

### C

Download a prebuilt package from [GitHub Releases](https://github.com/XxxXTeam/tempmail-sdk/releases):

| Package | Platform |
|---------|----------|
| `c-sdk-linux-amd64.zip` | Linux x64 |
| `c-sdk-darwin-arm64.zip` | macOS ARM64 |
| `c-sdk-windows-amd64.zip` | Windows x64 |

Or build from source:

```bash
cd sdk/c
curl -L -o vendor/cJSON.h https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.h
curl -L -o vendor/cJSON.c https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.c
cmake -B build -S . && cmake --build build
```

### Java (Maven)

```xml
<dependency>
  <groupId>io.github.xxxxteam</groupId>
  <artifactId>tempmail-sdk</artifactId>
  <version>0.1.0</version>
</dependency>
```

### C# (.NET)

```bash
dotnet add package XxxXTeam.TempMail
```

### Ruby

```bash
gem install tempmail_sdk
```

### PHP

The PHP SDK is not published to Composer / Packagist (the SDK lives in the `sdk/php/` subdirectory of this monorepo, and Composer can only resolve a package whose `composer.json` sits at the repository root). Use one of the following instead:

```bash
# Option 1: download the packaged zip from GitHub Releases and require its autoloader
#   (see the tempmail-sdk-php-*.zip asset on the release page)
unzip tempmail-sdk-php-*.zip -d tempmail-sdk-php
# then in your code:
#   require 'tempmail-sdk-php/vendor/autoload.php';

# Option 2: clone the repo and load sdk/php directly
git clone https://github.com/XxxXTeam/tempmail-sdk.git
composer install -d tempmail-sdk/sdk/php
# then require 'tempmail-sdk/sdk/php/vendor/autoload.php';
```

### Kotlin (Gradle)

```kotlin
implementation("io.github.xxxxteam:tempmail-sdk:0.1.0")
```

## 🚀 Quick Start

### npm — using TempEmailClient (recommended)

Tokens/sessions are managed automatically and work for every channel:

```typescript
import { TempEmailClient } from 'tempmail-sdk';

const client = new TempEmailClient();

// 1. Get a temporary mailbox (specify a channel, or omit for random)
const emailInfo = await client.generate({ channel: 'tempmail' });
console.log('Email:', emailInfo.email);

// 2. Poll for messages
const result = await client.getEmails();
for (const email of result.emails) {
  console.log(`From: ${email.from}`);
  console.log(`Subject: ${email.subject}`);
  console.log(`Body: ${email.text}`);
  console.log(`Date: ${email.date}`);
}
```

### npm — functional API

```typescript
import { generateEmail, getEmails } from 'tempmail-sdk';

// 1. Get a temporary mailbox
const emailInfo = await generateEmail({ channel: 'mail-tm' });
if (!emailInfo) {
  // When no channel is specified, all fallback paths may fail
  throw new Error('Creation failed');
}
console.log('Email:', emailInfo.email);

// 2. Get messages (the token is bound to EmailInfo internally; do not pass it yourself)
const result = await getEmails(emailInfo);
console.log(`Received ${result.emails.length} messages`, result.success);

// Probe a single channel only, without falling back to others:
const only = await generateEmail({ channel: 'smail-pw', channelFallback: false });
```

### Go — using Client

```go
package main

import (
    "fmt"
    tempemail "github.com/XxxXTeam/tempmail-sdk/sdk/go"
)

func main() {
    client := tempemail.NewClient()

    // 1. Get a temporary mailbox
    emailInfo, err := client.Generate(&tempemail.GenerateEmailOptions{
        Channel: tempemail.ChannelTempmail,
    })
    if err != nil {
        panic(err)
    }
    fmt.Println("Email:", emailInfo.Email)

    // 2. Get messages
    result, err := client.GetEmails()
    if err != nil {
        panic(err)
    }
    for _, email := range result.Emails {
        fmt.Printf("From: %s\n", email.From)
        fmt.Printf("Subject: %s\n", email.Subject)
        fmt.Printf("Body: %s\n", email.Text)
        fmt.Printf("Date: %s\n", email.Date)
    }
}
```

## 📐 Unified Email Format

No matter which channel you use, the returned email data structure is identical:

```typescript
interface Email {
  id: string;            // Unique message ID
  from: string;          // Sender address
  to: string;            // Recipient address
  subject: string;       // Subject
  text: string;          // Plain-text body
  html: string;          // HTML body
  date: string;          // ISO 8601 date
  isRead: boolean;       // Read flag
  attachments: {         // Attachments
    filename: string;
    size?: number;
    contentType?: string;
    url?: string;
  }[];
}
```

> **Note:** Some upstream APIs only expose a list or a summary (e.g. `maildrop`'s `description`), in which case `text` may only be a preview fragment. Each SDK's `normalize` module maps common alias fields into the structure above (e.g. ta-easy's `mail_sender`→`from`, `mail_title`→`subject`, `mail_body_text` / `mail_body_html`→`text` / `html`, numeric `received_at`→ISO date). If detail returns only `html` or only `text`, the SDK back-fills the missing field: `html` is stripped to produce `text`, and plain text is escaped and wrapped into basic HTML.

## 🔧 Environment Variables

| Variable | Description |
|----------|-------------|
| `TEMPMAIL_PROXY` | HTTP / SOCKS5 proxy URL |
| `TEMPMAIL_TIMEOUT` | Timeout in seconds |
| `TEMPMAIL_INSECURE` | `1` / `true` skips TLS verification |
| `TEMPMAIL_TELEMETRY_ENABLED` | `false` / `0` / `no` disables telemetry |
| `TEMPMAIL_TELEMETRY_URL` | Custom telemetry endpoint |
| `DROPMAIL_AUTH_TOKEN` | DropMail GraphQL `af_` token |
| `APIHZ_ID` | apihz channel credential id (defaults to the public account `88888888`) |
| `APIHZ_KEY` | apihz channel credential key (defaults to the public account `88888888`) |

### Anonymous Telemetry (can be disabled)

All SDKs enable anonymous usage statistics **by default**: events are queued in-process and periodically (or once a batch is full) merged into a single **`POST`** with a JSON body of `schema_version: 2` containing `sdk_language`, `sdk_version`, `os`, `arch` and `events[]` (`operation`, `channel`, `success`, `attempt_count`, `channels_tried`, redacted `error`, `ts_ms`, etc.). It contains **no** email content or tokens; anything that looks like an email address in error text is replaced with `[redacted]`.

Telemetry can also be disabled or repointed in code via global config: **Go** `SDKConfig.TelemetryEnabled` / `TelemetryEndpoint`; **npm** `telemetryEnabled` / `telemetryUrl`; **Rust** `telemetry_enabled` / `telemetry_url`; **Python** `SDKConfig.telemetry_enabled` / `telemetry_url`; **C** `tm_config_t.telemetry_enabled` / `telemetry_url` (and the equivalents on the other SDKs).

The optional **`server/`** in this repo is a sample collector (`POST /v1/event` writes to SQLite, etc.); **SDK reporting requires no token** and is unrelated to the collector's admin authentication.

## 📖 API Documentation

See each SDK directory for detailed docs:

| SDK | Docs | Registry |
|-----|------|----------|
| Go | [sdk/go/README.md](./sdk/go/README.md) | [pkg.go.dev](https://pkg.go.dev/github.com/XxxXTeam/tempmail-sdk/sdk/go) |
| npm | [sdk/npm/README.md](./sdk/npm/README.md) | [npmjs.org](https://www.npmjs.com/package/tempmail-sdk) |
| Rust | [sdk/rust/README.md](./sdk/rust/README.md) | [crates.io](https://crates.io/crates/tempmail-sdk) |
| Python | [sdk/python/README.md](./sdk/python/README.md) | [PyPI](https://pypi.org/project/tempemail-sdk/) |
| C | [sdk/c/README.md](./sdk/c/README.md) | [GitHub Releases](https://github.com/XxxXTeam/tempmail-sdk/releases) |
| Java | [sdk/java/README.md](./sdk/java/README.md) | [Maven Central](https://central.sonatype.com/artifact/io.github.xxxxteam/tempmail-sdk) |
| C# | [sdk/csharp/README.md](./sdk/csharp/README.md) | [NuGet](https://www.nuget.org/packages/XxxXTeam.TempMail/) |
| Ruby | [sdk/ruby/README.md](./sdk/ruby/README.md) | [RubyGems](https://rubygems.org/gems/tempmail_sdk) |
| PHP | [sdk/php/README.md](./sdk/php/README.md) | GitHub Releases (zip) |
| Kotlin | [sdk/kotlin/README.md](./sdk/kotlin/README.md) | Maven (Gradle) |

## 🔧 Project Structure

```
tempmail-sdk/
├── sdk/
│   ├── go/          # Go SDK
│   ├── npm/         # npm SDK (TypeScript)
│   ├── rust/        # Rust SDK
│   ├── python/      # Python SDK
│   ├── c/           # C SDK
│   ├── java/        # Java SDK (Maven)
│   ├── csharp/      # C# SDK (.NET)
│   ├── ruby/        # Ruby SDK (gem)
│   ├── php/         # PHP SDK (Composer)
│   └── kotlin/      # Kotlin SDK (Gradle)
├── server/          # Optional telemetry collector (Gin + SQLite)
├── webui/           # Web UI
├── scripts/         # Build / maintenance scripts
├── .github/
│   └── workflows/
│       ├── ci.yml       # CI workflow
│       └── release.yml  # Release workflow
├── .baseline_channels.txt  # Canonical channel list (source of truth for all SDKs)
├── LICENSE
└── README.md
```

Each SDK follows the same layered structure: a **Client** (state management), stateless **entry functions** (`GenerateEmail` / `GetEmails`) with channel fallback, one file per **provider**, a **normalize** module, a **config** module, a **retry** module and a **telemetry** module.

## 🚢 Release

Pushing a tag triggers an automated release:

```bash
git tag v1.1.0
git push origin v1.1.0
```

This automatically:

1. Verifies the build of all ten SDKs
2. Publishes the npm package to npmjs.org and GitHub Packages
3. Publishes the Rust crate to crates.io
4. Publishes the Python wheel to PyPI
5. Publishes the C# package to NuGet
6. Publishes the Java artifact to Maven Central
7. Publishes the Ruby gem to RubyGems
8. Packages the PHP SDK as a zip attached to the GitHub Release (not published to Packagist)
9. Publishes the Kotlin artifact
10. Builds multi-platform Go / Rust / C binaries
11. Creates a GitHub Release (with all build artifacts and a changelog)

### Configuring Secrets

Add the following under GitHub repo Settings → Secrets and variables → Actions:

| Secret | Description |
|--------|-------------|
| `NPM_TOKEN` | npm access token |
| `CARGO_REGISTRY_TOKEN` | crates.io API token |
| `PYPI_TOKEN` | PyPI API token |
| `NUGET_API_KEY` | NuGet API key |
| `RUBYGEMS_API_KEY` | RubyGems API key |

> `GITHUB_TOKEN` is provided automatically by GitHub Actions and needs no manual configuration.

## ⭐ Star History

<a href="https://star-history.com/#XxxXTeam/tempmail-sdk&Date">
 <picture>
   <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=XxxXTeam/tempmail-sdk&type=Date&theme=dark" />
   <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=XxxXTeam/tempmail-sdk&type=Date" />
   <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=XxxXTeam/tempmail-sdk&type=Date" />
 </picture>
</a>

## 🤝 Contributing

Issues and Pull Requests are welcome! Please read the [Contributing Guide](CONTRIBUTING.md) for details.

### Getting started

1. Fork this repository
2. Create a feature branch: `git checkout -b feat/your-feature`
3. Commit and push your changes
4. Open a Pull Request

### Adding a new channel provider

1. Add a provider file under each SDK's `providers/` directory
2. Implement the two core functions `generateEmail()` and `getEmails()`
3. Register the new channel in each SDK's Channel type/enum
4. Normalize returned data with `normalizeEmail()`
5. Use the shared global HTTP client for all requests (proxy/TLS aware)
6. Keep the channel identifier and count in sync across all ten SDKs (aligned with `.baseline_channels.txt`)
7. Update the README

See [CONTRIBUTING.md](CONTRIBUTING.md) for the full guide and code examples.

## 📄 License

This project is open source under the [GPL-3.0](LICENSE) license.

```
Copyright (C) 2026 XxxXTeam

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
```
