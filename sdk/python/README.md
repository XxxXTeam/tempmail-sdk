# tempmail-sdk (Python)

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

临时邮箱 SDK（Python），公开 **268** 个 `channel` 标识，按独立服务商合并为 **100** 个 provider。固定域名、裸域、镜像域名和同一 API 的多域名只算同一个独立服务商。顺序与 `client.py` 中 `ALL_CHANNELS` 一致，返回格式与根目录 README 描述一致，并与 Go / npm / Rust / C 对齐。随机生成邮箱时会在本端独立打乱尝试顺序，不需要与其他 SDK 的随机顺序一致。

## 安装

```bash
# 从 PyPI
pip install tempemail-sdk

# 从 GitHub Release（wheel）
pip install https://github.com/XxxXTeam/tempmail-sdk/releases/latest/download/tempemail_sdk-1.1.0-py3-none-any.whl
```

## 支持的渠道

| 渠道 | 服务商 | 需要 Token | 说明 |
|------|--------|:----------:|------|
| `tempmail` | tempmail.ing | - | 支持自定义有效期 |
| `tempmail-cn` | tempmail.cn | - | Socket.IO：`request shortid` / `set shortid` / `mail`；`GenerateEmailOptions.domain` 可指定自定义接入域名 |
| `ta-easy` | ta-easy.com | ✅ | REST `api-endpoint.ta-easy.com` |
| `10minute-one` | 10minutemail.one | ✅ | SSR / JWT + Web API；`GenerateEmailOptions.domain` 可选 |
| `xghff-com` | xghff.com | ✅ | 10minutemail.one 固定域名 `xghff.com` |
| `oqqaj-com` | oqqaj.com | ✅ | 10minutemail.one 固定域名 `oqqaj.com` |
| `psovv-com` | psovv.com | ✅ | 10minutemail.one 固定域名 `psovv.com` |
| `dbwot-com` | dbwot.com | ✅ | 10minutemail.one 固定域名 `dbwot.com` |
| `ygwpr-com` | ygwpr.com | ✅ | 10minutemail.one 固定域名 `ygwpr.com` |
| `imxwe-com` | imxwe.com | ✅ | 10minutemail.one 固定域名 `imxwe.com` |
| `linshiyou` | linshiyou.com | ✅ | `NEXUS_TOKEN` + Cookie；HTML 分段解析 |
| `mffac` | mffac.com | ✅ | mailbox `id`；`/api/emails/{id}` 详情补齐 `text` / `html`；REST 24h |
| `tempmail-lol` | tempmail.lol | ✅ | 支持指定域名 |
| `chatgpt-org-uk` | mail.chatgpt.org.uk | ✅ | Inbox Token 等由 SDK 封装 |
| `temp-mail-io` | temp-mail.io | ✅ | REST：动态读取 `mobileTestingHeader` 后调用 `api.internal.temp-mail.io/api/v3` |
| `mail-cx` | mail.cx | ✅ | 匿名 Web API：`/v1/config` 获取系统域名，`/v1/inbox/{email}` 长轮询收信，内部保存 `X-Client-ID` |
| `ddker-com` | ddker.com | ✅ | mail.cx 固定域名 `ddker.com` |
| `catchmail` | catchmail.io | - | 公开 REST：`/api/v1/mailbox?address=` + `/api/v1/message/{id}?mailbox=`；详情含 `body.text` / `body.html` |
| `catchmail-mailistry` | mailistry.com | - | Catchmail API 固定域名 `mailistry.com` |
| `catchmail-zeppost` | zeppost.com | - | Catchmail API 固定域名 `zeppost.com` |
| `mailforspam` | mailforspam.com | - | 公开 REST：`/api/mailboxes/{email}/emails` + `/api/mailboxes/{email}/emails/{id}`；详情含 `body_text` / `body_html` |
| `mailforspam-tempmail-io` | tempmail.io | - | MailForSpam API 固定域名 `tempmail.io` |
| `mailforspam-disposable` | disposable.email | - | MailForSpam API 固定域名 `disposable.email` |
| `tempmailc` | tempmailc.com | - | Public API：`GET /api/v1/new` 建址，`GET /api/v1/inbox` 拉列表，`GET /api/v1/message` 读取 `text` / `html` 正文 |
| `mailnesia` | mailnesia.com | - | 任意 `{local}@mailnesia.com` 建址；HTML 列表 `tr.emailheader` + 详情 `text_plain_{id}` / `text_html_{id}` 正文 |
| `throwawaymail` | throwawaymail.app | ✅ | Web API 建址并轮询收信；Token 由 SDK 内部维护 |
| `shitty-email` | shitty.email | ✅ | `POST /api/inbox` 建址；`X-Session-Token` + `GET /api/inbox` 拉列表，`GET /api/email/{id}` 读取 `text` / `html` 正文 |
| `tempmailpro` | tempmailpro.us | ✅ | `POST /api/v1/mailbox/create` 建箱；`GET /api/v1/mailbox/{token}/emails` 拉列表，详情 `body_text` / `body_html` 映射统一正文 |
| `devmail-uk` | devmail.uk | - | `GET /api/new` 建址；`GET /api/inbox/{mailbox}?detail=true` 拉列表；生成接口返回的 `email` / `mailbox` 字段均兼容解析 |
| `inboxkitten` | inboxkitten.com | - | 公开 API 拉取收件箱列表与详情 |
| `cleantempmail` | cleantempmail.com | - | `GET /api/generate-email` 建址；`GET /api/emails?email=` 拉列表；公开 API 通过 `X-API-Key` 头访问 |
| `getnada` | getnada.net | ✅ | `POST /api/inbox/open` 建箱；`GET /api/inbox/messages` 列表；`GET /api/inbox/message` 详情含 `text_plain` / `html_sanitized` |
| `1vpn-net` | 1vpn.net | ✅ | GetNada 固定域名 `1vpn.net` |
| `abematv-com` | abematv.com | ✅ | GetNada 固定域名 `abematv.com` |
| `abematv-net` | abematv.net | ✅ | GetNada 固定域名 `abematv.net` |
| `abematv-org` | abematv.org | ✅ | GetNada 固定域名 `abematv.org` |
| `aceh-cc` | aceh.cc | ✅ | GetNada 固定域名 `aceh.cc` |
| `bangkabelitung-net` | bangkabelitung.net | ✅ | GetNada 固定域名 `bangkabelitung.net` |
| `cctruyen-com` | cctruyen.com | ✅ | GetNada 固定域名 `cctruyen.com` |
| `getnada-com` | getnada.com | ✅ | GetNada 固定域名 `getnada.com` |
| `getnada-email` | getnada.email | ✅ | GetNada 固定域名 `getnada.email` |
| `getnada-net` | getnada.net | ✅ | GetNada 固定域名 `getnada.net` |
| `jawatengah-net` | jawatengah.net | ✅ | GetNada 固定域名 `jawatengah.net` |
| `jawatimur-net` | jawatimur.net | ✅ | GetNada 固定域名 `jawatimur.net` |
| `kalimantanbarat-net` | kalimantanbarat.net | ✅ | GetNada 固定域名 `kalimantanbarat.net` |
| `kalimantanselatan-net` | kalimantanselatan.net | ✅ | GetNada 固定域名 `kalimantanselatan.net` |
| `kalimantantengah-net` | kalimantantengah.net | ✅ | GetNada 固定域名 `kalimantantengah.net` |
| `kalimantantimur-net` | kalimantantimur.net | ✅ | GetNada 固定域名 `kalimantantimur.net` |
| `kalimantanutara-net` | kalimantanutara.net | ✅ | GetNada 固定域名 `kalimantanutara.net` |
| `kepulauanriau-net` | kepulauanriau.net | ✅ | GetNada 固定域名 `kepulauanriau.net` |
| `luxury345-com` | luxury345.com | ✅ | GetNada 固定域名 `luxury345.com` |
| `malukuutara-net` | malukuutara.net | ✅ | GetNada 固定域名 `malukuutara.net` |
| `nusatenggarabarat-net` | nusatenggarabarat.net | ✅ | GetNada 固定域名 `nusatenggarabarat.net` |
| `nusatenggaratimur-net` | nusatenggaratimur.net | ✅ | GetNada 固定域名 `nusatenggaratimur.net` |
| `papuabarat-net` | papuabarat.net | ✅ | GetNada 固定域名 `papuabarat.net` |
| `papuabaratdaya-net` | papuabaratdaya.net | ✅ | GetNada 固定域名 `papuabaratdaya.net` |
| `papuaselatan-net` | papuaselatan.net | ✅ | GetNada 固定域名 `papuaselatan.net` |
| `pehol-com` | pehol.com | ✅ | GetNada 固定域名 `pehol.com` |
| `ptruyen-com` | ptruyen.com | ✅ | GetNada 固定域名 `ptruyen.com` |
| `pulaubali-net` | pulaubali.net | ✅ | GetNada 固定域名 `pulaubali.net` |
| `riau-net` | riau.net | ✅ | GetNada 固定域名 `riau.net` |
| `seokey-org` | seokey.org | ✅ | GetNada 固定域名 `seokey.org` |
| `sulawesibarat-net` | sulawesibarat.net | ✅ | GetNada 固定域名 `sulawesibarat.net` |
| `sulawesiselatan-net` | sulawesiselatan.net | ✅ | GetNada 固定域名 `sulawesiselatan.net` |
| `sulawesitengah-net` | sulawesitengah.net | ✅ | GetNada 固定域名 `sulawesitengah.net` |
| `sulawesitenggara-net` | sulawesitenggara.net | ✅ | GetNada 固定域名 `sulawesitenggara.net` |
| `sumaterabarat-net` | sumaterabarat.net | ✅ | GetNada 固定域名 `sumaterabarat.net` |
| `sumateraselatan-net` | sumateraselatan.net | ✅ | GetNada 固定域名 `sumateraselatan.net` |
| `sumaterautara-net` | sumaterautara.net | ✅ | GetNada 固定域名 `sumaterautara.net` |
| `villatogel-com` | villatogel.com | ✅ | GetNada 固定域名 `villatogel.com` |
| `mail123` | mail123.fr | - | `GET /api/v1/mailbox/new` 建址；`GET /api/v1/mailbox/{address}/messages?limit=50` 列表；详情含 `text` / `html` |
| `mail10s` | mail10s.com | - | 任意随机地址 + 公开 inbox API；`sender` / `body_text` / `body_html` 映射统一正文 |
| `webmailtemp` | webmailtemp.com | ✅ | `GET /api/create` 建箱；会话 Cookie + `GET /api/check/{username}` 收信；30 分钟 TTL |
| `tempfastmail` | tempfastmail.com | ✅ | `POST /api/email-box` 建箱；邮箱 UUID 拉列表和详情；详情 `html` 由 normalize 派生 `text` |
| `1sec-mail` | tmaily.com | ✅ | `GET /generate` 取地址；从 `Set-Cookie` 提取 `TMaily_sid`；`GET /emails?address=` 拉列表，详情由 `body_text` / `html` 映射，缺失正文由 normalize 反向生成 |
| `fakemail` | fakemail.net | ✅ | `/index/index` 建址，`/index/refresh` 拉列表，`/index/email` 详情；`telo` HTML 正文 |
| `openinbox` | openinbox.io | ✅ | `POST /api/inbox` 建箱；`GET /emails/inbox/{id}` 列表；`GET /emails/{emailId}` 详情含 `textBody` / `htmlBody` |
| `inboxes` | inboxes.com | - | 公开 v2：`GET /api/v2/domain` 域名，`GET /api/v2/inbox/{email}` 列表，`GET /api/v2/message/{uid}` 详情含 `text` / `html` |
| `uncorreotemporal` | uncorreotemporal.com | ✅ | `POST /api/v1/mailboxes` 建箱；`X-Session-Token` 拉取列表和详情；详情含 `body_text` / `body_html` |
| `awamail` | awamail.com | ✅ | Session Cookie 自动管理 |
| `mail-tm` | mail.tm | ✅ | 自动注册账号获取 Bearer Token |
| `web-library-net` | web-library.net | ✅ | mail.tm 固定域名 `web-library.net` |
| `dropmail` | dropmail.me | ✅ | GraphQL API |
| `guerrillamail` | guerrillamail.com | ✅ | 公开 JSON API |
| `guerrillamail-com` | guerrillamail.com | ✅ | GuerrillaMail 裸域 JSON API 入口 |
| `maildrop` | maildrop.cx | ✅ | REST：`suffixes.php` + `emails.php`；`description`→`text` |
| `smail-pw` | smail.pw | ✅ | `__session` Cookie；正则 + JSON 遍历解析 Flight 中的邮件行对象 |
| `vip-215` | vip.215.im | ✅ | `POST` 建箱 + WebSocket；无正文时 synthetic 兜底 |
| `fake-legal` | fake.legal | - | `/api/domains` + `/api/inbox/new`；可选 `GenerateEmailOptions.domain` |
| `imgui-de` | imgui.de | - | fake.legal 固定域名 `imgui.de` |
| `pulsewebmenu-de` | pulsewebmenu.de | - | fake.legal 固定域名 `pulsewebmenu.de` |
| `moakt` | moakt.com | ✅ | HTML 收件箱 + `tm_session`；`domain` 可选语言路径；独立 `requests` 请求避免污染全局 Session Cookie |
| `drmail-in` | drmail.in | ✅ | Moakt 固定域名 `drmail.in` |
| `teml-net` | teml.net | ✅ | Moakt 固定域名 `teml.net` |
| `tmpeml-com` | tmpeml.com | ✅ | Moakt 固定域名 `tmpeml.com` |
| `tmpbox-net` | tmpbox.net | ✅ | Moakt 固定域名 `tmpbox.net` |
| `moakt-cc` | moakt.cc | ✅ | Moakt 固定域名 `moakt.cc` |
| `disbox-net` | disbox.net | ✅ | Moakt 固定域名 `disbox.net` |
| `tmpmail-org` | tmpmail.org | ✅ | Moakt 固定域名 `tmpmail.org` |
| `tmpmail-net` | tmpmail.net | ✅ | Moakt 固定域名 `tmpmail.net` |
| `tmails-net` | tmails.net | ✅ | Moakt 固定域名 `tmails.net` |
| `disbox-org` | disbox.org | ✅ | Moakt 固定域名 `disbox.org` |
| `moakt-co` | moakt.co | ✅ | Moakt 固定域名 `moakt.co` |
| `moakt-ws` | moakt.ws | ✅ | Moakt 固定域名 `moakt.ws` |
| `tmail-ws` | tmail.ws | ✅ | Moakt 固定域名 `tmail.ws` |
| `bareed-ws` | bareed.ws | ✅ | Moakt 固定域名 `bareed.ws` |
| `email10min` | email10min.com | ✅ | Cookie + CSRF；`POST /messages` 获取邮箱与邮件 |
| `mjj-cm` | mjj.cm | ✅ | Socket.IO：`request shortid` / `set shortid` / `mail` |
| `linshi-co` | linshi.co | ✅ | Socket.IO 克隆站，协议同 `mjj-cm` |
| `harakirimail` | harakirimail.com | - | 公开 REST：`GET /api/v1/inbox/{name}` + `GET /api/v1/email/{id}` |
| `jqkjqk-xyz` | jqkjqk.xyz | ✅ | mail.zhujump.com 固定域名 `jqkjqk.xyz` |
| `lyhlevi-com` | lyhlevi.com | ✅ | MoeMail 部署实例；注册登录后创建邮箱，列表/详情映射统一正文 |
| `tempmail-plus` | tempmail.plus | - | 公开 REST：`GET /api/mails/?email=` 列表，`GET /api/mails/{id}?email=` 详情 |
| `fexpost-com` | fexpost.com | - | TempMail Plus 固定域名 `fexpost.com` |
| `fexbox-org` | fexbox.org | - | TempMail Plus 固定域名 `fexbox.org` |
| `mailbox-in-ua` | mailbox.in.ua | - | TempMail Plus 固定域名 `mailbox.in.ua` |
| `rover-info` | rover.info | - | TempMail Plus 固定域名 `rover.info` |
| `chitthi-in` | chitthi.in | - | TempMail Plus 固定域名 `chitthi.in` |
| `fextemp-com` | fextemp.com | - | TempMail Plus 固定域名 `fextemp.com` |
| `any-pink` | any.pink | - | TempMail Plus 固定域名 `any.pink` |
| `merepost-com` | merepost.com | - | TempMail Plus 固定域名 `merepost.com` |
| `tempmail-lol-v2` | tempmail.lol | ✅ | `GET /generate` 返回 address+token，`GET /auth/{token}` 拉取收件箱 |
| `tempgbox` | tempgbox.net | ✅ | `POST /api/proxy?route=generate` 使用 `variant=googlemail`；每次生成随机 `X-Device-ID` 并随请求带随机来源 IP 头 |
| `emailnator` | emailnator.com | ✅ | XSRF + Cookie；Gmail/GoogleMail alias 选项生成，`messageID` 读取 HTML 正文 |
| `temporam` | temporam.com | - | 公开 REST：`/api/domains`、`/api/emails?email=`、`/api/emails/{id}` |
| `neighbours` | neighbours.sh | - | `GET /config/domains` 获取域名；`GET /inbox/{address}` / `GET /inbox/{address}/{uid}` 拉信；`404` 视为空收件箱 |
| `sharklasers` | sharklasers.com | ✅ | GuerrillaMail 镜像，API 与 `guerrillamail` 相同 |
| `sharklasers-com` | sharklasers.com | ✅ | GuerrillaMail 裸域镜像，API 与 `guerrillamail` 相同 |
| `grr-la` | grr.la | ✅ | GuerrillaMail 镜像，API 与 `guerrillamail` 相同 |
| `grr-la-com` | grr.la | ✅ | GuerrillaMail 裸域镜像，API 与 `guerrillamail` 相同 |
| `guerrillamail-info` | guerrillamail.info | ✅ | GuerrillaMail 镜像，API 与 `guerrillamail` 相同 |
| `spam4me` | spam4.me | ✅ | GuerrillaMail 镜像，API 与 `guerrillamail` 相同 |
| `guerrillamail-net` | guerrillamail.net | ✅ | GuerrillaMail 镜像，API 与 `guerrillamail` 相同 |
| `guerrillamail-org` | guerrillamail.org | ✅ | GuerrillaMail 镜像，API 与 `guerrillamail` 相同 |
| `guerrillamailblock` | guerrillamailblock.com | ✅ | GuerrillaMail 镜像，API 与 `guerrillamail` 相同 |
| `guerrillamail-com-www` | guerrillamail.com | ✅ | GuerrillaMail `www` JSON API 入口 |
| `m2u` | MailToYou | ✅ | `POST /v1/mailboxes/auto` 建箱；`token` + `viewToken` 组成内部 Token；`GET /v1/mailboxes/{token}/messages?view={viewToken}` 拉列表，`GET /v1/mailboxes/{token}/messages/{id}?view={viewToken}` 读取详情 |
| `tempy-email` | Tempy Email | - | `POST /api/v1/mailbox` 建箱（请求体可为空 JSON；可选 `domain`）；`GET /api/v1/mailbox/{email}/messages` 拉列表 |
| `fmail` | fmail.men | - | `GET /v1/random` 建址；`GET /v1/inbox/{local}?domain={domain}&limit=50` 拉列表；`GET /v1/email/{token}` 读取单封详情，可选 `domain` |
| `ockito` | ockito.com | ✅ | `POST /gtoken` 换取 access_token / refresh_token；`GET /email` 建址；`GET /retrieve/{email}/emails` 拉列表，`GET /retrieve/{email}/{uid}` 详情 |
| `anonbox` | anonbox.net | ✅ | `GET /en/` 解析页面建箱；`GET /{token}/` 拉信，token 形如 `inbox/secret`，mbox 明文解析 |
| `duckmail` | duckmail.sbs | ✅ | `GET /domains?page=1` 取域名；`POST /accounts` 创建账号；`POST /token` 获取 Bearer Token；`GET /messages` 拉列表 |
| `mailinator` | mailinator.com | - | 公开 REST：`GET /api/v2/domains/public/inboxes/{inbox}` 拉列表；`GET /api/v2/domains/public/messages/{id}/{text|texthtml|attachments}` 详情 |
| `tempmail365` |  | ✅ | tempmail365 |
| `tempinbox` |  | ✅ | tempinbox |
| `byom` |  | ✅ | byom |
| `anonymmail` |  | ✅ | anonymmail |
| `eyepaste` |  | ✅ | eyepaste |
| `mail-sunls` |  | ✅ | mail-sunls |
| `expressinboxhub` |  | ✅ | expressinboxhub |
| `lroid` |  | ✅ | lroid |
| `haribu` |  | ✅ | haribu |
| `rootsh` |  | ✅ | rootsh |
| `fake-email-site` | fake-email.site | - | `POST /api/temporary-address` 建箱；`GET /api/inbox/poll?address=` 拉信；返回 `temp_email_addr` / `messages` |
| `mohmal` |  | ✅ | mohmal |
| `mailgolem` |  | ✅ | mailgolem |
| `best-temp-mail` |  | ✅ | best-temp-mail |
| `disposablemail-app` |  | ✅ | disposablemail-app |
| `mailtemp-cc` |  | ✅ | mailtemp-cc |
| `minuteinbox` |  | ✅ | minuteinbox |
| `mailcatch` |  | ✅ | mailcatch |
| `tempemail-co` |  | ✅ | tempemail-co |
| `tempemails-net` |  | ✅ | tempemails-net |
| `altmails` |  | ✅ | altmails |
| `tempemail-info` |  | ✅ | tempemail-info |
| `smailpro` |  | ✅ | smailpro |
| `tempmailten` |  | ✅ | tempmailten |
| `maildrop-cc` |  | ✅ | maildrop-cc |
| `10minutemail-net` |  | ✅ | 10minutemail-net |
| `linshiyouxiang-net` |  | ✅ | linshiyouxiang-net |
| `tempmail-fyi` |  | ✅ | tempmail-fyi |
| `disposablemail-com` |  | ✅ | disposablemail-com |
| `tempp-mails` |  | ✅ | tempp-mails |
| `emailtemp-org` |  | ✅ | emailtemp-org |
| `mytempmail-cc` |  | ✅ | mytempmail-cc |
| `temp-mail-now` |  | ✅ | temp-mail-now |
| `mail-td` |  | ✅ | mail-td |
| `mailhole-de` |  | ✅ | mailhole-de |
| `tmail-link` |  | ✅ | tmail-link |
| `24mail-chacuo` |  | ✅ | 24mail-chacuo |
| `nimail` |  | ✅ | nimail |
| `freecustom` |  | ✅ | freecustom |
| `16888888-cyou` |  | ✅ | 16888888-cyou |
| `17666688-shop` |  | ✅ | 17666688-shop |
| `282mail-com` |  | ✅ | 282mail-com |
| `blackhole-djurby-se` |  | ✅ | blackhole-djurby-se |
| `block-bdea-cc` |  | ✅ | block-bdea-cc |
| `bsdu32-buzz` |  | ✅ | bsdu32-buzz |
| `b-smelly-cc` |  | ✅ | b-smelly-cc |
| `carlton183-changeip-net` |  | ✅ | carlton183-changeip-net |
| `dea-soon-it` |  | ✅ | dea-soon-it |
| `disposable-al-sudani-com` |  | ✅ | disposable-al-sudani-com |
| `disposable-nogonad-nl` |  | ✅ | disposable-nogonad-nl |
| `doxu243-buzz` |  | ✅ | doxu243-buzz |
| `easyme-pro` |  | ✅ | easyme-pro |
| `ebs-com-ar` |  | ✅ | ebs-com-ar |
| `etgdev-de` |  | ✅ | etgdev-de |
| `evergreenco-shop` |  | ✅ | evergreenco-shop |
| `fwd2m-eszett-es` |  | ✅ | fwd2m-eszett-es |
| `jama-trenet-eu` |  | ✅ | jama-trenet-eu |
| `j-fairuse-org` |  | ✅ | j-fairuse-org |
| `layueming-pics` |  | ✅ | layueming-pics |
| `m-887-at` |  | ✅ | m-887-at |
| `m8r-davidfuhr-de` |  | ✅ | m8r-davidfuhr-de |
| `m8r-mcasal-com` |  | ✅ | m8r-mcasal-com |
| `mail-bentrask-com` |  | ✅ | mail-bentrask-com |
| `mail-fsmash-org` |  | ✅ | mail-fsmash-org |
| `mailinatorzz-mooo-com` |  | ✅ | mailinatorzz-mooo-com |
| `mi-meon-be` |  | ✅ | mi-meon-be |
| `mingyuekeji-online` |  | ✅ | mingyuekeji-online |
| `mingyueming-click` |  | ✅ | mingyueming-click |
| `mingyueming-shop` |  | ✅ | mingyueming-shop |
| `mingyukeji-lol` |  | ✅ | mingyukeji-lol |
| `mn-curppa-com` |  | ✅ | mn-curppa-com |
| `m-nik-me` |  | ✅ | m-nik-me |
| `mtmdev-com` |  | ✅ | mtmdev-com |
| `nospam-thurstons-us` |  | ✅ | nospam-thurstons-us |
| `notfond-404-mn` |  | ✅ | notfond-404-mn |
| `null-k3vin-net` |  | ✅ | null-k3vin-net |
| `nuxh62-space` |  | ✅ | nuxh62-space |
| `proid-cloud-ip-cc` |  | ✅ | proid-cloud-ip-cc |
| `ramjane-mooo-com` |  | ✅ | ramjane-mooo-com |
| `rauxa-seny-cat` |  | ✅ | rauxa-seny-cat |
| `really-istrash-com` |  | ✅ | really-istrash-com |
| `sbook-pics` |  | ✅ | sbook-pics |
| `spam-hortuk-ovh` |  | ✅ | spam-hortuk-ovh |
| `sp-woot-at` |  | ✅ | sp-woot-at |
| `test-unergie-com` |  | ✅ | test-unergie-com |
| `torch-yi-org` |  | ✅ | torch-yi-org |
| `t-zibet-net` |  | ✅ | t-zibet-net |
| `xue32-buzz` |  | ✅ | xue32-buzz |
| `apihz` |  | ✅ | apihz |
| `sogetthis-com` |  | ✅ | sogetthis-com |
| `bobmail-info` |  | ✅ | bobmail-info |
| `suremail-info` |  | ✅ | suremail-info |
| `binkmail-com` |  | ✅ | binkmail-com |
| `veryrealemail-com` |  | ✅ | veryrealemail-com |
| `mailmomy` |  | ✅ | mailmomy |
| `chammy-info` |  | ✅ | chammy-info |
| `thisisnotmyrealemail-com` |  | ✅ | thisisnotmyrealemail-com |
| `notmailinator-com` |  | ✅ | notmailinator-com |
| `spamhereplease-com` |  | ✅ | spamhereplease-com |
| `sendspamhere-com` |  | ✅ | sendspamhere-com |
| `sendfree-org` |  | ✅ | sendfree-org |
| `junk-beats-org` |  | ✅ | junk-beats-org |
| `junk-ihmehl-com` |  | ✅ | junk-ihmehl-com |
| `junk-noplay-org` |  | ✅ | junk-noplay-org |
| `junk-vanillasystem-com` |  | ✅ | junk-vanillasystem-com |
| `spam-jasonpearce-com` |  | ✅ | spam-jasonpearce-com |
| `fish-skytale-net` |  | ✅ | fish-skytale-net |
| `spam-mccrew-com` |  | ✅ | spam-mccrew-com |
| `dropmail-click` |  | ✅ | dropmail-click |
| `spam-coroiu-com` |  | ✅ | spam-coroiu-com |
| `spam-deluser-net` |  | ✅ | spam-deluser-net |
| `spam-dhsf-net` |  | ✅ | spam-dhsf-net |
| `spam-lucatnt-com` |  | ✅ | spam-lucatnt-com |
| `spam-lyceum-life-com-ru` |  | ✅ | spam-lyceum-life-com-ru |
| `spam-netpirates-net` |  | ✅ | spam-netpirates-net |
| `spam-no-ip-net` |  | ✅ | spam-no-ip-net |
| `spam-ozh-org` |  | ✅ | spam-ozh-org |
| `spam-pyphus-org` |  | ✅ | spam-pyphus-org |
| `spam-shep-pw` |  | ✅ | spam-shep-pw |
| `spam-wtf-at` |  | ✅ | spam-wtf-at |
| `spam-wulczer-org` |  | ✅ | spam-wulczer-org |
| `crap-kakadua-net` |  | ✅ | crap-kakadua-net |
| `spam-janlugt-nl` |  | ✅ | spam-janlugt-nl |
| `min-burningfish-net` |  | ✅ | min-burningfish-net |
| `sink-fblay-com` |  | ✅ | sink-fblay-com |

