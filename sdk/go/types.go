package tempemail

/*
 * Channel 支持的临时邮箱渠道标识
 * 每个渠道对应一个第三方临时邮箱服务商。
 */
type Channel string

const (
	ChannelTempmail                Channel = "tempmail"                 // tempmail.ing
	ChannelTempmailCn              Channel = "tempmail-cn"              // tempmail.cn
	ChannelTaEasy                  Channel = "ta-easy"                  // ta-easy.com
	Channel10minuteOne             Channel = "10minute-one"             // 10minutemail.one（SSR JWT + web API）
	ChannelXghffCom                Channel = "xghff-com"                // 10minutemail.one 固定域 xghff.com
	ChannelOqqajCom                Channel = "oqqaj-com"                // 10minutemail.one 固定域 oqqaj.com
	ChannelPsovvCom                Channel = "psovv-com"                // 10minutemail.one 固定域 psovv.com
	ChannelDbwotCom                Channel = "dbwot-com"                // 10minutemail.one 固定域 dbwot.com
	ChannelYgwprCom                Channel = "ygwpr-com"                // 10minutemail.one 固定域 ygwpr.com
	ChannelImxweCom                Channel = "imxwe-com"                // 10minutemail.one 固定域 imxwe.com
	ChannelLinshiyou               Channel = "linshiyou"                // linshiyou.com
	ChannelMffac                   Channel = "mffac"                    // mffac.com
	ChannelTempmailLol             Channel = "tempmail-lol"             // tempmail.lol
	ChannelChatgptOrgUk            Channel = "chatgpt-org-uk"           // mail.chatgpt.org.uk
	ChannelTempMailIo              Channel = "temp-mail-io"             // temp-mail.io
	ChannelMailCx                  Channel = "mail-cx"                  // mail.cx
	ChannelDdkerCom                Channel = "ddker-com"                // mail.cx 固定域 ddker.com
	ChannelCatchmail               Channel = "catchmail"                // catchmail.io
	ChannelCatchmailMailistry      Channel = "catchmail-mailistry"      // mailistry.com（Catchmail API 域名）
	ChannelCatchmailZeppost        Channel = "catchmail-zeppost"        // zeppost.com（Catchmail API 域名）
	ChannelMailforspam             Channel = "mailforspam"              // mailforspam.com
	ChannelMailforspamTempmailIo   Channel = "mailforspam-tempmail-io"  // tempmail.io（MailForSpam API 域名）
	ChannelMailforspamDisposable   Channel = "mailforspam-disposable"   // disposable.email（MailForSpam API 域名）
	ChannelTempmailc               Channel = "tempmailc"                // tempmailc.com
	ChannelMailnesia               Channel = "mailnesia"                // mailnesia.com
	ChannelThrowawaymail           Channel = "throwawaymail"            // throwawaymail.app
	ChannelTempmailFish            Channel = "tempmail-fish"            // tempmail.fish
	ChannelNeighboursSh            Channel = "neighbours-sh"            // neighbours.sh
	ChannelShittyEmail             Channel = "shitty-email"             // shitty.email
	ChannelTempmailpro             Channel = "tempmailpro"              // tempmailpro.us
	ChannelDevmailUk               Channel = "devmail-uk"               // devmail.uk
	ChannelCleanTempMail           Channel = "cleantempmail"            // cleantempmail.com
	ChannelInboxkitten             Channel = "inboxkitten"              // inboxkitten.com
	ChannelGetnada                 Channel = "getnada"                  // getnada.net
	ChannelOneVpnNet               Channel = "1vpn-net"                 // getnada 固定域 1vpn.net
	ChannelAbematvCom              Channel = "abematv-com"              // getnada 固定域 abematv.com
	ChannelAbematvNet              Channel = "abematv-net"              // getnada 固定域 abematv.net
	ChannelAbematvOrg              Channel = "abematv-org"              // getnada 固定域 abematv.org
	ChannelAcehCc                  Channel = "aceh-cc"                  // getnada 固定域 aceh.cc
	ChannelBangkabelitungNet       Channel = "bangkabelitung-net"       // getnada 固定域 bangkabelitung.net
	ChannelCctruyenCom             Channel = "cctruyen-com"             // getnada 固定域 cctruyen.com
	ChannelGetnadaCom              Channel = "getnada-com"              // getnada 固定域 getnada.com
	ChannelGetnadaEmail            Channel = "getnada-email"            // getnada 固定域 getnada.email
	ChannelGetnadaNet              Channel = "getnada-net"              // getnada 固定域 getnada.net
	ChannelJawatengahNet           Channel = "jawatengah-net"           // getnada 固定域 jawatengah.net
	ChannelJawatimurNet            Channel = "jawatimur-net"            // getnada 固定域 jawatimur.net
	ChannelKalimantanbaratNet      Channel = "kalimantanbarat-net"      // getnada 固定域 kalimantanbarat.net
	ChannelKalimantanselatanNet    Channel = "kalimantanselatan-net"    // getnada 固定域 kalimantanselatan.net
	ChannelKalimantantengahNet     Channel = "kalimantantengah-net"     // getnada 固定域 kalimantantengah.net
	ChannelKalimantantimurNet      Channel = "kalimantantimur-net"      // getnada 固定域 kalimantantimur.net
	ChannelKalimantanutaraNet      Channel = "kalimantanutara-net"      // getnada 固定域 kalimantanutara.net
	ChannelKepulauanriauNet        Channel = "kepulauanriau-net"        // getnada 固定域 kepulauanriau.net
	ChannelLuxury345Com            Channel = "luxury345-com"            // getnada 固定域 luxury345.com
	ChannelMalukuutaraNet          Channel = "malukuutara-net"          // getnada 固定域 malukuutara.net
	ChannelNusatenggarabaratNet    Channel = "nusatenggarabarat-net"    // getnada 固定域 nusatenggarabarat.net
	ChannelNusatenggaratimurNet    Channel = "nusatenggaratimur-net"    // getnada 固定域 nusatenggaratimur.net
	ChannelPapuabaratNet           Channel = "papuabarat-net"           // getnada 固定域 papuabarat.net
	ChannelPapuabaratdayaNet       Channel = "papuabaratdaya-net"       // getnada 固定域 papuabaratdaya.net
	ChannelPapuaselatanNet         Channel = "papuaselatan-net"         // getnada 固定域 papuaselatan.net
	ChannelPeholCom                Channel = "pehol-com"                // getnada 固定域 pehol.com
	ChannelPtruyenCom              Channel = "ptruyen-com"              // getnada 固定域 ptruyen.com
	ChannelPulaubaliNet            Channel = "pulaubali-net"            // getnada 固定域 pulaubali.net
	ChannelRiauNet                 Channel = "riau-net"                 // getnada 固定域 riau.net
	ChannelSeokeyOrg               Channel = "seokey-org"               // getnada 固定域 seokey.org
	ChannelSulawesibaratNet        Channel = "sulawesibarat-net"        // getnada 固定域 sulawesibarat.net
	ChannelSulawesiselatanNet      Channel = "sulawesiselatan-net"      // getnada 固定域 sulawesiselatan.net
	ChannelSulawesitengahNet       Channel = "sulawesitengah-net"       // getnada 固定域 sulawesitengah.net
	ChannelSulawesitenggaraNet     Channel = "sulawesitenggara-net"     // getnada 固定域 sulawesitenggara.net
	ChannelSumaterabaratNet        Channel = "sumaterabarat-net"        // getnada 固定域 sumaterabarat.net
	ChannelSumateraselatanNet      Channel = "sumateraselatan-net"      // getnada 固定域 sumateraselatan.net
	ChannelSumaterautaraNet        Channel = "sumaterautara-net"        // getnada 固定域 sumaterautara.net
	ChannelVillatogelCom           Channel = "villatogel-com"           // getnada 固定域 villatogel.com
	ChannelMail123                 Channel = "mail123"                  // mail123.fr
	ChannelMail10s                 Channel = "mail10s"                  // mail10s.com
	ChannelWebmailtemp             Channel = "webmailtemp"              // webmailtemp.com
	ChannelTempfastmail            Channel = "tempfastmail"             // tempfastmail.com
	ChannelOneSecMail              Channel = "1sec-mail"                // 1sec-mail.com
	ChannelFakemail                Channel = "fakemail"                 // fakemail.net
	ChannelOpeninbox               Channel = "openinbox"                // openinbox.io
	ChannelInboxes                 Channel = "inboxes"                  // inboxes.com
	ChannelUncorreotemporal        Channel = "uncorreotemporal"         // uncorreotemporal.com
	ChannelAwamail                 Channel = "awamail"                  // awamail.com
	ChannelMailTm                  Channel = "mail-tm"                  // mail.tm
	ChannelWebLibraryNet           Channel = "web-library-net"          // mail.tm 固定域 web-library.net
	ChannelDropmail                Channel = "dropmail"                 // dropmail.me
	ChannelGuerrillaMail           Channel = "guerrillamail"            // guerrillamail.com
	ChannelGuerrillamailCom        Channel = "guerrillamail-com"        // guerrillamail.com 裸域 JSON API
	ChannelMaildrop                Channel = "maildrop"                 // maildrop.cx
	ChannelSmailPw                 Channel = "smail-pw"                 // smail.pw
	ChannelVip215                  Channel = "vip-215"                  // vip.215.im
	ChannelFakeLegal               Channel = "fake-legal"               // fake.legal
	ChannelImguiDe                 Channel = "imgui-de"                 // fake.legal 固定域 imgui.de
	ChannelPulsewebmenuDe          Channel = "pulsewebmenu-de"          // fake.legal 固定域 pulsewebmenu.de
	ChannelMoakt                   Channel = "moakt"                    // moakt.com（HTML 收件箱 + tm_session Cookie）
	ChannelDrmailIn                Channel = "drmail-in"                // moakt 固定域 drmail.in
	ChannelTemlNet                 Channel = "teml-net"                 // moakt 固定域 teml.net
	ChannelTmpemlCom               Channel = "tmpeml-com"               // moakt 固定域 tmpeml.com
	ChannelTmpboxNet               Channel = "tmpbox-net"               // moakt 固定域 tmpbox.net
	ChannelMoaktCc                 Channel = "moakt-cc"                 // moakt 固定域 moakt.cc
	ChannelDisboxNet               Channel = "disbox-net"               // moakt 固定域 disbox.net
	ChannelTmpmailOrg              Channel = "tmpmail-org"              // moakt 固定域 tmpmail.org
	ChannelTmpmailNet              Channel = "tmpmail-net"              // moakt 固定域 tmpmail.net
	ChannelTmailsNet               Channel = "tmails-net"               // moakt 固定域 tmails.net
	ChannelDisboxOrg               Channel = "disbox-org"               // moakt 固定域 disbox.org
	ChannelMoaktCo                 Channel = "moakt-co"                 // moakt 固定域 moakt.co
	ChannelMoaktWs                 Channel = "moakt-ws"                 // moakt 固定域 moakt.ws
	ChannelTmailWs                 Channel = "tmail-ws"                 // moakt 固定域 tmail.ws
	ChannelBareedWs                Channel = "bareed-ws"                // moakt 固定域 bareed.ws
	ChannelEmail10min              Channel = "email10min"               // email10min.com（CSRF + Cookie）
	ChannelMjjCm                   Channel = "mjj-cm"                   // mjj.cm（Socket.IO）
	ChannelLinshiCo                Channel = "linshi-co"                // linshi.co（Socket.IO）
	ChannelHarakirimail            Channel = "harakirimail"             // harakirimail.com
	ChannelJqkjqkXyz               Channel = "jqkjqk-xyz"               // mail.zhujump.com 固定域 jqkjqk.xyz
	ChannelLyhleviCom              Channel = "lyhlevi-com"              // lyhlevi.com（MoeMail 部署实例）
	ChannelTempmailPlus            Channel = "tempmail-plus"            // tempmail.plus
	ChannelFexpostCom              Channel = "fexpost-com"              // tempmail.plus 固定域 fexpost.com
	ChannelFexboxOrg               Channel = "fexbox-org"               // tempmail.plus 固定域 fexbox.org
	ChannelMailboxInUa             Channel = "mailbox-in-ua"            // tempmail.plus 固定域 mailbox.in.ua
	ChannelRoverInfo               Channel = "rover-info"               // tempmail.plus 固定域 rover.info
	ChannelChitthiIn               Channel = "chitthi-in"               // tempmail.plus 固定域 chitthi.in
	ChannelFextempCom              Channel = "fextemp-com"              // tempmail.plus 固定域 fextemp.com
	ChannelAnyPink                 Channel = "any-pink"                 // tempmail.plus 固定域 any.pink
	ChannelMerepostCom             Channel = "merepost-com"             // tempmail.plus 固定域 merepost.com
	ChannelTempmailLolV2           Channel = "tempmail-lol-v2"          // tempmail.lol（V2 REST API）
	ChannelTempgbox                Channel = "tempgbox"                 // tempgbox.net（Googlemail alias HTTP API）
	ChannelEmailnator              Channel = "emailnator"               // emailnator.com（Gmail alias）
	ChannelTemporam                Channel = "temporam"                 // temporam.com
	ChannelNeighbours              Channel = "neighbours"               // neighbours.sh
	ChannelSharklasers             Channel = "sharklasers"              // sharklasers.com（Guerrillamail 镜像）
	ChannelSharklasersCom          Channel = "sharklasers-com"          // sharklasers.com 裸域镜像
	ChannelGrrLa                   Channel = "grr-la"                   // grr.la（Guerrillamail 镜像）
	ChannelGrrLaCom                Channel = "grr-la-com"               // grr.la 裸域镜像
	ChannelGuerrillamailInfo       Channel = "guerrillamail-info"       // guerrillamail.info（Guerrillamail 镜像）
	ChannelSpam4me                 Channel = "spam4me"                  // spam4.me（Guerrillamail 镜像）
	ChannelGuerrillamailNet        Channel = "guerrillamail-net"        // guerrillamail.net（Guerrillamail 镜像）
	ChannelGuerrillamailOrg        Channel = "guerrillamail-org"        // guerrillamail.org（Guerrillamail 镜像）
	ChannelGuerrillamailBlock      Channel = "guerrillamailblock"       // guerrillamailblock.com（Guerrillamail 镜像）
	ChannelGuerrillamailComWww     Channel = "guerrillamail-com-www"    // guerrillamail.com（Guerrillamail 镜像）
	ChannelM2u                     Channel = "m2u"                      // m2u.io（MailToYou）
	ChannelTempyEmail              Channel = "tempy-email"              // tempy.email
	ChannelFmail                   Channel = "fmail"                    // fmail.men
	ChannelOckito                  Channel = "ockito"                   // ockito.com
	ChannelAnonbox                 Channel = "anonbox"                  // anonbox.net
	ChannelMailinator              Channel = "mailinator"               // mailinator.com
	ChannelDuckmail                Channel = "duckmail"                 // duckmail.sbs
	ChannelTempmail365             Channel = "tempmail365"              // tempmail365.cn
	ChannelTempinbox               Channel = "tempinbox"                // tempinbox.xyz
	ChannelByom                    Channel = "byom"                     // byom.de
	ChannelAnonymmail              Channel = "anonymmail"               // anonymmail.net
	ChannelEyepaste                Channel = "eyepaste"                 // eyepaste.com
	ChannelMailSunls               Channel = "mail-sunls"               // mail.sunls.de
	ChannelExpressinboxhub         Channel = "expressinboxhub"          // expressinboxhub.com
	ChannelLroid                   Channel = "lroid"                    // lroid.com（HTML 页面解析，域名 yevme.com）
	ChannelHaribu                  Channel = "haribu"                   // haribu.net（Tempail 类模式，域名 yevme.com）
	ChannelRootsh                  Channel = "rootsh"                   // rootsh.com（BccTo.CC）
	ChannelFakeEmailSite           Channel = "fake-email-site"          // fake-email.site
	ChannelMohmal                  Channel = "mohmal"                   // mohmal.com（HTML 收件箱 + connect.sid Session）
	ChannelMailgolem               Channel = "mailgolem"                // mailgolem.com（CSRF + Cookie Session）
	ChannelBestTempMail            Channel = "best-temp-mail"           // best-temp-mail.com（JSON REST API）
	ChannelDisposablemailApp       Channel = "disposablemail-app"       // disposablemail.app（纯 REST JSON API）
	ChannelMailtempCc              Channel = "mailtemp-cc"              // mailtemp.cc
	ChannelMinuteinbox             Channel = "minuteinbox"              // minuteinbox.com
	ChannelMailcatch               Channel = "mailcatch"                // mailcatch.com
	ChannelTempemailCo             Channel = "tempemail-co"             // tempemail.co
	ChannelTempemailsNet           Channel = "tempemails-net"           // tempemails.net
	ChannelAltmails                Channel = "altmails"                 // tempmail.altmails.com（CSRF + Cookie Session）
	ChannelTempemailInfo           Channel = "tempemail-info"           // tempemail.info
	ChannelSmailpro                Channel = "smailpro"                 // smailpro.com
	ChannelTempmailten             Channel = "tempmailten"              // tempmailten
	ChannelMaildropCc              Channel = "maildrop-cc"              // maildrop.cc
	ChannelTenminutemailNet        Channel = "10minutemail-net"         // 10minutemail.net
	ChannelLinshiyouxiangNet       Channel = "linshiyouxiang-net"       // linshiyouxiang.net
	ChannelTempMailFyi             Channel = "tempmail-fyi"             // temp-mail.fyi
	ChannelDisposablemailCom       Channel = "disposablemail-com"       // disposablemail.com（METRONET 后端）
	ChannelTemppMails              Channel = "tempp-mails"              // tempp-mails.com（Laravel 临时邮箱模板）
	ChannelEmailtempOrg            Channel = "emailtemp-org"            // emailtemp.org（Laravel 临时邮箱模板）
	ChannelMytempmailCc            Channel = "mytempmail-cc"            // mytempmail.cc（JSON REST API）
	ChannelTempMailNow             Channel = "temp-mail-now"            // temp-mail.now（Session Cookie API）
	ChannelMailTd                  Channel = "mail-td"                  // mail.td（SHA-256 PoW + JWT REST API）
	ChannelMailholeDe              Channel = "mailhole-de"              // mailhole.de（公共临时邮箱，无需认证）
	ChannelTmailLink               Channel = "tmail-link"               // tmail.link（Django CSRF + Cookie）
	Channel24mailChacuo            Channel = "24mail-chacuo"            // 24mail.chacuo.net（POST 注册/刷新）
	ChannelNimail                  Channel = "nimail"                   // nimail.cn（简单 POST API 临时邮箱）
	ChannelFreecustom              Channel = "freecustom"               // freecustom.email（免注册临时邮箱）
	ChannelApihz                   Channel = "apihz"                    // apihz.cn（接口盒子，需 id/key 凭据）
	ChannelMailmomy                Channel = "mailmomy"                 // mailmomy.com（免注册纯 GET JSON API）
	ChannelSogetthisCom            Channel = "sogetthis-com"            // mailinator 姊妹域名 sogetthis.com
	ChannelBobmailInfo             Channel = "bobmail-info"             // mailinator 姊妹域名 bobmail.info
	ChannelSuremailInfo            Channel = "suremail-info"            // mailinator 姊妹域名 suremail.info
	ChannelBinkmailCom             Channel = "binkmail-com"             // mailinator 姊妹域名 binkmail.com
	ChannelVeryrealemailCom        Channel = "veryrealemail-com"        // mailinator 姊妹域名 veryrealemail.com
	ChannelChammyInfo              Channel = "chammy-info"              // mailinator 姊妹域名 chammy.info
	ChannelThisisnotmyrealemailCom Channel = "thisisnotmyrealemail-com" // mailinator 姊妹域名 thisisnotmyrealemail.com
	ChannelNotmailinatorCom        Channel = "notmailinator-com"        // mailinator 姊妹域名 notmailinator.com
	ChannelSpamherepleaseCom       Channel = "spamhereplease-com"       // mailinator 姊妹域名 spamhereplease.com
	ChannelSendspamhereCom         Channel = "sendspamhere-com"         // mailinator 姊妹域名 sendspamhere.com
	ChannelSendfreeOrg             Channel = "sendfree-org"             // mailinator 姊妹域名 sendfree.org
	ChannelJunkBeatsOrg            Channel = "junk-beats-org"           // mailinator 姊妹子域名 junk.beats.org
	ChannelJunkIhmehlCom           Channel = "junk-ihmehl-com"          // mailinator 姊妹子域名 junk.ihmehl.com
	ChannelJunkNoplayOrg           Channel = "junk-noplay-org"          // mailinator 姊妹子域名 junk.noplay.org
	ChannelJunkVanillasystemCom    Channel = "junk-vanillasystem-com"   // mailinator 姊妹子域名 junk.vanillasystem.com
	ChannelSpamJasonpearceCom      Channel = "spam-jasonpearce-com"     // mailinator 姊妹子域名 spam.jasonpearce.com
	ChannelFishSkytaleNet          Channel = "fish-skytale-net"         // mailinator 姊妹子域名 fish.skytale.net
	ChannelSpamMccrewCom           Channel = "spam-mccrew-com"          // mailinator 姊妹子域名 spam.mccrew.com
	ChannelDropmailClick           Channel = "dropmail-click"           // dropmail.click（独立后端 REST API）
	ChannelSpamCoroiuCom           Channel = "spam-coroiu-com"          // mailinator 姊妹子域名 spam.coroiu.com
	ChannelSpamDeluserNet          Channel = "spam-deluser-net"         // mailinator 姊妹子域名 spam.deluser.net
	ChannelSpamDhsfNet             Channel = "spam-dhsf-net"            // mailinator 姊妹子域名 spam.dhsf.net
	ChannelSpamLucatntCom          Channel = "spam-lucatnt-com"         // mailinator 姊妹子域名 spam.lucatnt.com
	ChannelSpamLyceumLifeComRu     Channel = "spam-lyceum-life-com-ru"  // mailinator 姊妹子域名 spam.lyceum-life.com.ru
	ChannelSpamNetpiratesNet       Channel = "spam-netpirates-net"      // mailinator 姊妹子域名 spam.netpirates.net
	ChannelSpamNoIpNet             Channel = "spam-no-ip-net"           // mailinator 姊妹子域名 spam.no-ip.net
	ChannelSpamOzhOrg              Channel = "spam-ozh-org"             // mailinator 姊妹子域名 spam.ozh.org
	ChannelSpamPyphusOrg           Channel = "spam-pyphus-org"          // mailinator 姊妹子域名 spam.pyphus.org
	ChannelSpamShepPw              Channel = "spam-shep-pw"             // mailinator 姊妹子域名 spam.shep.pw
	ChannelSpamWtfAt               Channel = "spam-wtf-at"              // mailinator 姊妹子域名 spam.wtf.at
	ChannelSpamWulczerOrg          Channel = "spam-wulczer-org"         // mailinator 姊妹子域名 spam.wulczer.org
	ChannelCrapKakaduaNet          Channel = "crap-kakadua-net"         // mailinator 姊妹子域名 crap.kakadua.net
	ChannelSpamJanlugtNl           Channel = "spam-janlugt-nl"          // mailinator 姊妹子域名 spam.janlugt.nl
	ChannelMinBurningfishNet       Channel = "min-burningfish-net"      // mailinator 姊妹子域名 min.burningfish.net
	ChannelNospamThurstonsUs       Channel = "nospam-thurstons-us"      // mailinator 姊妹子域名 nospam.thurstons.us
	ChannelNullK3vinNet            Channel = "null-k3vin-net"           // mailinator 姊妹子域名 null.k3vin.net
	ChannelReallyIstrashCom        Channel = "really-istrash-com"       // mailinator 姊妹子域名 really.istrash.com
	ChannelSpamHortukOvh           Channel = "spam-hortuk-ovh"          // mailinator 姊妹子域名 spam.hortuk.ovh
	ChannelSinkFblayCom            Channel = "sink-fblay-com"           // mailinator 姊妹子域名 sink.fblay.com
	ChannelEtgdevDe                Channel = "etgdev-de"                // mailinator 姊妹子域名 etgdev.de
	ChannelMtmdevCom               Channel = "mtmdev-com"               // mailinator 姊妹子域名 mtmdev.com
	ChannelTestUnergieCom          Channel = "test-unergie-com"         // mailinator 姊妹子域名 test.unergie.com
	ChannelBlockBdeaCc             Channel = "block-bdea-cc"            // mailinator 姊妹子域名 block.bdea.cc
	ChannelTorchYiOrg              Channel = "torch-yi-org"             // mailinator 姊妹子域名 torch.yi.org
	ChannelCarlton183ChangeipNet   Channel = "carlton183-changeip-net"  // mailinator 姊妹子域名 183carlton.changeip.net
	ChannelMailFsmashOrg           Channel = "mail-fsmash-org"          // mailinator 姊妹子域名 mail.fsmash.org
	ChannelEbsComAr                Channel = "ebs-com-ar"               // mailinator 姊妹子域名 ebs.com.ar
	ChannelJamaTrenetEu            Channel = "jama-trenet-eu"           // mailinator 姊妹子域名 jama.trenet.eu
	ChannelBlackholeDjurbySe       Channel = "blackhole-djurby-se"      // mailinator 姊妹子域名 blackhole.djurby.se
	ChannelM8rDavidfuhrDe          Channel = "m8r-davidfuhr-de"         // mailinator 姊妹子域名 m8r.davidfuhr.de
	ChannelMiMeonBe                Channel = "mi-meon-be"               // mailinator 姊妹子域名 mi.meon.be
	ChannelMNikMe                  Channel = "m-nik-me"                 // mailinator 姊妹子域名 m.nik.me
	ChannelMailBentraskCom         Channel = "mail-bentrask-com"        // mail.bentrask.com
	ChannelTZibetNet               Channel = "t-zibet-net"              // t.zibet.net
	ChannelM8rMcasalCom            Channel = "m8r-mcasal-com"           // m8r.mcasal.com
	ChannelRamjaneMoooCom          Channel = "ramjane-mooo-com"         // ramjane.mooo.com
	ChannelRauxaSenyCat            Channel = "rauxa-seny-cat"           // rauxa.seny.cat
	ChannelSpWootAt                Channel = "sp-woot-at"               // sp.woot.at
	ChannelFwd2mEszettEs           Channel = "fwd2m-eszett-es"          // fwd2m.eszett.es
	ChannelM887At                  Channel = "m-887-at"                 // m.887.at
	ChannelN16888888Cyou           Channel = "16888888-cyou"            // mailmomy 域名 16888888.cyou
	ChannelN17666688Shop           Channel = "17666688-shop"            // mailmomy 域名 17666688.shop
	ChannelN282mailCom             Channel = "282mail-com"              // mailmomy 域名 282mail.com
	ChannelBsdu32Buzz              Channel = "bsdu32-buzz"              // mailmomy 域名 bsdu32.buzz
	ChannelDoxu243Buzz             Channel = "doxu243-buzz"             // mailmomy 域名 doxu243.buzz
	ChannelEasymePro               Channel = "easyme-pro"               // mailmomy 域名 easyme.pro
	ChannelEvergreencoShop         Channel = "evergreenco-shop"         // mailmomy 域名 evergreenco.shop
	ChannelLayuemingPics           Channel = "layueming-pics"           // mailmomy 域名 layueming.pics
	ChannelMingyuekejiOnline       Channel = "mingyuekeji-online"       // mailmomy 域名 mingyuekeji.online
	ChannelMingyuemingClick        Channel = "mingyueming-click"        // mailmomy 域名 mingyueming.click
	ChannelMingyuemingShop         Channel = "mingyueming-shop"         // mailmomy 域名 mingyueming.shop
	ChannelMingyukejiLol           Channel = "mingyukeji-lol"           // mailmomy 域名 mingyukeji.lol
	ChannelNuxh62Space             Channel = "nuxh62-space"             // mailmomy 域名 nuxh62.space
	ChannelProidCloudIpCc          Channel = "proid-cloud-ip-cc"        // mailmomy 域名 proid.cloud-ip.cc
	ChannelSbookPics               Channel = "sbook-pics"               // mailmomy 域名 sbook.pics
	ChannelXue32Buzz               Channel = "xue32-buzz"               // mailmomy 域名 xue32.buzz
	ChannelBSmellyCc               Channel = "b-smelly-cc"              // mailinator 姊妹 b.smelly.cc
	ChannelDeaSoonIt               Channel = "dea-soon-it"              // mailinator 姊妹 dea.soon.it
	ChannelDisposableAlSudaniCom   Channel = "disposable-al-sudani-com" // mailinator 姊妹 disposable.al-sudani.com
	ChannelDisposableNogonadNl     Channel = "disposable-nogonad-nl"    // mailinator 姊妹 disposable.nogonad.nl
	ChannelJFairuseOrg             Channel = "j-fairuse-org"            // mailinator 姊妹 j.fairuse.org
	ChannelMnCurppaCom             Channel = "mn-curppa-com"            // mailinator 姊妹 mn.curppa.com
	ChannelMailinatorzzmoooCom     Channel = "mailinatorzz-mooo-com"    // mailinator 姊妹 mailinatorzz.mooo.com
	ChannelNotfond404Mn            Channel = "notfond-404-mn"           // mailinator 姊妹 notfond.404.mn
)

