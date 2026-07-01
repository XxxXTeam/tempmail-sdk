# 临时邮箱 SDK

[![CI](https://github.com/XxxXTeam/tempmail-sdk/actions/workflows/ci.yml/badge.svg)](https://github.com/XxxXTeam/tempmail-sdk/actions/workflows/ci.yml)
[![npm version](https://badge.fury.io/js/tempmail-sdk.svg)](https://www.npmjs.com/package/tempmail-sdk)
[![Go Reference](https://pkg.go.dev/badge/github.com/XxxXTeam/tempmail-sdk/sdk/go.svg)](https://pkg.go.dev/github.com/XxxXTeam/tempmail-sdk/sdk/go)
[![PyPI version](https://badge.fury.io/py/tempemail-sdk.svg)](https://pypi.org/project/tempemail-sdk/)
[![crates.io](https://img.shields.io/crates/v/tempmail-sdk.svg)](https://crates.io/crates/tempmail-sdk)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

五端 SDK（**Go、npm、Rust、Python、C**）共同公开 **182** 个 `channel` 标识，按独立服务商合并为 **66** 个 provider。固定域名、裸域、镜像域名和同一 API 的多域名只算同一个独立服务商。下表为五端共同顺序（与 Go `allChannels` 对齐）。C 语言中 `tm_list_channels()` 返回顺序亦与下表一致；`tm_channel_t` 的**枚举数值顺序**仍为历史兼容布局，与列表顺序不同，见 `sdk/c/README.md`。所有渠道返回**统一标准化格式**，无需关心各服务商的接口差异。

## ✨ 特性

- 🌐 **五端共同公开 176 个 `channel` 标识，合并为 67 个独立服务商**；C 的 `tm_channel_t` 枚举下标与 `tm_list_channels` 顺序不同（见 `sdk/c/README.md`）
- 📐 **统一标准化返回格式** — 所有渠道的邮件数据结构完全一致
- 📦 提供 Go、npm、Rust、Python、C 五种 SDK
- 🔄 支持邮箱生成和邮件轮询
- 📝 完整的类型定义（TypeScript / Rust / Go）
- 🚀 简单易用的 API，开箱即用
- 🔌 Token/Session 自动管理（使用 Client 类）
- 🔁 创建邮箱 / 拉取邮件支持可配置 **HTTP 重试**（退避与最大次数等，各语言字段见子目录 README）
- 📊 **匿名遥测**默认开启：批量上报操作成败与重试元数据（不含邮件内容，错误串中的邮箱会脱敏），可用环境变量或全局配置关闭或改端点
- 🏗️ 多平台预编译二进制（Go / Rust / C）
- 📡 所有包均可通过 GitHub 托管安装，无需第三方注册表

## 📋 支持的渠道

下列顺序为 Go、Rust、Python、C 与 npm 的公共列表顺序。只要求五端公共渠道集合、数量与 `listChannels` / `ListChannels` / `list_channels` / `tm_list_channels()` 返回顺序一致；随机生成邮箱时，各 SDK 会在本端独立打乱尝试顺序，不需要、也不承诺跨 SDK 随机顺序一致。渠道统计以独立服务商为准，同一 provider 的固定域名、裸域、镜像域名和同 API 多域名不重复计入服务商数量。若上游按设备标识限制连续生成，且真实验证确认同一设备连续生成后返回 429，对应 provider 可以在每次生成邮箱时创建新的设备 ID，并把该邮箱对应的设备 ID 存入内部 Token/Session，后续收件继续使用同一个设备 ID。

| 渠道 | 服务商 | 认证方式 | 说明 |
|------|--------|----------|------|
| `tempmail` | [tempmail.ing](https://tempmail.ing) | 邮箱地址 | 支持自定义有效期 |
| `tempmail-cn` | [tempmail.cn](https://tempmail.cn) | 邮箱地址 | Socket.IO 事件协议：`request shortid` / `set shortid` / `mail`；`domain` 可指定 `tempmail.cn` 或已解析到该服务的自定义域名 |
| `ta-easy` | [ta-easy.com](https://www.ta-easy.com) | Token（会话 UUID） | `POST https://api-endpoint.ta-easy.com/temp-email/address/new` 建址；`POST .../temp-email/inbox/list` 拉信；`expiresAt` 为毫秒时间戳；上游字段如 `mail_sender` / `mail_title` / `mail_body_*` 由 SDK 归一化到统一 `Email` |
| `10minute-one` | [10minutemail.one](https://10minutemail.one) | Token | 站点 SSR / JWT + Web API 建邮与收信；`domain` 可选接入参数（见各 SDK 说明） |
| `xghff-com` | [xghff.com](https://xghff.com) | Token | 10minutemail.one API 固定域名 `xghff.com`；生成和收信接口与 `10minute-one` 相同 |
| `oqqaj-com` | [oqqaj.com](https://oqqaj.com) | Token | 10minutemail.one API 固定域名 `oqqaj.com`；生成和收信接口与 `10minute-one` 相同 |
| `psovv-com` | [psovv.com](https://psovv.com) | Token | 10minutemail.one API 固定域名 `psovv.com`；生成和收信接口与 `10minute-one` 相同 |
| `dbwot-com` | [dbwot.com](https://dbwot.com) | Token | 10minutemail.one API 固定域名 `dbwot.com`；生成和收信接口与 `10minute-one` 相同 |
| `ygwpr-com` | [ygwpr.com](https://ygwpr.com) | Token | 10minutemail.one API 固定域名 `ygwpr.com`；生成和收信接口与 `10minute-one` 相同 |
| `imxwe-com` | [imxwe.com](https://imxwe.com) | Token | 10minutemail.one API 固定域名 `imxwe.com`；生成和收信接口与 `10minute-one` 相同 |
| `linshiyou` | [linshiyou.com](https://linshiyou.com) | Token（`NEXUS_TOKEN`） | 创建邮箱时 Set-Cookie 下发 `NEXUS_TOKEN`；收信需携带该 Token 与 `tmail-emails` 等 Cookie；列表与正文由 HTML 分段 / iframe 解析 |
| `mffac` | [mffac.com](https://www.mffac.com) | Token（mailbox `id`） | REST：`POST /api/mailboxes` 创建，`GET /api/mailboxes/{local}/emails` 列表，`GET /api/emails/{id}` 详情；详情 `textContent` / `htmlContent` 映射为统一正文；默认 24h |
| `tempmail-lol` | [tempmail.lol](https://tempmail.lol) | Token | 支持指定域名 |
| `chatgpt-org-uk` | [mail.chatgpt.org.uk](https://mail.chatgpt.org.uk) | Inbox Token | 官网在 HTML 注入 `__BROWSER_AUTH`；npm 已随首页一并解析并用于创建邮箱 |
| `temp-mail-io` | [temp-mail.io](https://temp-mail.io) | Token | REST：动态读取 `mobileTestingHeader` 后调用 `api.internal.temp-mail.io/api/v3`；生成接口返回的 token 由 SDK 内部维护 |
| `mail-cx` | [mail.cx](https://mail.cx) | Client ID | 匿名 Web API：`GET /v1/config` 获取系统域名，隐式邮箱地址 `{local}@{domain}`；`GET /v1/inbox/{email}` 长轮询收信，详情通过 `/v1/email/{id}` 拉取 |
| `ddker-com` | [ddker.com](https://ddker.com) | Client ID | mail.cx API 固定域名 `ddker.com`；生成和收信接口与 `mail-cx` 相同 |
| `catchmail` | [catchmail.io](https://catchmail.io) | - | 公开 REST：`GET api.catchmail.io/api/v1/mailbox?address=` 列表，`GET /message/{id}?mailbox=` 详情；详情 `body.text` / `body.html` 映射为统一 `text` / `html` |
| `catchmail-mailistry` | [mailistry.com](https://mailistry.com) | - | Catchmail API 固定域名 `mailistry.com`；收信接口与 `catchmail` 相同 |
| `catchmail-zeppost` | [zeppost.com](https://zeppost.com) | - | Catchmail API 固定域名 `zeppost.com`；收信接口与 `catchmail` 相同 |
| `mailforspam` | [mailforspam.com](https://mailforspam.com) | - | 公开 REST：`GET api.mailforspam.com/api/mailboxes/{email}/emails` 列表，`GET /api/mailboxes/{email}/emails/{id}` 详情；详情 `body_text` / `body_html` 映射为统一 `text` / `html` |
| `mailforspam-tempmail-io` | [tempmail.io](https://tempmail.io) | - | MailForSpam API 固定域名 `tempmail.io`；收信接口与 `mailforspam` 相同 |
| `mailforspam-disposable` | [disposable.email](https://disposable.email) | - | MailForSpam API 固定域名 `disposable.email`；收信接口与 `mailforspam` 相同 |
| `tempmailo` | [tempmailo.com](https://tempmailo.com) | Token（`__RequestVerificationToken` + Cookie） | Web API：`GET /changemail` 建址，`POST /` 传 `mail` 拉信；列表对象直接含 `text` / `html` 正文 |
| `tempmailc` | [tempmailc.com](https://tempmailc.com) | - | Public API：`GET /api/v1/new` 建址，`GET /api/v1/inbox` 拉列表，`GET /api/v1/message` 读取 `text` / `html` 正文 |
| `mailnesia` | [mailnesia.com](https://mailnesia.com) | - | 任意 `{local}@mailnesia.com` 建址；`GET /mailbox/{local}` 解析 `tr.emailheader` 列表，`GET /mailbox/{local}/{id}` 读取 `text_plain_{id}` / `text_html_{id}` 正文 |
| `throwawaymail` | [throwawaymail.app](https://throwawaymail.app) | Token | Web API 建址并轮询收信；Token 由 SDK 内部维护 |
| `shitty-email` | [shitty.email](https://shitty.email) | Token | `POST /api/inbox` 建址；`X-Session-Token` + `GET /api/inbox` 拉列表，`GET /api/email/{id}` 读取 `text` / `html` 正文 |
| `tempmailpro` | [tempmailpro.us](https://tempmailpro.us) | Token | `POST /api/v1/mailbox/create` 建箱；`GET /api/v1/mailbox/{token}/emails` 拉列表，详情字段 `body_text` / `body_html` 映射统一正文 |
| `devmail-uk` | [devmail.uk](https://devmail.uk) | 邮箱地址 | `GET /api/new` 建址；`GET /api/inbox/{mailbox}?detail=true` 拉列表；生成接口返回的 `email` / `mailbox` 字段均兼容解析 |
| `cleantempmail` | [cleantempmail.com](https://cleantempmail.com) | 邮箱地址 | `GET /api/generate-email` 建址；`GET /api/emails?email=` 拉列表；公开 API 通过 `X-API-Key` 头访问 |
| `inboxkitten` | [inboxkitten.com](https://inboxkitten.com) | - | 公开 API 拉取收件箱列表与详情 |
| `getnada` | [getnada.net](https://getnada.net) | Token | `POST /api/inbox/open` 建箱；`GET /api/inbox/messages` 列表；`GET /api/inbox/message` 详情含 `text_plain` / `html_sanitized` |
| `1vpn-net` | [1vpn.net](https://1vpn.net) | Token | GetNada 固定域名 `1vpn.net`；收信接口与 `getnada` 相同 |
| `abematv-com` | [abematv.com](https://abematv.com) | Token | GetNada 固定域名 `abematv.com`；收信接口与 `getnada` 相同 |
| `abematv-net` | [abematv.net](https://abematv.net) | Token | GetNada 固定域名 `abematv.net`；收信接口与 `getnada` 相同 |
| `abematv-org` | [abematv.org](https://abematv.org) | Token | GetNada 固定域名 `abematv.org`；收信接口与 `getnada` 相同 |
| `aceh-cc` | [aceh.cc](https://aceh.cc) | Token | GetNada 固定域名 `aceh.cc`；收信接口与 `getnada` 相同 |
| `bangkabelitung-net` | [bangkabelitung.net](https://bangkabelitung.net) | Token | GetNada 固定域名 `bangkabelitung.net`；收信接口与 `getnada` 相同 |
| `cctruyen-com` | [cctruyen.com](https://cctruyen.com) | Token | GetNada 固定域名 `cctruyen.com`；收信接口与 `getnada` 相同 |
| `getnada-com` | [getnada.com](https://getnada.com) | Token | GetNada 固定域名 `getnada.com`；收信接口与 `getnada` 相同 |
| `getnada-email` | [getnada.email](https://getnada.email) | Token | GetNada 固定域名 `getnada.email`；收信接口与 `getnada` 相同 |
| `getnada-net` | [getnada.net](https://getnada.net) | Token | GetNada 固定域名 `getnada.net`；收信接口与 `getnada` 相同 |
| `jawatengah-net` | [jawatengah.net](https://jawatengah.net) | Token | GetNada 固定域名 `jawatengah.net`；收信接口与 `getnada` 相同 |
| `jawatimur-net` | [jawatimur.net](https://jawatimur.net) | Token | GetNada 固定域名 `jawatimur.net`；收信接口与 `getnada` 相同 |
| `kalimantanbarat-net` | [kalimantanbarat.net](https://kalimantanbarat.net) | Token | GetNada 固定域名 `kalimantanbarat.net`；收信接口与 `getnada` 相同 |
| `kalimantanselatan-net` | [kalimantanselatan.net](https://kalimantanselatan.net) | Token | GetNada 固定域名 `kalimantanselatan.net`；收信接口与 `getnada` 相同 |
| `kalimantantengah-net` | [kalimantantengah.net](https://kalimantantengah.net) | Token | GetNada 固定域名 `kalimantantengah.net`；收信接口与 `getnada` 相同 |
| `kalimantantimur-net` | [kalimantantimur.net](https://kalimantantimur.net) | Token | GetNada 固定域名 `kalimantantimur.net`；收信接口与 `getnada` 相同 |
| `kalimantanutara-net` | [kalimantanutara.net](https://kalimantanutara.net) | Token | GetNada 固定域名 `kalimantanutara.net`；收信接口与 `getnada` 相同 |
| `kepulauanriau-net` | [kepulauanriau.net](https://kepulauanriau.net) | Token | GetNada 固定域名 `kepulauanriau.net`；收信接口与 `getnada` 相同 |
| `luxury345-com` | [luxury345.com](https://luxury345.com) | Token | GetNada 固定域名 `luxury345.com`；收信接口与 `getnada` 相同 |
| `malukuutara-net` | [malukuutara.net](https://malukuutara.net) | Token | GetNada 固定域名 `malukuutara.net`；收信接口与 `getnada` 相同 |
| `nusatenggarabarat-net` | [nusatenggarabarat.net](https://nusatenggarabarat.net) | Token | GetNada 固定域名 `nusatenggarabarat.net`；收信接口与 `getnada` 相同 |
| `nusatenggaratimur-net` | [nusatenggaratimur.net](https://nusatenggaratimur.net) | Token | GetNada 固定域名 `nusatenggaratimur.net`；收信接口与 `getnada` 相同 |
| `papuabarat-net` | [papuabarat.net](https://papuabarat.net) | Token | GetNada 固定域名 `papuabarat.net`；收信接口与 `getnada` 相同 |
| `papuabaratdaya-net` | [papuabaratdaya.net](https://papuabaratdaya.net) | Token | GetNada 固定域名 `papuabaratdaya.net`；收信接口与 `getnada` 相同 |
| `papuaselatan-net` | [papuaselatan.net](https://papuaselatan.net) | Token | GetNada 固定域名 `papuaselatan.net`；收信接口与 `getnada` 相同 |
| `pehol-com` | [pehol.com](https://pehol.com) | Token | GetNada 固定域名 `pehol.com`；收信接口与 `getnada` 相同 |
| `ptruyen-com` | [ptruyen.com](https://ptruyen.com) | Token | GetNada 固定域名 `ptruyen.com`；收信接口与 `getnada` 相同 |
| `pulaubali-net` | [pulaubali.net](https://pulaubali.net) | Token | GetNada 固定域名 `pulaubali.net`；收信接口与 `getnada` 相同 |
| `riau-net` | [riau.net](https://riau.net) | Token | GetNada 固定域名 `riau.net`；收信接口与 `getnada` 相同 |
| `seokey-org` | [seokey.org](https://seokey.org) | Token | GetNada 固定域名 `seokey.org`；收信接口与 `getnada` 相同 |
| `sulawesibarat-net` | [sulawesibarat.net](https://sulawesibarat.net) | Token | GetNada 固定域名 `sulawesibarat.net`；收信接口与 `getnada` 相同 |
| `sulawesiselatan-net` | [sulawesiselatan.net](https://sulawesiselatan.net) | Token | GetNada 固定域名 `sulawesiselatan.net`；收信接口与 `getnada` 相同 |
| `sulawesitengah-net` | [sulawesitengah.net](https://sulawesitengah.net) | Token | GetNada 固定域名 `sulawesitengah.net`；收信接口与 `getnada` 相同 |
| `sulawesitenggara-net` | [sulawesitenggara.net](https://sulawesitenggara.net) | Token | GetNada 固定域名 `sulawesitenggara.net`；收信接口与 `getnada` 相同 |
| `sumaterabarat-net` | [sumaterabarat.net](https://sumaterabarat.net) | Token | GetNada 固定域名 `sumaterabarat.net`；收信接口与 `getnada` 相同 |
| `sumateraselatan-net` | [sumateraselatan.net](https://sumateraselatan.net) | Token | GetNada 固定域名 `sumateraselatan.net`；收信接口与 `getnada` 相同 |
| `sumaterautara-net` | [sumaterautara.net](https://sumaterautara.net) | Token | GetNada 固定域名 `sumaterautara.net`；收信接口与 `getnada` 相同 |
| `villatogel-com` | [villatogel.com](https://villatogel.com) | Token | GetNada 固定域名 `villatogel.com`；收信接口与 `getnada` 相同 |
| `mail123` | [mail123.fr](https://mail123.fr) | - | `GET /api/v1/mailbox/new` 建址；`GET /api/v1/mailbox/{address}/messages?limit=50` 列表；详情含 `text` / `html` |
| `mail10s` | [mail10s.com](https://mail10s.com) | - | 任意随机地址；`GET /api/emails/{email}/inbox` 公开 inbox API；`body_text` / `body_html` 映射统一正文 |
| `webmailtemp` | [webmailtemp.com](https://webmailtemp.com) | Token（会话 Cookie + username） | `GET /api/create` 建箱；`GET /api/check/{username}` 收信；30 分钟 TTL |
| `tempfastmail` | [tempfastmail.com](https://tempfastmail.com) | Token（邮箱 UUID） | `POST /api/email-box` 建箱；`GET /api/email-box/{uuid}/emails` 列表；详情 `html` 由 normalize 派生 `text` |
| `1sec-mail` | [tmaily.com](https://tmaily.com) | Token | `GET /generate` 取地址；从 `Set-Cookie` 提取 `TMaily_sid`；`GET /emails?address=` 拉列表，详情由 `body_text` / `html` 映射，缺失正文由 normalize 反向生成 |
| `fakemail` | [fakemail.net](https://fakemail.net) | Token | `/index/index` 建址，`/index/refresh` 拉列表，`/index/email` 详情；`telo` HTML 正文映射为统一 `html` |
| `openinbox` | [openinbox.io](https://openinbox.io) | Token | `POST /api/inbox` 建箱；`GET /emails/inbox/{id}` 列表；`GET /emails/{emailId}` 详情含 `textBody` / `htmlBody` |
| `inboxes` | [inboxes.com](https://inboxes.com) | - | 公开 v2：`GET /api/v2/domain` 域名，`GET /api/v2/inbox/{email}` 列表，`GET /api/v2/message/{uid}` 详情含 `text` / `html` |
| `uncorreotemporal` | [uncorreotemporal.com](https://uncorreotemporal.com) | Token（`X-Session-Token`） | `POST /api/v1/mailboxes` 建箱；`GET /api/v1/mailboxes/{address}/messages` 列表；详情含 `body_text` / `body_html` |
| `awamail` | [awamail.com](https://awamail.com) | Session Cookie | 自动提取 Cookie |
| `mail-tm` | [mail.tm](https://mail.tm) / [api.mail.tm](https://api.mail.tm) | Bearer Token | REST API，自动注册账号；npm 实现与 **Internxt** 等前端一致（如 `GET /domains?page=1`、常见浏览器请求头） |
| `web-library-net` | [web-library.net](https://web-library.net) | Bearer Token | mail.tm API 固定域名 `web-library.net`；账号、登录、收信接口与 `mail-tm` 相同 |
| `dropmail` | [dropmail.me](https://dropmail.me) | Session ID | GraphQL API |
| `guerrillamail` | [guerrillamail.com](https://guerrillamail.com) | Session | 公开 JSON API |
| `guerrillamail-com` | [guerrillamail.com](https://guerrillamail.com) | Session | GuerrillaMail 裸域 JSON API 入口 |
| `maildrop` | [maildrop.cx](https://maildrop.cx) | Token（完整邮箱） | REST：`GET /api/suffixes.php` 获取后缀，**排除** `transformer.edu.kg`，再随机生成本地部分；可选 `domain` 指定后缀（须仍在列表中）；`GET /api/emails.php?addr=` 拉列表；列表字段 `description` 映射为统一结构中的 `text`，无单封 MIME/HTML 全文 |
| `smail-pw` | [smail.pw](https://smail.pw) | `__session` Cookie | React Router `_root.data`（RSC/Flight）；列表侧为元数据，**npm / Python** 已解析 D1 行对象与引用下标 |
| `vip-215` | [vip.215.im](https://vip.215.im) | WebSocket Token | `POST` 建箱 + `wss` 收 `message.new`；推送无正文时各 SDK 使用 **synthetic-v1** 统一生成 `text` / `html`（C 收信依赖 libcurl WebSocket，版本过低会降级） |
| `fake-legal` | [fake.legal](https://fake.legal) | - | `GET /api/domains` + `GET /api/inbox/new?domain=` 建址；`GET /api/inbox/{encodeURIComponent(邮箱)}` 拉信；可选 `domain` |
| `imgui-de` | [imgui.de](https://imgui.de) | - | fake.legal API 固定域名 `imgui.de`；收信接口与 `fake-legal` 相同 |
| `pulsewebmenu-de` | [pulsewebmenu.de](https://pulsewebmenu.de) | - | fake.legal API 固定域名 `pulsewebmenu.de`；收信接口与 `fake-legal` 相同 |
| `moakt` | [moakt.com](https://www.moakt.com) | Token（`mok1:` + Base64 JSON：`locale` + 合并 Cookie，须含 `tm_session`） | HTML：`GET /{locale}` → `POST /{locale}/inbox` 创建邮箱 → `GET /{locale}/inbox` 解析 `#email-address`；收信解析 `href` 中 `/email/{uuid}` 并逐封 `GET .../html`；`domain` 可选语言路径（如 `zh`）；各 SDK 以独立会话或显式 `Cookie` 头维护，避免与全局 Cookie 混用 |
| `drmail-in` | [drmail.in](https://drmail.in) | Token（`mok1:` + Cookie） | Moakt 固定域名 `drmail.in`；创建和收信接口与 `moakt` 相同 |
| `teml-net` | [teml.net](https://teml.net) | Token（`mok1:` + Cookie） | Moakt 固定域名 `teml.net`；创建和收信接口与 `moakt` 相同 |
| `tmpeml-com` | [tmpeml.com](https://tmpeml.com) | Token（`mok1:` + Cookie） | Moakt 固定域名 `tmpeml.com`；创建和收信接口与 `moakt` 相同 |
| `tmpbox-net` | [tmpbox.net](https://tmpbox.net) | Token（`mok1:` + Cookie） | Moakt 固定域名 `tmpbox.net`；创建和收信接口与 `moakt` 相同 |
| `moakt-cc` | [moakt.cc](https://moakt.cc) | Token（`mok1:` + Cookie） | Moakt 固定域名 `moakt.cc`；创建和收信接口与 `moakt` 相同 |
| `disbox-net` | [disbox.net](https://disbox.net) | Token（`mok1:` + Cookie） | Moakt 固定域名 `disbox.net`；创建和收信接口与 `moakt` 相同 |
| `tmpmail-org` | [tmpmail.org](https://tmpmail.org) | Token（`mok1:` + Cookie） | Moakt 固定域名 `tmpmail.org`；创建和收信接口与 `moakt` 相同 |
| `tmpmail-net` | [tmpmail.net](https://tmpmail.net) | Token（`mok1:` + Cookie） | Moakt 固定域名 `tmpmail.net`；创建和收信接口与 `moakt` 相同 |
| `tmails-net` | [tmails.net](https://tmails.net) | Token（`mok1:` + Cookie） | Moakt 固定域名 `tmails.net`；创建和收信接口与 `moakt` 相同 |
| `disbox-org` | [disbox.org](https://disbox.org) | Token（`mok1:` + Cookie） | Moakt 固定域名 `disbox.org`；创建和收信接口与 `moakt` 相同 |
| `moakt-co` | [moakt.co](https://moakt.co) | Token（`mok1:` + Cookie） | Moakt 固定域名 `moakt.co`；创建和收信接口与 `moakt` 相同 |
| `moakt-ws` | [moakt.ws](https://moakt.ws) | Token（`mok1:` + Cookie） | Moakt 固定域名 `moakt.ws`；创建和收信接口与 `moakt` 相同 |
| `tmail-ws` | [tmail.ws](https://tmail.ws) | Token（`mok1:` + Cookie） | Moakt 固定域名 `tmail.ws`；创建和收信接口与 `moakt` 相同 |
| `bareed-ws` | [bareed.ws](https://bareed.ws) | Token（`mok1:` + Cookie） | Moakt 固定域名 `bareed.ws`；创建和收信接口与 `moakt` 相同 |
| `email10min` | [email10min.com](https://email10min.com) | Cookie + CSRF | `GET /zh` 取 CSRF，`POST /messages` 取邮箱/邮件；站点域名偶有跳转，稳定性一般 |
| `mjj-cm` | [mjj.cm](https://mjj.cm) | Session | Socket.IO：`request shortid` / `set shortid` / `mail` |
| `linshi-co` | [linshi.co](https://linshi.co) | Session | Socket.IO 克隆站，协议同 `mjj-cm` |
| `harakirimail` | [harakirimail.com](https://harakirimail.com) | - | 公开 REST：`GET /api/v1/inbox/{name}`，逐封 `GET /api/v1/email/{id}` 获取正文 |
| `jqkjqk-xyz` | [jqkjqk.xyz](https://jqkjqk.xyz) | Token（mail.zhujump.com 会话） | mail.zhujump.com 固定域名 `jqkjqk.xyz`；收信接口与 zhujump provider 相同 |
| `lyhlevi-com` | [lyhlevi.com](https://lyhlevi.com) | Token（MoeMail 会话） | lyhlevi.com MoeMail 部署实例；账号注册登录后创建邮箱，列表和详情接口映射统一正文 |
| `tempmail-plus` | [tempmail.plus](https://tempmail.plus) | - | 公开 REST：`GET /api/mails/?email=` 列表，`GET /api/mails/{id}?email=` 详情；`mailto.plus` 域名 |
| `fexpost-com` | [fexpost.com](https://fexpost.com) | - | TempMail Plus 固定域名 `fexpost.com`；列表和详情接口与 `tempmail-plus` 相同 |
| `fexbox-org` | [fexbox.org](https://fexbox.org) | - | TempMail Plus 固定域名 `fexbox.org`；列表和详情接口与 `tempmail-plus` 相同 |
| `mailbox-in-ua` | [mailbox.in.ua](https://mailbox.in.ua) | - | TempMail Plus 固定域名 `mailbox.in.ua`；列表和详情接口与 `tempmail-plus` 相同 |
| `rover-info` | [rover.info](https://rover.info) | - | TempMail Plus 固定域名 `rover.info`；列表和详情接口与 `tempmail-plus` 相同 |
| `chitthi-in` | [chitthi.in](https://chitthi.in) | - | TempMail Plus 固定域名 `chitthi.in`；列表和详情接口与 `tempmail-plus` 相同 |
| `fextemp-com` | [fextemp.com](https://fextemp.com) | - | TempMail Plus 固定域名 `fextemp.com`；列表和详情接口与 `tempmail-plus` 相同 |
| `any-pink` | [any.pink](https://any.pink) | - | TempMail Plus 固定域名 `any.pink`；列表和详情接口与 `tempmail-plus` 相同 |
| `merepost-com` | [merepost.com](https://merepost.com) | - | TempMail Plus 固定域名 `merepost.com`；列表和详情接口与 `tempmail-plus` 相同 |
| `tempmail-lol-v2` | [api.tempmail.lol](https://api.tempmail.lol) | Token | `GET /generate` 返回 address+token，`GET /auth/{token}` 拉取收件箱 |
| `tempgbox` | [tempgbox.net](https://tempgbox.net) | Device ID | `POST /api/proxy?route=generate` 使用 `variant=googlemail` 生成 Gmail/Googlemail alias；每次生成随机 `X-Device-ID`，并随请求带随机来源 IP 头 |
| `emailnator` | [emailnator.com](https://www.emailnator.com) | XSRF + Cookie | `POST /generate-email` 使用 Gmail/GoogleMail alias 选项生成地址；`POST /message-list` 拉取列表并用 `messageID` 读取 HTML 正文 |
| `temporam` | [temporam.com](https://temporam.com) | 邮箱地址 | 公开 REST：`GET /api/domains` 获取域名，`GET /api/emails?email=` 拉列表，`GET /api/emails/{id}` 读取正文 |
| `neighbours` | [neighbours.sh](https://neighbours.sh) | - | `GET /config/domains` 获取域名；`GET /inbox/{address}` / `GET /inbox/{address}/{uid}` 拉信；`404` 视为空收件箱 |
| `pleasenospam` | [pleasenospam.email](https://pleasenospam.email) | - | `GET /{email}.json` 拉列表；`from` 为数组，取 `from[0]` 作为发件人 |
| `sharklasers` | [sharklasers.com](https://www.sharklasers.com) | Session | GuerrillaMail 镜像，API 与 `guerrillamail` 相同 |
| `sharklasers-com` | [sharklasers.com](https://sharklasers.com) | Session | GuerrillaMail 裸域镜像，API 与 `guerrillamail` 相同 |
| `grr-la` | [grr.la](https://www.grr.la) | Session | GuerrillaMail 镜像，API 与 `guerrillamail` 相同 |
| `grr-la-com` | [grr.la](https://grr.la) | Session | GuerrillaMail 裸域镜像，API 与 `guerrillamail` 相同 |
| `guerrillamail-info` | [guerrillamail.info](https://www.guerrillamail.info) | Session | GuerrillaMail 镜像，API 与 `guerrillamail` 相同 |
| `spam4me` | [spam4.me](https://www.spam4.me) | Session | GuerrillaMail 镜像，API 与 `guerrillamail` 相同 |
| `guerrillamail-net` | [guerrillamail.net](https://www.guerrillamail.net) | Session | GuerrillaMail 镜像，API 与 `guerrillamail` 相同 |
| `guerrillamail-org` | [guerrillamail.org](https://www.guerrillamail.org) | Session | GuerrillaMail 镜像，API 与 `guerrillamail` 相同 |
| `guerrillamailblock` | [guerrillamailblock.com](https://www.guerrillamailblock.com) | Session | GuerrillaMail 镜像，API 与 `guerrillamail` 相同 |
| `guerrillamail-com-www` | [guerrillamail.com](https://www.guerrillamail.com) | Session | GuerrillaMail `www` JSON API 入口 |
| `m2u` | [MailToYou](https://m2u.io) | Token | `POST /v1/mailboxes/auto` 创建邮箱；`token` + `viewToken` 组成内部令牌；`GET /v1/mailboxes/{token}/messages?view={viewToken}` 列表，`GET /v1/mailboxes/{token}/messages/{id}?view={viewToken}` 详情 |
| `tempy-email` | [Tempy Email](https://tempy.email) | - | `POST /api/v1/mailbox` 创建邮箱（请求体可为空 JSON；可选 `domain`）；`GET /api/v1/mailbox/{email}/messages` 拉列表 |
| `fmail` | [fmail.men](https://fmail.men) | - | `GET /v1/random` 建址；`GET /v1/inbox/{local}?domain={domain}&limit=50` 拉列表；`GET /v1/email/{token}` 读取单封详情 |
| `ockito` | [ockito.com](https://ockito.com) | Token（access_token + refresh_token） | `POST /gtoken` 换取会话令牌；`GET /email` 建址；`GET /retrieve/{email}/emails` 拉列表，`GET /retrieve/{email}/{uid}` 详情 |
| `anonbox` | [anonbox.net](https://anonbox.net) | Token（`inbox/secret`） | `GET /en/` 解析页面建箱；`GET /{token}/` 拉信，token 形如 `inbox/secret`，mbox 明文解析 |
| `duckmail` | [duckmail.sbs](https://duckmail.sbs) | Bearer Token | `GET /domains?page=1` 取域名；`POST /accounts` 创建账号；`POST /token` 获取 Bearer Token；`GET /messages` 拉列表 |
| `mailinator` | [mailinator.com](https://mailinator.com) | - | 公开 REST：`GET /api/v2/domains/public/inboxes/{inbox}` 拉列表；`GET /api/v2/domains/public/messages/{id}/{text|texthtml|attachments}` 详情 |

> **提示：** 使用 Client 类时，Token/Session 由 SDK 自动管理，无需手动处理。C SDK 中 `tm_list_channels()` 的**返回顺序**与上表一致；若按 `tm_channel_t` **枚举常量**编程，其数值顺序与上表不同，以 `tempmail_sdk.h` 与 `sdk/c/README.md` 为准。

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

> **说明：** 个别渠道的上游接口只提供列表或摘要（例如 `maildrop` 的 `description`），此时 `text` 可能仅为预览片段。各语言 `normalize` 模块会将常见别名字段映射到上述结构（例如 ta-easy 的 `mail_sender`→`from`、`mail_title`→`subject`、`mail_body_text` / `mail_body_html`→`text` / `html`，数字型 `received_at`→ISO 日期）。如果上游详情只返回 `html` 或只返回 `text`，SDK 会从已有正文反向生成缺失字段：`html` 会去标签生成 `text`，纯文本会转义并包进基础 HTML。

## 📦 包获取渠道

每个 SDK 均有多种获取方式，下表汇总所有可用渠道：

| SDK | 渠道 | 安装方式 | 认证 |
|-----|------|---------|:----:|
| **Go** | GitHub (git tag) | `go get github.com/XxxXTeam/tempmail-sdk/sdk/go` | - |
| **npm** | [npmjs.org](https://www.npmjs.com/package/tempmail-sdk) | `npm install tempmail-sdk` | - |
| **npm** | [GitHub Packages](https://github.com/XxxXTeam/tempmail-sdk/pkgs/npm/tempmail-sdk) | `npm install @XxxXTeam/tempmail-sdk` | 🔑 |
| **Rust** | [crates.io](https://crates.io/crates/tempmail-sdk) | `tempmail-sdk = "1.1.0"` | - |
| **Rust** | GitHub (git) | `tempmail-sdk = { git = "...", subdirectory = "sdk/rust" }` | - |
| **Python** | [PyPI](https://pypi.org/project/tempemail-sdk/) | `pip install tempemail-sdk` | - |
| **Python** | [GitHub Release](https://github.com/XxxXTeam/tempmail-sdk/releases) | `pip install <wheel URL>` | - |
| **C** | [GitHub Release](https://github.com/XxxXTeam/tempmail-sdk/releases) | 下载预编译 zip 包 | - |
| **C** | 源码编译 | CMake 构建 | - |

> 🔑 = 需要认证。GitHub Packages (npm) 需要配置 GitHub PAT，详见下方说明。

## 📦 安装

### Go

```bash
go get github.com/XxxXTeam/tempmail-sdk/sdk/go
```

### npm / TypeScript

```bash
# 从 npmjs.org（推荐，无需认证）
npm install tempmail-sdk
```

<details>
<summary>从 GitHub Packages 安装（需认证）</summary>

GitHub Packages 的 npm 包即使是公开仓库也需要认证：

```bash
# 1. 创建 GitHub PAT: Settings → Developer settings → Personal access tokens → 勾选 read:packages
# 2. 配置 .npmrc
echo "@XxxXTeam:registry=https://npm.pkg.github.com" >> ~/.npmrc
echo "//npm.pkg.github.com/:_authToken=YOUR_GITHUB_TOKEN" >> ~/.npmrc
# 3. 安装
npm install @XxxXTeam/tempmail-sdk
```

</details>

### Rust

```toml
# 从 crates.io（推荐）
[dependencies]
tempmail-sdk = "1.1.0"

# 从 GitHub（始终获取最新代码）
[dependencies]
tempmail-sdk = { git = "https://github.com/XxxXTeam/tempmail-sdk", subdirectory = "sdk/rust" }
```

### Python

```bash
# 从 PyPI（推荐）
pip install tempemail-sdk

# 从 GitHub Release（wheel 直链）
pip install https://github.com/XxxXTeam/tempmail-sdk/releases/latest/download/tempemail_sdk-1.1.0-py3-none-any.whl
```

### C

从 [GitHub Releases](https://github.com/XxxXTeam/tempmail-sdk/releases) 下载预编译包：

| 包名 | 平台 |
|------|------|
| `c-sdk-linux-amd64.zip` | Linux x64 |
| `c-sdk-darwin-arm64.zip` | macOS ARM64 |
| `c-sdk-windows-amd64.zip` | Windows x64 |

或源码编译：

```bash
cd sdk/c
curl -L -o vendor/cJSON.h https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.h
curl -L -o vendor/cJSON.c https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.c
cmake -B build -S . && cmake --build build
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
if (!emailInfo) {
  // 未指定 channel 时可能多渠道路径全部失败
  throw new Error('创建失败');
}
console.log('邮箱:', emailInfo.email);

// 2. 获取邮件（Token 由 SDK 内部与 EmailInfo 绑定，勿自行传 token）
const result = await getEmails(emailInfo);
console.log(`收到 ${result.emails.length} 封邮件`, result.success);

// 仅探测某一渠道、失败时不 Fallback 到其他渠道：
const only = await generateEmail({ channel: 'smail-pw', channelFallback: false });
```

### npm — 仓库内示例脚本

在 `sdk/npm` 目录下（需先 `npm install`，部分 demo 另需 `nodemailer`）：

| 脚本 | 说明 |
|------|------|
| `demo/poll-emails.ts` | 交互或 **SMTP 自动探针**（设置 `SMTP_HOST` 等）；可用 `POLL_CHANNELS=smail-pw` 限定渠道 |

常用环境变量：`TEMPMAIL_PROXY`、`TEMPMAIL_TIMEOUT`、`TEMPMAIL_INSECURE`；DropMail 见各 SDK 文档中的 `DROPMAIL_*` 说明。

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

## 匿名遥测与 HTTP 重试

### 匿名遥测（可关闭）

五语言 SDK **默认开启**匿名用量统计：在进程内将事件入队，定时或满批后合并为一次 **`POST`**，JSON 体为 **`schema_version: 2`**，包含 `sdk_language`、`sdk_version`、`os`、`arch` 以及 `events[]`（如 `operation`、`channel`、`success`、`attempt_count`、`channels_tried`、脱敏后的 `error`、`ts_ms` 等）。**不包含**邮件正文或 Token；错误文案里形似邮箱的片段会替换为 `[redacted]`。

| 环境变量 | 说明 |
|---------|------|
| `TEMPMAIL_TELEMETRY_ENABLED` | `true` / `1` / `yes` 开启（默认），`false` / `0` / `no` 关闭 |
| `TEMPMAIL_TELEMETRY_URL` | 覆盖默认上报 URL（内置默认一般为 `https://sdk-1.openel.top/v1/event`，以各 SDK 源码为准） |

也可在代码里通过全局配置关闭或指定 URL：**Go** `SDKConfig.TelemetryEnabled` / `TelemetryEndpoint`；**npm** `telemetryEnabled` / `telemetryUrl`；**Rust** `telemetry_enabled` / `telemetry_url`；**Python** `SDKConfig.telemetry_enabled` / `telemetry_url`；**C** `tm_config_t.telemetry_enabled` / `telemetry_url`。

本仓库下的 **`telemetry-server/`** 为可选的收集服务示例（`POST /v1/event` 写入 SQLite 等）；**SDK 上报不需要令牌**，与自建服务的管理端鉴权无关。

### HTTP 重试

各语言在「生成邮箱」「拉取邮件」上均支持 **Retry**（如最大次数、超时、退避），具体字段名见对应 SDK 的 `README.md` 与类型定义（如 npm 的 `retry`、`Go` 的 `RetryOptions` 等）。

## 📖 API 文档

详细 API 文档请参阅各 SDK 目录：

| SDK | 文档 | 注册表 |
|-----|------|--------|
| Go | [sdk/go/README.md](./sdk/go/README.md) | [pkg.go.dev](https://pkg.go.dev/github.com/XxxXTeam/tempmail-sdk/sdk/go) |
| npm | [sdk/npm/README.md](./sdk/npm/README.md) | [npmjs.org](https://www.npmjs.com/package/tempmail-sdk) |
| Rust | [sdk/rust/README.md](./sdk/rust/README.md) | [crates.io](https://crates.io/crates/tempmail-sdk) |
| Python | [sdk/python/README.md](./sdk/python/README.md) | [PyPI](https://pypi.org/project/tempemail-sdk/) |
| C | [sdk/c/README.md](./sdk/c/README.md) | [GitHub Releases](https://github.com/XxxXTeam/tempmail-sdk/releases) |

## 🔧 项目结构

```
tempmail-sdk/
├── sdk/
│   ├── go/                     # Go SDK
│   │   ├── client.go           # 入口文件
│   │   ├── types.go            # 类型定义
│   │   ├── normalize.go        # 标准化转换
│   │   └── provider/*.go       # 各渠道实现（如 ta_easy.go、catchmail.go、mailforspam.go）
│   ├── npm/                    # npm SDK (TypeScript)
│   │   ├── src/
│   │   │   ├── index.ts        # 入口文件
│   │   │   ├── types.ts        # 类型定义
│   │   │   └── providers/      # 各渠道实现
│   │   ├── demo/               # 示例与探针脚本
│   │   └── test/               # 测试代码
│   ├── rust/                   # Rust SDK
│   │   ├── src/
│   │   │   ├── lib.rs          # 库入口
│   │   │   ├── client.rs       # Client 实现
│   │   │   └── providers/      # 各渠道实现
│   │   └── examples/           # 示例代码
│   ├── python/                 # Python SDK
│   │   ├── tempmail_sdk/
│   │   │   ├── __init__.py     # 入口
│   │   │   ├── client.py       # Client 实现
│   │   │   └── providers/      # 各渠道实现
│   │   └── pyproject.toml      # 包配置
│   └── c/                      # C SDK
│       ├── include/            # 公共头文件
│       ├── src/                # 源码
│       │   └── providers/      # 各渠道实现
│       └── CMakeLists.txt      # 构建配置
├── telemetry-server/           # 可选：遥测收集服务（Gin + SQLite 等，见目录内实现）
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
git tag v1.1.0
git push origin v1.1.0
```

这将自动：
1. 验证全部 5 种 SDK 构建
2. 发布 npm 包到 npmjs.org 和 GitHub Packages
3. 发布 Rust crate 到 crates.io
4. 发布 Python wheel 到 PyPI
5. 构建 Go / Rust / C 多平台二进制
6. 创建 GitHub Release（附带全部构建产物 + 变更日志）

### 配置 Secrets

在 GitHub 仓库 Settings → Secrets and variables → Actions 中添加：

| Secret | 说明 |
|--------|------|
| `NPM_TOKEN` | npm 访问令牌 |
| `CARGO_REGISTRY_TOKEN` | crates.io API Token |
| `PYPI_TOKEN` | PyPI API Token |

> `GITHUB_TOKEN` 由 GitHub Actions 自动提供，无需手动配置。

## 🛠️ 开发

### Go SDK

```bash
cd sdk/go
go build ./...
gofmt -d .
```

### npm SDK

```bash
cd sdk/npm
npm install
npm run build
npx tsc --noEmit
npm test
```

### Rust SDK

```bash
cd sdk/rust
cargo build
cargo test
cargo clippy
```

### Python SDK

```bash
cd sdk/python
pip install -e ".[dev]"
pytest
```

### C SDK

```bash
cd sdk/c
# 下载 cJSON 依赖
curl -L -o vendor/cJSON.h https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.h
curl -L -o vendor/cJSON.c https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.c
cmake -B build -S . && cmake --build build
```

## ⭐ Star 历史

<a href="https://star-history.com/#XxxXTeam/tempmail-sdk&Date">
 <picture>
   <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=XxxXTeam/tempmail-sdk&type=Date&theme=dark" />
   <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=XxxXTeam/tempmail-sdk&type=Date" />
   <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=XxxXTeam/tempmail-sdk&type=Date" />
 </picture>
</a>

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！请阅读 [贡献指南](CONTRIBUTING.md) 了解详情。

### 快速开始

1. Fork 本仓库
2. 创建功能分支：`git checkout -b feat/your-feature`
3. 提交代码并推送
4. 创建 Pull Request

### 添加新的渠道提供商

1. 在各 SDK 的 `providers/` 目录下新建提供商文件
2. 实现 `generateEmail()` 和 `getEmails()` 两个核心函数
3. 在各 SDK 的 Channel 类型/枚举中注册新渠道
4. 使用 `normalizeEmail()` 标准化返回数据
5. 所有 HTTP 请求使用全局共享客户端（支持代理/TLS 配置）
6. 更新 README 文档

详见 [CONTRIBUTING.md](CONTRIBUTING.md) 中的完整指南和代码示例。

## 📄 许可证

本项目基于 [GPL-3.0](LICENSE) 许可证开源。

```
Copyright (C) 2026 XxxXTeam

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
```