## 快速开始

```python
from tempmail_sdk import generate_email, get_emails, GenerateEmailOptions

# 创建临时邮箱
info = generate_email(GenerateEmailOptions(channel="guerrillamail"))
if info:
    print(f"邮箱: {info.email}")

    # 获取邮件（channel / email / token 由 SDK 从 EmailInfo 读取，无需手动传入）
    result = get_emails(info)
    if result.success:
        print(f"收到 {len(result.emails)} 封邮件")
```

## 使用客户端类

```python
from tempmail_sdk import TempEmailClient, GenerateEmailOptions

client = TempEmailClient()
info = client.generate(GenerateEmailOptions(channel="maildrop"))
result = client.get_emails()
```

## 日志

```python
from tempmail_sdk import set_log_level, LOG_DEBUG

set_log_level(LOG_DEBUG)  # 开启所有日志
```

## 代理与 HTTP 配置

SDK 支持全局配置代理、超时等 HTTP 客户端参数，也可通过环境变量零代码配置：

```python
from tempmail_sdk import set_config

# 一行跳过 SSL 验证
set_config(insecure=True)

# 设置代理
set_config(proxy="http://127.0.0.1:7890")

# 设置代理 + 超时 + 跳过 SSL 验证
set_config(proxy="socks5://127.0.0.1:1080", timeout=30, insecure=True)

# 添加自定义请求头
set_config(headers={"X-Custom": "value"})
```