/*
 * EmailInfo 创建临时邮箱后返回的邮箱信息
 * 包含邮箱地址和生命周期信息，认证令牌由 SDK 内部维护，不对外暴露
 */
type EmailInfo struct {
	/* 创建该邮箱所使用的渠道 */
	Channel Channel `json:"channel"`
	/* 临时邮箱地址 */
	Email string `json:"email"`
	/* 认证令牌，由 SDK 内部维护，不对外暴露 */
	token string
	/* 邮箱过期时间（ISO 8601 字符串或 Unix 时间戳） */
	ExpiresAt any `json:"expiresAt,omitempty"`
	/* 邮箱创建时间（ISO 8601 字符串） */
	CreatedAt string `json:"createdAt,omitempty"`
}

/*
 * EmailAttachment 标准化邮件附件
 * 不同渠道的附件字段名不同，SDK 统一归一化为此结构
 */
type EmailAttachment struct {
	/* 附件文件名 */
	Filename string `json:"filename"`
	/* 附件大小（字节） */
	Size int64 `json:"size,omitempty"`
	/* MIME 类型，如 application/pdf */
	ContentType string `json:"contentType,omitempty"`
	/* 附件下载地址 */
	URL string `json:"url,omitempty"`
}

/*
 * Email 标准化邮件格式
 * 所有渠道的 API 响应经过归一化处理后统一返回此结构
 */
