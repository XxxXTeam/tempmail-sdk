# tempmail-sdk-go

[![Go Reference](https://pkg.go.dev/badge/github.com/XxxXTeam/tempmail-sdk/sdk/go.svg)](https://pkg.go.dev/github.com/XxxXTeam/tempmail-sdk/sdk/go)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

Go 语言临时邮箱 SDK，公开 **176** 个 `channel` 标识，按独立服务商合并为 **67** 个 provider。固定域名、裸域、镜像域名和同一 API 的多域名只算同一个独立服务商。所有渠道返回**统一标准化格式**。`ListChannels` 顺序与 `client.go` 中 `allChannels` 一致，并与 npm / Rust / Python / C 的公共列表顺序对齐；随机生成邮箱时会在本端独立打乱尝试顺序，不需要与其他 SDK 的随机顺序一致。

## 安装

```bash
go get github.com/XxxXTeam/tempmail-sdk/sdk/go
```

## 支持的渠道

| 渠道 | 服务商 | 常量 | 说明 |
|------|--------|------|------|
| `tempmail` | tempmail.ing | `ChannelTempmail` | 支持自定义有效期 |
| `tempmail-cn` | tempmail.cn | `ChannelTempmailCn` | Socket.IO：`request shortid` / `set shortid` / `mail`；`Domain` 可指定 `tempmail.cn` 或自定义接入域名 |
| `ta-easy` | ta-easy.com | `ChannelTaEasy` | REST：`api-endpoint.ta-easy.com`；需 Token；`ExpiresAt` 毫秒时间戳 |
| `10minute-one` | 10minutemail.one | `Channel10minuteOne` | SSR / JWT + Web API；`Domain` 可选（见实现） |
| `xghff-com` | xghff.com | `ChannelXghffCom` | 10minutemail.one 固定域名 `xghff.com` |
| `oqqaj-com` | oqqaj.com | `ChannelOqqajCom` | 10minutemail.one 固定域名 `oqqaj.com` |
| `psovv-com` | psovv.com | `ChannelPsovvCom` | 10minutemail.one 固定域名 `psovv.com` |
| `dbwot-com` | dbwot.com | `ChannelDbwotCom` | 10minutemail.one 固定域名 `dbwot.com` |
| `ygwpr-com` | ygwpr.com | `ChannelYgwprCom` | 10minutemail.one 固定域名 `ygwpr.com` |
| `imxwe-com` | imxwe.com | `ChannelImxweCom` | 10minutemail.one 固定域名 `imxwe.com` |
| `linshiyou` | linshiyou.com | `ChannelLinshiyou` | `NEXUS_TOKEN` + Cookie；HTML 分段解析 |
| `mffac` | mffac.com | `ChannelMffac` | REST mailbox `id` 作 token；`/api/emails/{id}` 详情补齐 `text` / `html`；24h |
| `tempmail-lol` | tempmail.lol | `ChannelTempmailLol` | 支持指定域名 |
| `chatgpt-org-uk` | mail.chatgpt.org.uk | `ChannelChatgptOrgUk` | Inbox Token 等请求头已封装 |
| `temp-mail-io` | temp-mail.io | `ChannelTempMailIo` | REST：动态读取 `mobileTestingHeader` 后调用 `api.internal.temp-mail.io/api/v3` |
| `mail-cx` | mail.cx | `ChannelMailCx` | 匿名 Web API：`/v1/config` 获取系统域名，`/v1/inbox/{email}` 长轮询收信，内部保存 `X-Client-ID` |
| `ddker-com` | ddker.com | `ChannelDdkerCom` | mail.cx 固定域名 `ddker.com` |
| `catchmail` | catchmail.io | `ChannelCatchmail` | 公开 REST：`/api/v1/mailbox?address=` + `/api/v1/message/{id}?mailbox=`；详情含 `body.text` / `body.html` |
| `catchmail-mailistry` | mailistry.com | `ChannelCatchmailMailistry` | Catchmail API 固定域名 `mailistry.com` |
| `catchmail-zeppost` | zeppost.com | `ChannelCatchmailZeppost` | Catchmail API 固定域名 `zeppost.com` |
| `mailforspam` | mailforspam.com | `ChannelMailforspam` | 公开 REST：`/api/mailboxes/{email}/emails` + `/api/mailboxes/{email}/emails/{id}`；详情含 `body_text` / `body_html` |
| `mailforspam-tempmail-io` | tempmail.io | `ChannelMailforspamTempmailIo` | MailForSpam API 固定域名 `tempmail.io` |
| `mailforspam-disposable` | disposable.email | `ChannelMailforspamDisposable` | MailForSpam API 固定域名 `disposable.email` |
| `tempmailo` | tempmailo.com | `ChannelTempmailo` | `GET /changemail` 建址，`POST /` 传 `mail` 拉信；返回对象直接含 `text` / `html` |
| `tempmailc` | tempmailc.com | `ChannelTempmailc` | Public API：`GET /api/v1/new` 建址，`GET /api/v1/inbox` 拉列表，`GET /api/v1/message` 读取 `text` / `html` 正文 |
| `mailnesia` | mailnesia.com | `ChannelMailnesia` | 任意 `{local}@mailnesia.com` 建址；HTML 列表 `tr.emailheader` + 详情 `text_plain_{id}` / `text_html_{id}` 正文 |
| `throwawaymail` | throwawaymail.app | `ChannelThrowawaymail` | Web API 建址并轮询收信；Token 由 SDK 内部维护 |
| `shitty-email` | shitty.email | `ChannelShittyEmail` | `POST /api/inbox` 建址；`X-Session-Token` + `GET /api/inbox` 拉列表，`GET /api/email/{id}` 读取 `text` / `html` 正文 |
| `tempmailpro` | tempmailpro.us | `ChannelTempmailpro` | `POST /api/v1/mailbox/create` 建箱；`GET /api/v1/mailbox/{token}/emails` 拉列表，详情 `body_text` / `body_html` 映射统一正文 |
| `devmail-uk` | devmail.uk | `ChannelDevmailUk` | `GET /api/new` 建址；`GET /api/inbox/{mailbox}?detail=true` 拉列表；生成接口返回的 `email` / `mailbox` 字段均兼容解析 |
| `cleantempmail` | cleantempmail.com | `ChannelCleanTempMail` | `GET /api/generate-email` 建址；`GET /api/emails?email=` 拉列表；公开 API 通过 `X-API-Key` 头访问 |
| `inboxkitten` | inboxkitten.com | `ChannelInboxkitten` | 公开 API 拉取收件箱列表与详情 |
| `getnada` | getnada.net | `ChannelGetnada` | `POST /api/inbox/open` 建箱；`GET /api/inbox/messages` 列表；`GET /api/inbox/message` 详情含 `text_plain` / `html_sanitized` |
| `1vpn-net` | 1vpn.net | `ChannelOneVpnNet` | getnada 固定域名 `1vpn.net` |
| `abematv-com` | abematv.com | `ChannelAbematvCom` | getnada 固定域名 `abematv.com` |
| `abematv-net` | abematv.net | `ChannelAbematvNet` | getnada 固定域名 `abematv.net` |
| `abematv-org` | abematv.org | `ChannelAbematvOrg` | getnada 固定域名 `abematv.org` |
| `aceh-cc` | aceh.cc | `ChannelAcehCc` | getnada 固定域名 `aceh.cc` |
| `bangkabelitung-net` | bangkabelitung.net | `ChannelBangkabelitungNet` | getnada 固定域名 `bangkabelitung.net` |
| `cctruyen-com` | cctruyen.com | `ChannelCctruyenCom` | getnada 固定域名 `cctruyen.com` |
| `getnada-com` | getnada.com | `ChannelGetnadaCom` | getnada 固定域名 `getnada.com` |
| `getnada-email` | getnada.email | `ChannelGetnadaEmail` | getnada 固定域名 `getnada.email` |
| `getnada-net` | getnada.net | `ChannelGetnadaNet` | getnada 固定域名 `getnada.net` |
| `jawatengah-net` | jawatengah.net | `ChannelJawatengahNet` | getnada 固定域名 `jawatengah.net` |
| `jawatimur-net` | jawatimur.net | `ChannelJawatimurNet` | getnada 固定域名 `jawatimur.net` |
| `kalimantanbarat-net` | kalimantanbarat.net | `ChannelKalimantanbaratNet` | getnada 固定域名 `kalimantanbarat.net` |
| `kalimantanselatan-net` | kalimantanselatan.net | `ChannelKalimantanselatanNet` | getnada 固定域名 `kalimantanselatan.net` |
| `kalimantantengah-net` | kalimantantengah.net | `ChannelKalimantantengahNet` | getnada 固定域名 `kalimantantengah.net` |
| `kalimantantimur-net` | kalimantantimur.net | `ChannelKalimantantimurNet` | getnada 固定域名 `kalimantantimur.net` |
| `kalimantanutara-net` | kalimantanutara.net | `ChannelKalimantanutaraNet` | getnada 固定域名 `kalimantanutara.net` |
| `kepulauanriau-net` | kepulauanriau.net | `ChannelKepulauanriauNet` | getnada 固定域名 `kepulauanriau.net` |
| `luxury345-com` | luxury345.com | `ChannelLuxury345Com` | getnada 固定域名 `luxury345.com` |
| `malukuutara-net` | malukuutara.net | `ChannelMalukuutaraNet` | getnada 固定域名 `malukuutara.net` |
| `nusatenggarabarat-net` | nusatenggarabarat.net | `ChannelNusatenggarabaratNet` | getnada 固定域名 `nusatenggarabarat.net` |
| `nusatenggaratimur-net` | nusatenggaratimur.net | `ChannelNusatenggaratimurNet` | getnada 固定域名 `nusatenggaratimur.net` |
| `papuabarat-net` | papuabarat.net | `ChannelPapuabaratNet` | getnada 固定域名 `papuabarat.net` |
| `papuabaratdaya-net` | papuabaratdaya.net | `ChannelPapuabaratdayaNet` | getnada 固定域名 `papuabaratdaya.net` |
| `papuaselatan-net` | papuaselatan.net | `ChannelPapuaselatanNet` | getnada 固定域名 `papuaselatan.net` |
| `pehol-com` | pehol.com | `ChannelPeholCom` | getnada 固定域名 `pehol.com` |
| `ptruyen-com` | ptruyen.com | `ChannelPtruyenCom` | getnada 固定域名 `ptruyen.com` |
| `pulaubali-net` | pulaubali.net | `ChannelPulaubaliNet` | getnada 固定域名 `pulaubali.net` |
| `riau-net` | riau.net | `ChannelRiauNet` | getnada 固定域名 `riau.net` |
| `seokey-org` | seokey.org | `ChannelSeokeyOrg` | getnada 固定域名 `seokey.org` |
| `sulawesibarat-net` | sulawesibarat.net | `ChannelSulawesibaratNet` | getnada 固定域名 `sulawesibarat.net` |
| `sulawesiselatan-net` | sulawesiselatan.net | `ChannelSulawesiselatanNet` | getnada 固定域名 `sulawesiselatan.net` |
| `sulawesitengah-net` | sulawesitengah.net | `ChannelSulawesitengahNet` | getnada 固定域名 `sulawesitengah.net` |
| `sulawesitenggara-net` | sulawesitenggara.net | `ChannelSulawesitenggaraNet` | getnada 固定域名 `sulawesitenggara.net` |
| `sumaterabarat-net` | sumaterabarat.net | `ChannelSumaterabaratNet` | getnada 固定域名 `sumaterabarat.net` |
| `sumateraselatan-net` | sumateraselatan.net | `ChannelSumateraselatanNet` | getnada 固定域名 `sumateraselatan.net` |
| `sumaterautara-net` | sumaterautara.net | `ChannelSumaterautaraNet` | getnada 固定域名 `sumaterautara.net` |
| `villatogel-com` | villatogel.com | `ChannelVillatogelCom` | getnada 固定域名 `villatogel.com` |
| `mail123` | mail123.fr | `ChannelMail123` | `GET /api/v1/mailbox/new` 建址；`GET /api/v1/mailbox/{address}/messages?limit=50` 列表；详情含 `text` / `html` |
| `mail10s` | mail10s.com | `ChannelMail10s` | 任意随机地址 + 公开 inbox API；`sender` / `body_text` / `body_html` 映射统一正文 |
| `webmailtemp` | webmailtemp.com | `ChannelWebmailtemp` | `GET /api/create` 建箱；会话 Cookie + `GET /api/check/{username}` 收信；30 分钟 TTL |
| `tempfastmail` | tempfastmail.com | `ChannelTempfastmail` | `POST /api/email-box` 建箱；邮箱 UUID 拉列表和详情；详情 `html` 由 normalize 派生 `text` |
| `1sec-mail` | tmaily.com | `ChannelOneSecMail` | `GET /generate` 取地址；从 `Set-Cookie` 提取 `TMaily_sid`；`GET /emails?address=` 拉列表，详情由 `body_text` / `html` 映射，缺失正文由 normalize 反向生成 |
| `fakemail` | fakemail.net | `ChannelFakemail` | `/index/index` 建址，`/index/refresh` 拉列表，`/index/email` 详情；`telo` HTML 正文 |
| `openinbox` | openinbox.io | `ChannelOpeninbox` | `POST /api/inbox` 建箱；`GET /emails/inbox/{id}` 列表；`GET /emails/{emailId}` 详情含 `textBody` / `htmlBody` |
| `inboxes` | inboxes.com | `ChannelInboxes` | 公开 v2：`GET /api/v2/domain` 域名，`GET /api/v2/inbox/{email}` 列表，`GET /api/v2/message/{uid}` 详情含 `text` / `html` |
| `uncorreotemporal` | uncorreotemporal.com | `ChannelUncorreotemporal` | `POST /api/v1/mailboxes` 建箱；`X-Session-Token` 拉取列表和详情；详情含 `body_text` / `body_html` |
| `awamail` | awamail.com | `ChannelAwamail` | Session Cookie 自动管理 |
| `mail-tm` | mail.tm | `ChannelMailTm` | 自动注册（`api.mail.tm`），Bearer Token |
| `web-library-net` | web-library.net | `ChannelWebLibraryNet` | mail.tm 固定域名 `web-library.net` |
| `dropmail` | dropmail.me | `ChannelDropmail` | GraphQL，Session ID |
| `guerrillamail` | guerrillamail.com | `ChannelGuerrillaMail` | 公开 JSON API |
| `guerrillamail-com` | guerrillamail.com | `ChannelGuerrillamailCom` | GuerrillaMail 裸域 JSON API 入口 |
| `maildrop` | maildrop.cx | `ChannelMaildrop` | REST：`suffixes.php`（排除 `transformer.edu.kg`）+ `emails.php`；`description`→`text` |
| `smail-pw` | smail.pw | `ChannelSmailPw` | `_root.data` + `__session`；RSC/Flight 解析 |
| `vip-215` | vip.215.im | `ChannelVip215` | `POST` 建箱 + WebSocket；无正文时 synthetic 兜底 |
| `fake-legal` | fake.legal | `ChannelFakeLegal` | `/api/domains` + `/api/inbox/new`；可选域名 |
| `imgui-de` | imgui.de | `ChannelImguiDe` | fake.legal 固定域名 `imgui.de` |
| `pulsewebmenu-de` | pulsewebmenu.de | `ChannelPulsewebmenuDe` | fake.legal 固定域名 `pulsewebmenu.de` |
| `moakt` | moakt.com | `ChannelMoakt` | `GET /{locale}` → `/inbox` 解析 `#email-address`；Token 为 `mok1:` + Base64 JSON（`locale` + Cookie，须含 `tm_session`）；收信逐封 `GET .../email/{uuid}/html`；`HTTPClientNoCookieJar`；`Domain` 可选语言路径 |
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
| `email10min` | email10min.com | `ChannelEmail10min` | CSRF + Cookie；`POST /messages` 获取邮箱与邮件 |
| `mjj-cm` | mjj.cm | `ChannelMjjCm` | Socket.IO：`request shortid` / `set shortid` / `mail` |
| `linshi-co` | linshi.co | `ChannelLinshiCo` | Socket.IO 克隆站，协议同 `mjj-cm` |
| `harakirimail` | harakirimail.com | `ChannelHarakirimail` | 公开 REST：`GET /api/v1/inbox/{name}` + `GET /api/v1/email/{id}` |
| `jqkjqk-xyz` | jqkjqk.xyz | `ChannelJqkjqkXyz` | mail.zhujump.com 固定域名 `jqkjqk.xyz` |
| `lyhlevi-com` | lyhlevi.com | `ChannelLyhleviCom` | MoeMail 部署实例；注册登录后创建邮箱，列表/详情映射统一正文 |
| `tempmail-plus` | tempmail.plus | `ChannelTempmailPlus` | 公开 REST：`GET /api/mails/?email=` 列表，`GET /api/mails/{id}?email=` 详情 |
| `fexpost-com` | fexpost.com | `ChannelFexpostCom` | TempMail Plus 固定域名 `fexpost.com` |
| `fexbox-org` | fexbox.org | `ChannelFexboxOrg` | TempMail Plus 固定域名 `fexbox.org` |
| `mailbox-in-ua` | mailbox.in.ua | `ChannelMailboxInUa` | TempMail Plus 固定域名 `mailbox.in.ua` |
| `rover-info` | rover.info | `ChannelRoverInfo` | TempMail Plus 固定域名 `rover.info` |
| `chitthi-in` | chitthi.in | `ChannelChitthiIn` | TempMail Plus 固定域名 `chitthi.in` |
| `fextemp-com` | fextemp.com | `ChannelFextempCom` | TempMail Plus 固定域名 `fextemp.com` |
| `any-pink` | any.pink | `ChannelAnyPink` | TempMail Plus 固定域名 `any.pink` |
| `merepost-com` | merepost.com | `ChannelMerepostCom` | TempMail Plus 固定域名 `merepost.com` |
| `tempmail-lol-v2` | tempmail.lol | `ChannelTempmailLolV2` | `GET /generate` 返回 address+token，`GET /auth/{token}` 拉取收件箱 |
| `tempgbox` | tempgbox.net | `ChannelTempgbox` | `POST /api/proxy?route=generate` 使用 `variant=googlemail`；每次生成随机 `X-Device-ID` 并随请求带随机来源 IP 头 |
| `emailnator` | emailnator.com | `ChannelEmailnator` | XSRF + Cookie；Gmail/GoogleMail alias 选项生成，`messageID` 读取 HTML 正文 |
| `temporam` | temporam.com | `ChannelTemporam` | 公开 REST：`/api/domains`、`/api/emails?email=`、`/api/emails/{id}` |
| `neighbours` | neighbours.sh | `ChannelNeighbours` | `GET /config/domains` 获取域名；`GET /inbox/{address}` / `GET /inbox/{address}/{uid}` 拉信；`404` 视为空收件箱 |
| `fake-email-site` | fake-email.site | `ChannelFakeEmailSite` | `POST /api/temporary-address` 建箱；`GET /api/inbox/poll?address=` 拉信；返回 `temp_email_addr` / `messages` |
| `sharklasers` | sharklasers.com | `ChannelSharklasers` | GuerrillaMail 镜像，API 与 `guerrillamail` 相同 |
| `sharklasers-com` | sharklasers.com | `ChannelSharklasersCom` | GuerrillaMail 裸域镜像，API 与 `guerrillamail` 相同 |
| `grr-la` | grr.la | `ChannelGrrLa` | GuerrillaMail 镜像，API 与 `guerrillamail` 相同 |
| `grr-la-com` | grr.la | `ChannelGrrLaCom` | GuerrillaMail 裸域镜像，API 与 `guerrillamail` 相同 |
| `guerrillamail-info` | guerrillamail.info | `ChannelGuerrillamailInfo` | GuerrillaMail 镜像，API 与 `guerrillamail` 相同 |
| `spam4me` | spam4.me | `ChannelSpam4me` | GuerrillaMail 镜像，API 与 `guerrillamail` 相同 |
| `guerrillamail-net` | guerrillamail.net | `ChannelGuerrillamailNet` | GuerrillaMail 镜像，API 与 `guerrillamail` 相同 |
| `guerrillamail-org` | guerrillamail.org | `ChannelGuerrillamailOrg` | GuerrillaMail 镜像，API 与 `guerrillamail` 相同 |
| `guerrillamailblock` | guerrillamailblock.com | `ChannelGuerrillamailBlock` | GuerrillaMail 镜像，API 与 `guerrillamail` 相同 |
| `guerrillamail-com-www` | guerrillamail.com | `ChannelGuerrillamailComWww` | GuerrillaMail `www` JSON API 入口 |
| `m2u` | MailToYou | `ChannelM2u` | `POST /v1/mailboxes/auto` 建箱；`token` + `viewToken` 组成内部 Token；`GET /v1/mailboxes/{token}/messages?view={viewToken}` 拉列表，`GET /v1/mailboxes/{token}/messages/{id}?view={viewToken}` 读取详情 |
| `tempy-email` | Tempy Email | `ChannelTempyEmail` | `POST /api/v1/mailbox` 建箱（请求体可为空 JSON；可选 `domain`）；`GET /api/v1/mailbox/{email}/messages` 拉列表 |
| `fmail` | fmail.men | `ChannelFmail` | `GET /v1/random` 建箱；`GET /v1/inbox/{local}?domain={domain}&limit=50` 拉列表；`GET /v1/email/{token}` 读取单封详情，可选 `Domain` |
| `ockito` | ockito.com | `ChannelOckito` | `POST /gtoken` 换取 access_token / refresh_token；`GET /email` 建箱；`GET /retrieve/{email}/emails` 拉列表，`GET /retrieve/{email}/{uid}` 详情 |
| `anonbox` | anonbox.net | `ChannelAnonbox` | `GET /en/` 解析页面建箱；`GET /{token}/` 拉信，token 形如 `inbox/secret`，mbox 明文解析 |
| `duckmail` | duckmail.sbs | `ChannelDuckmail` | `GET /domains?page=1` 取域名；`POST /accounts` 创建账号；`POST /token` 获取 Bearer Token；`GET /messages` 拉列表 |
| `mailinator` | mailinator.com | `ChannelMailinator` | 公开 REST：`GET /api/v2/domains/public/inboxes/{inbox}` 拉列表；`GET /api/v2/domains/public/messages/{id}/{text|texthtml|attachments}` 详情 |

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
    Channel: tempemail.ChannelMailGw,
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

**配置项（`SDKConfig`）：**

| 字段 | 类型 | 说明 |
|------|------|------|
| `Proxy` | `string` | 代理 URL（http/https/socks5） |
| `Timeout` | `time.Duration` | 全局超时，默认 15s |
| `Insecure` | `bool` | 跳过 SSL 验证（调试用） |
| `DropmailAuthToken` | `string` | DropMail GraphQL 路径中的 `af_` 令牌；空则自动申请 |
| `DropmailDisableAutoToken` | `bool` | 为 true 时不自动拉取/续期令牌 |
| `DropmailRenewLifetime` | `string` | 续期请求的 lifetime，如 `1d` |
| `TelemetryEnabled` | `*bool` | `nil` 默认开启匿名遥测；指向 `false` 关闭 |
| `TelemetryEndpoint` | `string` | 非空时作为上报 URL，覆盖环境变量与内置默认 |

**环境变量（无需修改代码）：**

```bash
export TEMPMAIL_PROXY="http://127.0.0.1:7890"
export TEMPMAIL_INSECURE=1
export TEMPMAIL_TIMEOUT=30
export DROPMAIL_AUTH_TOKEN="af_..."   # 或 DROPMAIL_API_TOKEN
export DROPMAIL_NO_AUTO_TOKEN=1
export DROPMAIL_RENEW_LIFETIME=1d
export TEMPMAIL_TELEMETRY_ENABLED=false
export TEMPMAIL_TELEMETRY_URL="https://example.com/v1/event"
```

## 匿名遥测

默认 **开启**：将 `generate_email` / `get_emails` 等操作的成败与重试信息**批量** `POST` 到上报端点（`schema_version: 2`），内置默认 URL 见 `telemetry.go`（一般为 `https://sdk-1.openel.top/v1/event`）。错误串中的邮箱形态会脱敏。关闭：环境变量 `TEMPMAIL_TELEMETRY_ENABLED=false`（或 `0` / `no`），或代码中 `off := false; SetConfig(SDKConfig{TelemetryEnabled: &off})`；改 URL：`TEMPMAIL_TELEMETRY_URL` 或 `TelemetryEndpoint`。

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
| `Domain` | `*string` | 指定域名或接入参数（`tempmail-cn`、`tempmail-lol`、`maildrop`、`fake-legal`、`catchmail` / `mailforspam` 固定域名、`moakt` 语言路径、`10minute-one` 站点参数等） |
| `Retry` | `*RetryOptions` | 创建邮箱时的 HTTP 重试，nil 使用默认 |

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