**配置项（`SDKConfig` / `set_config`）：**

| 参数 | 类型 | 说明 |
|------|------|------|
| `proxy` | `str` | 代理 URL（http/https/socks5） |
| `timeout` | `int` | 全局超时秒数，默认 15 |
| `insecure` | `bool` | 跳过 SSL 验证（调试用） |
| `headers` | `dict` | 自定义请求头 |
| `dropmail_auth_token` 等 | 见 `config.py` | DropMail `DROPMAIL_*` 环境变量亦可 |
| `telemetry_enabled` | `Optional[bool]` | `None` 默认开启；`False` 关闭匿名遥测 |
| `telemetry_url` | `Optional[str]` | 覆盖默认上报 URL |

**环境变量（无需修改代码）：**

```bash
export TEMPMAIL_PROXY="http://127.0.0.1:7890"
export TEMPMAIL_INSECURE=1
export TEMPMAIL_TIMEOUT=30
export DROPMAIL_AUTH_TOKEN="af_..."
export DROPMAIL_NO_AUTO_TOKEN=1
export TEMPMAIL_TELEMETRY_ENABLED=false
export TEMPMAIL_TELEMETRY_URL="https://example.com/v1/event"
```

## 匿名遥测

默认 **开启**：批量 `POST` 匿名事件（`schema_version: 2`），内置默认 URL 见 `telemetry.py`。关闭：`TEMPMAIL_TELEMETRY_ENABLED=false`（或 `0` / `no`）或 `set_config(telemetry_enabled=False)`；改 URL：`TEMPMAIL_TELEMETRY_URL` 或 `telemetry_url`。

## 重试配置

```python
from tempmail_sdk import generate_email, GenerateEmailOptions, RetryConfig

info = generate_email(GenerateEmailOptions(
    channel="mail-gw",
    retry=RetryConfig(max_retries=3, initial_delay=2.0),
))
```

拉取邮件同样可传 `GetEmailsOptions(retry=RetryConfig(...))`。