type Email struct {
	/* 邮件唯一标识 */
	ID string `json:"id"`
	/* 发件人邮箱地址 */
	From string `json:"from"`
	/* 收件人邮箱地址 */
	To string `json:"to"`
	/* 邮件主题 */
	Subject string `json:"subject"`
	/* 纯文本内容 */
	Text string `json:"text"`
	/* HTML 内容 */
	HTML string `json:"html"`
	/* ISO 8601 格式的日期字符串 */
	Date string `json:"date"`
	/* 是否已读 */
	IsRead bool `json:"isRead"`
	/* 附件列表 */
	Attachments []EmailAttachment `json:"attachments"`
}

/*
 * GetEmailsResult 获取邮件列表的返回结果
 * Success 为 false 时表示请求失败（重试耗尽），Emails 为空切片
 */
type GetEmailsResult struct {
	/* 所使用的渠道 */
	Channel Channel `json:"channel"`
	/* 查询的邮箱地址 */
	Email string `json:"email"`
	/* 邮件列表，失败时为空切片 */
	Emails []Email `json:"emails"`
	/* 请求是否成功，false 表示重试耗尽后仍失败 */
	Success bool `json:"success"`
}

/*
 * GenerateEmailOptions 创建临时邮箱的选项
 *
 * 示例:
 *   info, err := GenerateEmail(&GenerateEmailOptions{
 *       Channel: ChannelMailTm,
 *       Retry:   &RetryOptions{MaxRetries: 3},
 *   })
 */
type GenerateEmailOptions struct {
	/* 指定渠道，空字符串则随机选择 */
	Channel Channel
	/* 邮箱有效时长（分钟），仅 tempmail 渠道支持，默认 30 */
	Duration int
	/* 指定邮箱域名/接入域名：如 tempmail-cn、tempmail-lol、fake-legal、catchmail、mailforspam；moakt 渠道表示语言路径（如 zh、en） */
	Domain *string
	/* 重试配置，nil 则使用默认值（最多重试 2 次） */
	Retry *RetryOptions
}

/*
 * GetEmailsOptions 获取邮件列表的选项
 * Channel/Email/Token 等由 SDK 从 EmailInfo 中自动获取，用户无需手动传递
 *
 * 示例:
 *   info, _ := GenerateEmail(&GenerateEmailOptions{Channel: ChannelMailTm})
 *   result, _ := GetEmails(info, nil) // nil 表示使用默认配置
 *   if result.Success && len(result.Emails) > 0 {
 *       fmt.Println("收到邮件:", result.Emails[0].Subject)
 *   }
 */
type GetEmailsOptions struct {
	/* 重试配置，nil 则使用默认值（最多重试 2 次） */
	Retry *RetryOptions
}
