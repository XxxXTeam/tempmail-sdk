"""
SDK 主入口
聚合所有渠道的逻辑，提供统一的 API
"""

import random
from typing import Optional, List
from .types import (
    EmailInfo, Email, GetEmailsResult,
    GenerateEmailOptions, GetEmailsOptions,
    ChannelInfo,
)
from .retry import with_retry, with_retry_with_attempts
from .telemetry import report_telemetry
from .logger import get_logger
from .providers import (
    tempmail, tempmail_cn, ta_easy, linshiyou, mffac, tempmail_lol, chatgpt_org_uk,
    awamail, mail_tm,
    dropmail, guerrillamail, maildrop, smail_pw,
    vip_215, fake_legal, moakt, tenminute_one,
    email10min, mjj_cm, linshi_co,
    harakirimail, tempmail_plus,
    tempmail_lol_v2, tempgbox, emailnator, temporam, neighbours, guerrillamail_mirrors, temp_mail_io, mail_cx, devmail_uk, cleantempmail,
    catchmail, mailforspam, tempmailc, mailnesia, throwawaymail, shitty_email, tempmailpro, m2u, tempy_email, inboxkitten, getnada, mail123, mail10s, webmailtemp, tempfastmail, one_sec_mail, fakemail, openinbox, inboxes, uncorreotemporal,
    zhujump, fmail, ockito, anonbox, duckmail,
    mailinator,
    tempmail365,
    tempinbox,
    byom, anonymmail, eyepaste,
    segamail,
    mail_sunls,
    expressinboxhub,
    lroid,
    haribu,
    pleasenospam,
    rootsh,
    fake_email_site,
    mohmal,
    mailgolem,
    best_temp_mail,
    disposablemail_app,
    mailtemp_cc,
    minuteinbox,
    mailcatch,
    tempemail_co,
    tempemails_net,
    altmails,
    tempemail_info,
    smailpro,
    tempmailten,
    maildrop_cc,
    tenminutemail_net,
    linshiyouxiang_net,
    tempmail_fyi,
    disposablemail_com,
    tempp_mails,
    emailtemp_org,
)

# 所有支持的公共渠道列表；随机尝试顺序会基于该列表在本端独立洗牌，不要求跨 SDK 一致
ALL_CHANNELS = [
    "tempmail",
    "tempmail-cn",
    "ta-easy",
    "10minute-one",
    "xghff-com",
    "oqqaj-com",
    "psovv-com",
    "dbwot-com",
    "ygwpr-com",
    "imxwe-com",
    "linshiyou",
    "mffac",
    "tempmail-lol",
    "chatgpt-org-uk",
    "temp-mail-io",
    "mail-cx",
    "ddker-com",
    "catchmail",
    "catchmail-mailistry",
    "catchmail-zeppost",
    "mailforspam",
    "mailforspam-tempmail-io",
    "mailforspam-disposable",
    "tempmailc",
    "mailnesia",
    "throwawaymail",
    "shitty-email",
    "tempmailpro",
    "devmail-uk",
    "cleantempmail",
    "inboxkitten",
    "getnada",
    "1vpn-net",
    "abematv-com",
    "abematv-net",
    "abematv-org",
    "aceh-cc",
    "bangkabelitung-net",
    "cctruyen-com",
    "getnada-com",
    "getnada-email",
    "getnada-net",
    "jawatengah-net",
    "jawatimur-net",
    "kalimantanbarat-net",
    "kalimantanselatan-net",
    "kalimantantengah-net",
    "kalimantantimur-net",
    "kalimantanutara-net",
    "kepulauanriau-net",
    "luxury345-com",
    "malukuutara-net",
    "nusatenggarabarat-net",
    "nusatenggaratimur-net",
    "papuabarat-net",
    "papuabaratdaya-net",
    "papuaselatan-net",
    "pehol-com",
    "ptruyen-com",
    "pulaubali-net",
    "riau-net",
    "seokey-org",
    "sulawesibarat-net",
    "sulawesiselatan-net",
    "sulawesitengah-net",
    "sulawesitenggara-net",
    "sumaterabarat-net",
    "sumateraselatan-net",
    "sumaterautara-net",
    "villatogel-com",
    "mail123",
    "mail10s",
    "webmailtemp",
    "tempfastmail",
    "1sec-mail",
    "fakemail",
    "openinbox",
    "inboxes",
    "uncorreotemporal",
    "awamail",
    "mail-tm",
    "web-library-net",
    "dropmail",
    "guerrillamail",
    "guerrillamail-com",
    "maildrop",
    "smail-pw",
    "vip-215",
    "fake-legal",
    "imgui-de",
    "pulsewebmenu-de",
    "moakt",
    "drmail-in",
    "teml-net",
    "tmpeml-com",
    "tmpbox-net",
    "moakt-cc",
    "disbox-net",
    "tmpmail-org",
    "tmpmail-net",
    "tmails-net",
    "disbox-org",
    "moakt-co",
    "moakt-ws",
    "tmail-ws",
    "bareed-ws",
    "email10min",
    "mjj-cm",
    "linshi-co",
    "harakirimail",
    "jqkjqk-xyz",
    "lyhlevi-com",
    "tempmail-plus",
    "fexpost-com",
    "fexbox-org",
    "mailbox-in-ua",
    "rover-info",
    "chitthi-in",
    "fextemp-com",
    "any-pink",
    "merepost-com",
    "tempmail-lol-v2",
    "tempgbox",
    "emailnator",
    "temporam",
    "neighbours",
    "sharklasers",
    "sharklasers-com",
    "grr-la",
    "grr-la-com",
    "guerrillamail-info",
    "spam4me",
    "guerrillamail-net",
    "guerrillamail-org",
    "guerrillamailblock",
    "guerrillamail-com-www",
    "m2u",
    "tempy-email",
    "fmail",
    "ockito",
    "anonbox",
    "duckmail",
    "mailinator",
    "tempmail365",
    "tempinbox",
    "byom",
    "anonymmail",
    "eyepaste",
    "segamail",
    "mail-sunls",
    "expressinboxhub",
    "lroid",
    "haribu",
    "pleasenospam",
    "spamlessmail-org",
    "rootsh",
    "fake-email-site",
    "mohmal",
    "mailgolem",
    "best-temp-mail",
    "disposablemail-app",
    "mailtemp-cc",
    "minuteinbox",
    "mailcatch",
    "tempemail-co",
    "tempemails-net",
    "altmails",
    "tempemail-info",
    "smailpro",
    "tempmailten",
    "maildrop-cc",
    "10minutemail-net",
    "linshiyouxiang-net",
    "tempmail-fyi",
    "disposablemail-com",
    "tempp-mails",
    "emailtemp-org",
]

# 渠道信息映射表
CHANNEL_INFO_MAP = {
    "tempmail": ChannelInfo(channel="tempmail", name="TempMail", website="tempmail.ing"),
    "tempmail-cn": ChannelInfo(channel="tempmail-cn", name="TempMail CN", website="tempmail.cn"),
    "ta-easy": ChannelInfo(channel="ta-easy", name="TA Easy", website="ta-easy.com"),
    "10minute-one": ChannelInfo(channel="10minute-one", name="10 Minute Email", website="10minutemail.one"),
    "xghff-com": ChannelInfo(channel="xghff-com", name="xghff.com", website="10minutemail.one"),
    "oqqaj-com": ChannelInfo(channel="oqqaj-com", name="oqqaj.com", website="10minutemail.one"),
    "psovv-com": ChannelInfo(channel="psovv-com", name="psovv.com", website="10minutemail.one"),
    "dbwot-com": ChannelInfo(channel="dbwot-com", name="dbwot.com", website="10minutemail.one"),
    "ygwpr-com": ChannelInfo(channel="ygwpr-com", name="ygwpr.com", website="10minutemail.one"),
    "imxwe-com": ChannelInfo(channel="imxwe-com", name="imxwe.com", website="10minutemail.one"),
    "linshiyou": ChannelInfo(channel="linshiyou", name="临时邮", website="linshiyou.com"),
    "mffac": ChannelInfo(channel="mffac", name="MFFAC", website="mffac.com"),
    "tempmail-lol": ChannelInfo(channel="tempmail-lol", name="TempMail LOL", website="tempmail.lol"),
    "chatgpt-org-uk": ChannelInfo(channel="chatgpt-org-uk", name="ChatGPT Mail", website="mail.chatgpt.org.uk"),
    "temp-mail-io": ChannelInfo(channel="temp-mail-io", name="Temp-Mail.io", website="temp-mail.io"),
    "mail-cx": ChannelInfo(channel="mail-cx", name="Mail.cx", website="mail.cx"),
    "ddker-com": ChannelInfo(channel="ddker-com", name="ddker.com", website="mail.cx"),
    "catchmail": ChannelInfo(channel="catchmail", name="Catchmail", website="catchmail.io"),
    "catchmail-mailistry": ChannelInfo(channel="catchmail-mailistry", name="Catchmail Mailistry", website="mailistry.com"),
    "catchmail-zeppost": ChannelInfo(channel="catchmail-zeppost", name="Catchmail Zeppost", website="zeppost.com"),
    "mailforspam": ChannelInfo(channel="mailforspam", name="MailForSpam", website="mailforspam.com"),
    "mailforspam-tempmail-io": ChannelInfo(channel="mailforspam-tempmail-io", name="MailForSpam TempMail.io", website="tempmail.io"),
    "mailforspam-disposable": ChannelInfo(channel="mailforspam-disposable", name="MailForSpam Disposable", website="disposable.email"),
    "tempmailc": ChannelInfo(channel="tempmailc", name="TempMailC", website="tempmailc.com"),
    "mailnesia": ChannelInfo(channel="mailnesia", name="Mailnesia", website="mailnesia.com"),
    "throwawaymail": ChannelInfo(channel="throwawaymail", name="ThrowawayMail", website="throwawaymail.app"),
    "shitty-email": ChannelInfo(channel="shitty-email", name="shitty.email", website="shitty.email"),
    "tempmailpro": ChannelInfo(channel="tempmailpro", name="TempMail Pro", website="tempmailpro.us"),
    "devmail-uk": ChannelInfo(channel="devmail-uk", name="DevMail UK", website="devmail.uk"),
    "cleantempmail": ChannelInfo(channel="cleantempmail", name="CleanTempMail", website="cleantempmail.com"),
    "inboxkitten": ChannelInfo(channel="inboxkitten", name="InboxKitten", website="inboxkitten.com"),
    "getnada": ChannelInfo(channel="getnada", name="GetNada", website="getnada.net"),
    "1vpn-net": ChannelInfo(channel="1vpn-net", name="1vpn.net", website="getnada.net"),
    "abematv-com": ChannelInfo(channel="abematv-com", name="abematv.com", website="getnada.net"),
    "abematv-net": ChannelInfo(channel="abematv-net", name="abematv.net", website="getnada.net"),
    "abematv-org": ChannelInfo(channel="abematv-org", name="abematv.org", website="getnada.net"),
    "aceh-cc": ChannelInfo(channel="aceh-cc", name="aceh.cc", website="getnada.net"),
    "bangkabelitung-net": ChannelInfo(channel="bangkabelitung-net", name="bangkabelitung.net", website="getnada.net"),
    "cctruyen-com": ChannelInfo(channel="cctruyen-com", name="cctruyen.com", website="getnada.net"),
    "getnada-com": ChannelInfo(channel="getnada-com", name="getnada.com", website="getnada.net"),
    "getnada-email": ChannelInfo(channel="getnada-email", name="getnada.email", website="getnada.net"),
    "getnada-net": ChannelInfo(channel="getnada-net", name="getnada.net", website="getnada.net"),
    "jawatengah-net": ChannelInfo(channel="jawatengah-net", name="jawatengah.net", website="getnada.net"),
    "jawatimur-net": ChannelInfo(channel="jawatimur-net", name="jawatimur.net", website="getnada.net"),
    "kalimantanbarat-net": ChannelInfo(channel="kalimantanbarat-net", name="kalimantanbarat.net", website="getnada.net"),
    "kalimantanselatan-net": ChannelInfo(channel="kalimantanselatan-net", name="kalimantanselatan.net", website="getnada.net"),
    "kalimantantengah-net": ChannelInfo(channel="kalimantantengah-net", name="kalimantantengah.net", website="getnada.net"),
    "kalimantantimur-net": ChannelInfo(channel="kalimantantimur-net", name="kalimantantimur.net", website="getnada.net"),
    "kalimantanutara-net": ChannelInfo(channel="kalimantanutara-net", name="kalimantanutara.net", website="getnada.net"),
    "kepulauanriau-net": ChannelInfo(channel="kepulauanriau-net", name="kepulauanriau.net", website="getnada.net"),
    "luxury345-com": ChannelInfo(channel="luxury345-com", name="luxury345.com", website="getnada.net"),
    "malukuutara-net": ChannelInfo(channel="malukuutara-net", name="malukuutara.net", website="getnada.net"),
    "nusatenggarabarat-net": ChannelInfo(channel="nusatenggarabarat-net", name="nusatenggarabarat.net", website="getnada.net"),
    "nusatenggaratimur-net": ChannelInfo(channel="nusatenggaratimur-net", name="nusatenggaratimur.net", website="getnada.net"),
    "papuabarat-net": ChannelInfo(channel="papuabarat-net", name="papuabarat.net", website="getnada.net"),
    "papuabaratdaya-net": ChannelInfo(channel="papuabaratdaya-net", name="papuabaratdaya.net", website="getnada.net"),
    "papuaselatan-net": ChannelInfo(channel="papuaselatan-net", name="papuaselatan.net", website="getnada.net"),
    "pehol-com": ChannelInfo(channel="pehol-com", name="pehol.com", website="getnada.net"),
    "ptruyen-com": ChannelInfo(channel="ptruyen-com", name="ptruyen.com", website="getnada.net"),
    "pulaubali-net": ChannelInfo(channel="pulaubali-net", name="pulaubali.net", website="getnada.net"),
    "riau-net": ChannelInfo(channel="riau-net", name="riau.net", website="getnada.net"),
    "seokey-org": ChannelInfo(channel="seokey-org", name="seokey.org", website="getnada.net"),
    "sulawesibarat-net": ChannelInfo(channel="sulawesibarat-net", name="sulawesibarat.net", website="getnada.net"),
    "sulawesiselatan-net": ChannelInfo(channel="sulawesiselatan-net", name="sulawesiselatan.net", website="getnada.net"),
    "sulawesitengah-net": ChannelInfo(channel="sulawesitengah-net", name="sulawesitengah.net", website="getnada.net"),
    "sulawesitenggara-net": ChannelInfo(channel="sulawesitenggara-net", name="sulawesitenggara.net", website="getnada.net"),
    "sumaterabarat-net": ChannelInfo(channel="sumaterabarat-net", name="sumaterabarat.net", website="getnada.net"),
    "sumateraselatan-net": ChannelInfo(channel="sumateraselatan-net", name="sumateraselatan.net", website="getnada.net"),
    "sumaterautara-net": ChannelInfo(channel="sumaterautara-net", name="sumaterautara.net", website="getnada.net"),
    "villatogel-com": ChannelInfo(channel="villatogel-com", name="villatogel.com", website="getnada.net"),
    "mail123": ChannelInfo(channel="mail123", name="Mail123", website="mail123.fr"),
    "mail10s": ChannelInfo(channel="mail10s", name="Mail10s", website="mail10s.com"),
    "webmailtemp": ChannelInfo(channel="webmailtemp", name="WebMailTemp", website="webmailtemp.com"),
    "tempfastmail": ChannelInfo(channel="tempfastmail", name="TempFastMail", website="tempfastmail.com"),
    "1sec-mail": ChannelInfo(channel="1sec-mail", name="1SecMail", website="1sec-mail.com"),
    "fakemail": ChannelInfo(channel="fakemail", name="FakeMail", website="fakemail.net"),
    "openinbox": ChannelInfo(channel="openinbox", name="OpenInbox", website="openinbox.io"),
    "inboxes": ChannelInfo(channel="inboxes", name="Inboxes", website="inboxes.com"),
    "uncorreotemporal": ChannelInfo(channel="uncorreotemporal", name="UnCorreoTemporal", website="uncorreotemporal.com"),
    "awamail": ChannelInfo(channel="awamail", name="AwaMail", website="awamail.com"),
    "mail-tm": ChannelInfo(channel="mail-tm", name="Mail.tm", website="mail.tm"),
    "web-library-net": ChannelInfo(channel="web-library-net", name="web-library.net", website="mail.tm"),
    "dropmail": ChannelInfo(channel="dropmail", name="DropMail", website="dropmail.me"),
    "guerrillamail": ChannelInfo(channel="guerrillamail", name="Guerrilla Mail", website="guerrillamail.com"),
    "guerrillamail-com": ChannelInfo(channel="guerrillamail-com", name="GuerrillaMail Root", website="guerrillamail.com"),
    "maildrop": ChannelInfo(channel="maildrop", name="Maildrop", website="maildrop.cx"),
    "smail-pw": ChannelInfo(channel="smail-pw", name="Smail.pw", website="smail.pw"),
    "vip-215": ChannelInfo(channel="vip-215", name="VIP 215", website="vip.215.im"),
    "fake-legal": ChannelInfo(channel="fake-legal", name="Fake Legal", website="fake.legal"),
    "imgui-de": ChannelInfo(channel="imgui-de", name="imgui.de", website="fake.legal"),
    "pulsewebmenu-de": ChannelInfo(channel="pulsewebmenu-de", name="pulsewebmenu.de", website="fake.legal"),
    "moakt": ChannelInfo(channel="moakt", name="Moakt", website="moakt.com"),
    "drmail-in": ChannelInfo(channel="drmail-in", name="drmail.in", website="moakt.com"),
    "teml-net": ChannelInfo(channel="teml-net", name="teml.net", website="moakt.com"),
    "tmpeml-com": ChannelInfo(channel="tmpeml-com", name="tmpeml.com", website="moakt.com"),
    "tmpbox-net": ChannelInfo(channel="tmpbox-net", name="tmpbox.net", website="moakt.com"),
    "moakt-cc": ChannelInfo(channel="moakt-cc", name="moakt.cc", website="moakt.com"),
    "disbox-net": ChannelInfo(channel="disbox-net", name="disbox.net", website="moakt.com"),
    "tmpmail-org": ChannelInfo(channel="tmpmail-org", name="tmpmail.org", website="moakt.com"),
    "tmpmail-net": ChannelInfo(channel="tmpmail-net", name="tmpmail.net", website="moakt.com"),
    "tmails-net": ChannelInfo(channel="tmails-net", name="tmails.net", website="moakt.com"),
    "disbox-org": ChannelInfo(channel="disbox-org", name="disbox.org", website="moakt.com"),
    "moakt-co": ChannelInfo(channel="moakt-co", name="moakt.co", website="moakt.com"),
    "moakt-ws": ChannelInfo(channel="moakt-ws", name="moakt.ws", website="moakt.com"),
    "tmail-ws": ChannelInfo(channel="tmail-ws", name="tmail.ws", website="moakt.com"),
    "bareed-ws": ChannelInfo(channel="bareed-ws", name="bareed.ws", website="moakt.com"),
    "email10min": ChannelInfo(channel="email10min", name="Email10Min", website="email10min.com"),
    "mjj-cm": ChannelInfo(channel="mjj-cm", name="MJJ Mail", website="mjj.cm"),
    "linshi-co": ChannelInfo(channel="linshi-co", name="临时Co", website="linshi.co"),
    "harakirimail": ChannelInfo(channel="harakirimail", name="HarakiriMail", website="harakirimail.com"),
    "jqkjqk-xyz": ChannelInfo(channel="jqkjqk-xyz", name="jqkjqk.xyz", website="mail.zhujump.com"),
    "lyhlevi-com": ChannelInfo(channel="lyhlevi-com", name="LyhLevi MoeMail", website="lyhlevi.com"),
    "tempmail-plus": ChannelInfo(channel="tempmail-plus", name="TempMail Plus", website="tempmail.plus"),
    "fexpost-com": ChannelInfo(channel="fexpost-com", name="fexpost.com", website="tempmail.plus"),
    "fexbox-org": ChannelInfo(channel="fexbox-org", name="fexbox.org", website="tempmail.plus"),
    "mailbox-in-ua": ChannelInfo(channel="mailbox-in-ua", name="mailbox.in.ua", website="tempmail.plus"),
    "rover-info": ChannelInfo(channel="rover-info", name="rover.info", website="tempmail.plus"),
    "chitthi-in": ChannelInfo(channel="chitthi-in", name="chitthi.in", website="tempmail.plus"),
    "fextemp-com": ChannelInfo(channel="fextemp-com", name="fextemp.com", website="tempmail.plus"),
    "any-pink": ChannelInfo(channel="any-pink", name="any.pink", website="tempmail.plus"),
    "merepost-com": ChannelInfo(channel="merepost-com", name="merepost.com", website="tempmail.plus"),
    "tempmail-lol-v2": ChannelInfo(channel="tempmail-lol-v2", name="TempMail LOL V2", website="tempmail.lol"),
    "tempgbox": ChannelInfo(channel="tempgbox", name="TempGBox", website="tempgbox.net"),
    "emailnator": ChannelInfo(channel="emailnator", name="Emailnator", website="emailnator.com"),
    "temporam": ChannelInfo(channel="temporam", name="Temporam", website="temporam.com"),
    "neighbours": ChannelInfo(channel="neighbours", name="Neighbours", website="neighbours.sh"),
    "sharklasers": ChannelInfo(channel="sharklasers", name="SharkLasers", website="sharklasers.com"),
    "sharklasers-com": ChannelInfo(channel="sharklasers-com", name="SharkLasers Root", website="sharklasers.com"),
    "grr-la": ChannelInfo(channel="grr-la", name="Grr.la", website="grr.la"),
    "grr-la-com": ChannelInfo(channel="grr-la-com", name="Grr.la Root", website="grr.la"),
    "guerrillamail-info": ChannelInfo(channel="guerrillamail-info", name="GuerrillaMail Info", website="guerrillamail.info"),
    "spam4me": ChannelInfo(channel="spam4me", name="Spam4.me", website="spam4.me"),
    "guerrillamail-net": ChannelInfo(channel="guerrillamail-net", name="GuerrillaMail Net", website="guerrillamail.net"),
    "guerrillamail-org": ChannelInfo(channel="guerrillamail-org", name="GuerrillaMail Org", website="guerrillamail.org"),
    "guerrillamailblock": ChannelInfo(channel="guerrillamailblock", name="GuerrillaMailBlock", website="guerrillamailblock.com"),
    "guerrillamail-com-www": ChannelInfo(channel="guerrillamail-com-www", name="GuerrillaMail WWW", website="guerrillamail.com"),
    "m2u": ChannelInfo(channel="m2u", name="MailToYou", website="m2u.io"),
    "tempy-email": ChannelInfo(channel="tempy-email", name="Tempy Email", website="tempy.email"),
    "fmail": ChannelInfo(channel="fmail", name="FMail", website="fmail.men"),
    "ockito": ChannelInfo(channel="ockito", name="Ockito", website="ockito.com"),
    "anonbox": ChannelInfo(channel="anonbox", name="Anonbox", website="anonbox.net"),
    "duckmail": ChannelInfo(channel="duckmail", name="DuckMail", website="duckmail.sbs"),
    "mailinator": ChannelInfo(channel="mailinator", name="Mailinator", website="mailinator.com"),
    "tempmail365": ChannelInfo(channel="tempmail365", name="Tempmail365", website="tempmail365.cn"),
    "tempinbox": ChannelInfo(channel="tempinbox", name="TempInbox", website="www.tempinbox.xyz"),
    "byom": ChannelInfo(channel="byom", name="Byom", website="byom.de"),
    "anonymmail": ChannelInfo(channel="anonymmail", name="Anonymmail", website="anonymmail.net"),
    "eyepaste": ChannelInfo(channel="eyepaste", name="Eyepaste", website="eyepaste.com"),
    "segamail": ChannelInfo(channel="segamail", name="Segamail", website="segamail.com"),
    "mail-sunls": ChannelInfo(channel="mail-sunls", name="Mail Sunls", website="mail.sunls.de"),
    "expressinboxhub": ChannelInfo(channel="expressinboxhub", name="ExpressInboxHub", website="expressinboxhub.com"),
    "lroid": ChannelInfo(channel="lroid", name="Lroid", website="lroid.com"),
    "haribu": ChannelInfo(channel="haribu", name="Haribu", website="haribu.net"),
    "pleasenospam": ChannelInfo(channel="pleasenospam", name="PleeaseNoSpam", website="pleasenospam.email"),
    "spamlessmail-org": ChannelInfo(channel="spamlessmail-org", name="spamlessmail.org", website="pleasenospam.email"),
    "rootsh": ChannelInfo(channel="rootsh", name="Rootsh(BccTo)", website="rootsh.com"),
    "fake-email-site": ChannelInfo(channel="fake-email-site", name="FakeEmailSite", website="fake-email.site"),
    "mohmal": ChannelInfo(channel="mohmal", name="Mohmal", website="mohmal.com"),
    "mailgolem": ChannelInfo(channel="mailgolem", name="MailGolem", website="mailgolem.com"),
    "best-temp-mail": ChannelInfo(channel="best-temp-mail", name="BestTempMail", website="best-temp-mail.com"),
    "disposablemail-app": ChannelInfo(channel="disposablemail-app", name="DisposableMail", website="disposablemail.app"),
    "mailtemp-cc": ChannelInfo(channel="mailtemp-cc", name="MailTemp.cc", website="mailtemp.cc"),
    "minuteinbox": ChannelInfo(channel="minuteinbox", name="MinuteInbox", website="minuteinbox.com"),
    "mailcatch": ChannelInfo(channel="mailcatch", name="MailCatch", website="mailcatch.com"),
    "tempemail-co": ChannelInfo(channel="tempemail-co", name="TempEmail.co", website="tempemail.co"),
    "tempemails-net": ChannelInfo(channel="tempemails-net", name="TempEmails.net", website="tempemails.net"),
    "altmails": ChannelInfo(channel="altmails", name="AltMails", website="tempmail.altmails.com"),
    "tempemail-info": ChannelInfo(channel="tempemail-info", name="TempEmailInfo", website="tempemail.info"),
    "smailpro": ChannelInfo(channel="smailpro", name="SmailPro", website="smailpro.com"),
    "tempmailten": ChannelInfo(channel="tempmailten", name="TempMailTen", website="tempmailten.com"),
    "maildrop-cc": ChannelInfo(channel="maildrop-cc", name="MailDrop.cc", website="maildrop.cc"),
    "10minutemail-net": ChannelInfo(channel="10minutemail-net", name="10MinuteMail.net", website="10minutemail.net"),
    "linshiyouxiang-net": ChannelInfo(channel="linshiyouxiang-net", name="LinShiYouXiang", website="linshiyouxiang.net"),
    "tempmail-fyi": ChannelInfo(channel="tempmail-fyi", name="TempMail.FYI", website="temp-mail.fyi"),
    "disposablemail-com": ChannelInfo(channel="disposablemail-com", name="DisposableMail", website="disposablemail.com"),
    "tempp-mails": ChannelInfo(channel="tempp-mails", name="TemppMails", website="tempp-mails.com"),
    "emailtemp-org": ChannelInfo(channel="emailtemp-org", name="EmailTemp", website="emailtemp.org"),
}


def list_channels() -> List[ChannelInfo]:
    """获取所有支持的渠道列表"""
    return [CHANNEL_INFO_MAP[ch] for ch in ALL_CHANNELS]


def get_channel_info(channel: str) -> Optional[ChannelInfo]:
    """获取指定渠道的详细信息"""
    return CHANNEL_INFO_MAP.get(channel)


def _require_token(token: Optional[str], channel: str) -> str:
    """验证 token 不为空，为空则抛出 ValueError"""
    if not token:
        raise ValueError(f"token is required for {channel} channel")
    return token


def _raise(exc: Exception):
    """在 lambda 表达式中抛出异常的辅助函数（lambda 内无法直接使用 raise 语句）"""
    raise exc


def _with_channel(info: EmailInfo, channel: str) -> EmailInfo:
    info.channel = channel
    return info


def generate_email(options: Optional[GenerateEmailOptions] = None) -> Optional[EmailInfo]:
    """
    创建临时邮箱

    错误处理策略:
    - 指定渠道失败时，自动尝试其他可用渠道（打乱顺序逐个尝试）
    - 未指定渠道时，打乱全部渠道逐个尝试，直到成功
    - 所有渠道均不可用时返回 None（不抛出异常）

    示例:
        info = generate_email(GenerateEmailOptions(channel="guerrillamail"))
        if info:
            print(info.email)
    """
    if options is None:
        options = GenerateEmailOptions()

    logger = get_logger()

    # 构建尝试顺序：指定渠道优先尝试，失败后随机尝试其他渠道；未指定则打乱全部逐个尝试
    try_order = _build_channel_order(options.channel)

    channels_tried = 0
    last_err = ""
    for ch in try_order:
        channels_tried += 1
        logger.info(f"创建临时邮箱, 渠道: {ch}")
        r = with_retry_with_attempts(
            lambda c=ch: _generate_email_once(c, options),
            options.retry,
        )
        if r.ok:
            result = r.value
            assert result is not None
            logger.info(f"邮箱创建成功: {result.email} (渠道: {ch})")
            report_telemetry("generate_email", ch, True, r.attempts, channels_tried, "")
            return result
        err = r.error
        last_err = str(err) if err else "unknown"
        logger.warning(f"渠道 {ch} 不可用: {last_err}，尝试下一个渠道")

    logger.error("所有渠道均不可用，创建邮箱失败")
    report_telemetry("generate_email", "", False, 0, channels_tried, last_err)
    return None


def _build_channel_order(preferred: Optional[str] = None) -> list:
    """
    构建渠道尝试顺序
    指定渠道时优先尝试该渠道，其余渠道打乱追加
    未指定时打乱全部渠道
    返回值是本端运行时随机顺序，不作为跨 SDK 一致性约束
    """
    shuffled = ALL_CHANNELS[:]
    random.shuffle(shuffled)
    if not preferred:
        return shuffled
    rest = [ch for ch in shuffled if ch != preferred]
    return [preferred] + rest


# 创建邮箱分发表：渠道标识 -> 接收 GenerateEmailOptions 并返回 EmailInfo 的可调用对象
# 用于将原先的 if-elif 链替换为 O(1) 字典查找
_GENERATE_DISPATCH = {
    "tempmail": lambda o: tempmail.generate_email(o.duration),
    "tempmail-cn": lambda o: tempmail_cn.generate_email(o.domain),
    "ta-easy": lambda o: ta_easy.generate_email(),
    "10minute-one": lambda o: tenminute_one.generate_email(o.domain),
    "xghff-com": lambda o: _with_channel(tenminute_one.generate_email("xghff.com"), "xghff-com"),
    "oqqaj-com": lambda o: _with_channel(tenminute_one.generate_email("oqqaj.com"), "oqqaj-com"),
    "psovv-com": lambda o: _with_channel(tenminute_one.generate_email("psovv.com"), "psovv-com"),
    "dbwot-com": lambda o: _with_channel(tenminute_one.generate_email("dbwot.com"), "dbwot-com"),
    "ygwpr-com": lambda o: _with_channel(tenminute_one.generate_email("ygwpr.com"), "ygwpr-com"),
    "imxwe-com": lambda o: _with_channel(tenminute_one.generate_email("imxwe.com"), "imxwe-com"),
    "linshiyou": lambda o: linshiyou.generate_email(),
    "mffac": lambda o: mffac.generate_email(),
    "tempmail-lol": lambda o: tempmail_lol.generate_email(o.domain),
    "chatgpt-org-uk": lambda o: chatgpt_org_uk.generate_email(),
    "temp-mail-io": lambda o: temp_mail_io.generate_email(),
    "mail-cx": lambda o: mail_cx.generate_email(o.domain),
    "ddker-com": lambda o: _with_channel(mail_cx.generate_email("ddker.com"), "ddker-com"),
    "catchmail": lambda o: catchmail.generate_email(o.domain),
    "catchmail-mailistry": lambda o: _with_channel(catchmail.generate_email("mailistry.com"), "catchmail-mailistry"),
    "catchmail-zeppost": lambda o: _with_channel(catchmail.generate_email("zeppost.com"), "catchmail-zeppost"),
    "mailforspam": lambda o: mailforspam.generate_email(o.domain),
    "mailforspam-tempmail-io": lambda o: _with_channel(mailforspam.generate_email("tempmail.io"), "mailforspam-tempmail-io"),
    "mailforspam-disposable": lambda o: _with_channel(mailforspam.generate_email("disposable.email"), "mailforspam-disposable"),
    "tempmailc": lambda o: tempmailc.generate_email(),
    "mailnesia": lambda o: mailnesia.generate_email(),
    "throwawaymail": lambda o: throwawaymail.generate_email(),
    "shitty-email": lambda o: shitty_email.generate_email(),
    "tempmailpro": lambda o: tempmailpro.generate_email(),
    "devmail-uk": lambda o: devmail_uk.generate_email(),
    "cleantempmail": lambda o: cleantempmail.generate_email(),
    "m2u": lambda o: m2u.generate_email(),
    "tempy-email": lambda o: tempy_email.generate_email(o.domain),
    "inboxkitten": lambda o: inboxkitten.generate_email(),
    "getnada": lambda o: getnada.generate_email(o.domain),
    "1vpn-net": lambda o: getnada.generate_email("1vpn.net", "1vpn-net"),
    "abematv-com": lambda o: getnada.generate_email("abematv.com", "abematv-com"),
    "abematv-net": lambda o: getnada.generate_email("abematv.net", "abematv-net"),
    "abematv-org": lambda o: getnada.generate_email("abematv.org", "abematv-org"),
    "aceh-cc": lambda o: getnada.generate_email("aceh.cc", "aceh-cc"),
    "bangkabelitung-net": lambda o: getnada.generate_email("bangkabelitung.net", "bangkabelitung-net"),
    "cctruyen-com": lambda o: getnada.generate_email("cctruyen.com", "cctruyen-com"),
    "getnada-com": lambda o: getnada.generate_email("getnada.com", "getnada-com"),
    "getnada-email": lambda o: getnada.generate_email("getnada.email", "getnada-email"),
    "getnada-net": lambda o: getnada.generate_email("getnada.net", "getnada-net"),
    "jawatengah-net": lambda o: getnada.generate_email("jawatengah.net", "jawatengah-net"),
    "jawatimur-net": lambda o: getnada.generate_email("jawatimur.net", "jawatimur-net"),
    "kalimantanbarat-net": lambda o: getnada.generate_email("kalimantanbarat.net", "kalimantanbarat-net"),
    "kalimantanselatan-net": lambda o: getnada.generate_email("kalimantanselatan.net", "kalimantanselatan-net"),
    "kalimantantengah-net": lambda o: getnada.generate_email("kalimantantengah.net", "kalimantantengah-net"),
    "kalimantantimur-net": lambda o: getnada.generate_email("kalimantantimur.net", "kalimantantimur-net"),
    "kalimantanutara-net": lambda o: getnada.generate_email("kalimantanutara.net", "kalimantanutara-net"),
    "kepulauanriau-net": lambda o: getnada.generate_email("kepulauanriau.net", "kepulauanriau-net"),
    "luxury345-com": lambda o: getnada.generate_email("luxury345.com", "luxury345-com"),
    "malukuutara-net": lambda o: getnada.generate_email("malukuutara.net", "malukuutara-net"),
    "nusatenggarabarat-net": lambda o: getnada.generate_email("nusatenggarabarat.net", "nusatenggarabarat-net"),
    "nusatenggaratimur-net": lambda o: getnada.generate_email("nusatenggaratimur.net", "nusatenggaratimur-net"),
    "papuabarat-net": lambda o: getnada.generate_email("papuabarat.net", "papuabarat-net"),
    "papuabaratdaya-net": lambda o: getnada.generate_email("papuabaratdaya.net", "papuabaratdaya-net"),
    "papuaselatan-net": lambda o: getnada.generate_email("papuaselatan.net", "papuaselatan-net"),
    "pehol-com": lambda o: getnada.generate_email("pehol.com", "pehol-com"),
    "ptruyen-com": lambda o: getnada.generate_email("ptruyen.com", "ptruyen-com"),
    "pulaubali-net": lambda o: getnada.generate_email("pulaubali.net", "pulaubali-net"),
    "riau-net": lambda o: getnada.generate_email("riau.net", "riau-net"),
    "seokey-org": lambda o: getnada.generate_email("seokey.org", "seokey-org"),
    "sulawesibarat-net": lambda o: getnada.generate_email("sulawesibarat.net", "sulawesibarat-net"),
    "sulawesiselatan-net": lambda o: getnada.generate_email("sulawesiselatan.net", "sulawesiselatan-net"),
    "sulawesitengah-net": lambda o: getnada.generate_email("sulawesitengah.net", "sulawesitengah-net"),
    "sulawesitenggara-net": lambda o: getnada.generate_email("sulawesitenggara.net", "sulawesitenggara-net"),
    "sumaterabarat-net": lambda o: getnada.generate_email("sumaterabarat.net", "sumaterabarat-net"),
    "sumateraselatan-net": lambda o: getnada.generate_email("sumateraselatan.net", "sumateraselatan-net"),
    "sumaterautara-net": lambda o: getnada.generate_email("sumaterautara.net", "sumaterautara-net"),
    "villatogel-com": lambda o: getnada.generate_email("villatogel.com", "villatogel-com"),
    "mail123": lambda o: mail123.generate_email(),
    "mail10s": lambda o: mail10s.generate_email(),
    "webmailtemp": lambda o: webmailtemp.generate_email(),
    "tempfastmail": lambda o: tempfastmail.generate_email(),
    "1sec-mail": lambda o: one_sec_mail.generate_email(),
    "fakemail": lambda o: fakemail.generate_email(),
    "openinbox": lambda o: openinbox.generate_email(),
    "inboxes": lambda o: inboxes.generate_email(o.domain),
    "uncorreotemporal": lambda o: uncorreotemporal.generate_email(),
    "awamail": lambda o: awamail.generate_email(),
    "mail-tm": lambda o: mail_tm.generate_email(),
    "web-library-net": lambda o: _with_channel(mail_tm.generate_email(), "web-library-net"),
    "dropmail": lambda o: dropmail.generate_email(),
    "guerrillamail": lambda o: guerrillamail.generate_email(),
    "guerrillamail-com": lambda o: guerrillamail_mirrors.guerrillamail_com_generate(),
    "maildrop": lambda o: maildrop.generate_email(o.domain),
    "smail-pw": lambda o: smail_pw.generate_email(),
    "vip-215": lambda o: vip_215.generate_email(),
    "fake-legal": lambda o: fake_legal.generate_email(o.domain),
    "imgui-de": lambda o: fake_legal.generate_email("imgui.de", "imgui-de"),
    "pulsewebmenu-de": lambda o: fake_legal.generate_email("pulsewebmenu.de", "pulsewebmenu-de"),
    "moakt": lambda o: moakt.generate_email(o.domain),
    "drmail-in": lambda o: _with_channel(moakt.generate_email("drmail.in"), "drmail-in"),
    "teml-net": lambda o: _with_channel(moakt.generate_email("teml.net"), "teml-net"),
    "tmpeml-com": lambda o: _with_channel(moakt.generate_email("tmpeml.com"), "tmpeml-com"),
    "tmpbox-net": lambda o: _with_channel(moakt.generate_email("tmpbox.net"), "tmpbox-net"),
    "moakt-cc": lambda o: _with_channel(moakt.generate_email("moakt.cc"), "moakt-cc"),
    "disbox-net": lambda o: _with_channel(moakt.generate_email("disbox.net"), "disbox-net"),
    "tmpmail-org": lambda o: _with_channel(moakt.generate_email("tmpmail.org"), "tmpmail-org"),
    "tmpmail-net": lambda o: _with_channel(moakt.generate_email("tmpmail.net"), "tmpmail-net"),
    "tmails-net": lambda o: _with_channel(moakt.generate_email("tmails.net"), "tmails-net"),
    "disbox-org": lambda o: _with_channel(moakt.generate_email("disbox.org"), "disbox-org"),
    "moakt-co": lambda o: _with_channel(moakt.generate_email("moakt.co"), "moakt-co"),
    "moakt-ws": lambda o: _with_channel(moakt.generate_email("moakt.ws"), "moakt-ws"),
    "tmail-ws": lambda o: _with_channel(moakt.generate_email("tmail.ws"), "tmail-ws"),
    "bareed-ws": lambda o: _with_channel(moakt.generate_email("bareed.ws"), "bareed-ws"),
    "email10min": lambda o: email10min.generate_email(),
    "mjj-cm": lambda o: mjj_cm.generate_email(),
    "linshi-co": lambda o: linshi_co.generate_email(),
    "harakirimail": lambda o: harakirimail.generate_email(),
    "jqkjqk-xyz": lambda o: zhujump.generate_email("jqkjqk.xyz", "jqkjqk-xyz"),
    "lyhlevi-com": lambda o: zhujump.generate_email_for_instance("https://lyhlevi.com", "lyhlevi.com", "lyhlevi-com", 24 * 60 * 60 * 1000),
    "tempmail-plus": lambda o: tempmail_plus.generate_email(),
    "fexpost-com": lambda o: tempmail_plus.generate_email("fexpost.com", "fexpost-com"),
    "fexbox-org": lambda o: tempmail_plus.generate_email("fexbox.org", "fexbox-org"),
    "mailbox-in-ua": lambda o: tempmail_plus.generate_email("mailbox.in.ua", "mailbox-in-ua"),
    "rover-info": lambda o: tempmail_plus.generate_email("rover.info", "rover-info"),
    "chitthi-in": lambda o: tempmail_plus.generate_email("chitthi.in", "chitthi-in"),
    "fextemp-com": lambda o: tempmail_plus.generate_email("fextemp.com", "fextemp-com"),
    "any-pink": lambda o: tempmail_plus.generate_email("any.pink", "any-pink"),
    "merepost-com": lambda o: tempmail_plus.generate_email("merepost.com", "merepost-com"),
    "tempmail-lol-v2": lambda o: tempmail_lol_v2.generate_email(),
    "tempgbox": lambda o: tempgbox.generate_email(),
    "emailnator": lambda o: emailnator.generate_email(),
    "temporam": lambda o: temporam.generate_email(o.domain),
    "neighbours": lambda o: neighbours.generate_email(o.domain),
    "fmail": lambda o: fmail.generate_email(o.domain),
    "ockito": lambda o: ockito.generate_email(),
    "anonbox": lambda o: anonbox.generate_email(),
    "mailinator": lambda o: mailinator.generate_email(),
    "tempmail365": lambda o: tempmail365.generate_email(o.domain),
    "duckmail": lambda o: duckmail.generate_email(),
    "tempinbox": lambda o: tempinbox.generate_email(o.domain),
    "sharklasers": lambda o: guerrillamail_mirrors.sharklasers_generate(),
    "sharklasers-com": lambda o: guerrillamail_mirrors.sharklasers_com_generate(),
    "grr-la": lambda o: guerrillamail_mirrors.grrla_generate(),
    "grr-la-com": lambda o: guerrillamail_mirrors.grrla_com_generate(),
    "guerrillamail-info": lambda o: guerrillamail_mirrors.guerrillamail_info_generate(),
    "spam4me": lambda o: guerrillamail_mirrors.spam4me_generate(),
    "guerrillamail-net": lambda o: guerrillamail_mirrors.guerrillamail_net_generate(),
    "guerrillamail-org": lambda o: guerrillamail_mirrors.guerrillamail_org_generate(),
    "guerrillamailblock": lambda o: guerrillamail_mirrors.guerrillamailblock_generate(),
    "guerrillamail-com-www": lambda o: guerrillamail_mirrors.guerrillamail_com_www_generate(),
    "byom": lambda o: byom.generate_email(),
    "anonymmail": lambda o: anonymmail.generate_email(),
    "eyepaste": lambda o: eyepaste.generate_email(),
    "segamail": lambda o: segamail.generate_email(),
    "mail-sunls": lambda o: mail_sunls.generate_email(),
    "expressinboxhub": lambda o: expressinboxhub.generate_email(),
    "lroid": lambda o: lroid.generate_email(),
    "haribu": lambda o: haribu.generate_email(),
    "pleasenospam": lambda o: pleasenospam.generate_email(o.domain),
    "spamlessmail-org": lambda o: _with_channel(pleasenospam.generate_email("spamlessmail.org"), "spamlessmail-org"),
    "rootsh": lambda o: rootsh.generate_email(o.domain),
    "fake-email-site": lambda o: fake_email_site.generate_email(),
    "mohmal": lambda o: mohmal.generate_email(),
    "mailgolem": lambda o: mailgolem.generate_email(),
    "best-temp-mail": lambda o: best_temp_mail.generate_email(),
    "disposablemail-app": lambda o: disposablemail_app.generate_email(),
    "mailtemp-cc": lambda o: mailtemp_cc.generate_email(),
    "minuteinbox": lambda o: minuteinbox.generate_email(),
    "mailcatch": lambda o: mailcatch.generate_email(),
    "tempemail-co": lambda o: tempemail_co.generate_email(),
    "tempemails-net": lambda o: tempemails_net.generate_email(),
    "altmails": lambda o: altmails.generate_email(),
    "tempemail-info": lambda o: tempemail_info.generate_email(),
    "smailpro": lambda o: smailpro.generate_email(),
    "tempmailten": lambda o: tempmailten.generate_email(),
    "maildrop-cc": lambda o: maildrop_cc.generate_email(),
    "10minutemail-net": lambda o: tenminutemail_net.generate_email(),
    "linshiyouxiang-net": lambda o: linshiyouxiang_net.generate_email(),
    "tempmail-fyi": lambda o: tempmail_fyi.generate_email(),
    "disposablemail-com": lambda o: disposablemail_com.generate_email(),
    "tempp-mails": lambda o: tempp_mails.generate_email(),
    "emailtemp-org": lambda o: emailtemp_org.generate_email(),
}


def _generate_email_once(channel: str, options: GenerateEmailOptions) -> EmailInfo:
    """单次创建邮箱（不含重试逻辑）"""
    handler = _GENERATE_DISPATCH.get(channel)
    if handler is None:
        raise ValueError(f"Unknown channel: {channel}")
    return handler(options)


def get_emails(info: EmailInfo, options: Optional[GetEmailsOptions] = None) -> GetEmailsResult:
    """
    获取邮件列表
    Channel/Email/Token 等由 SDK 从 EmailInfo 中自动获取，用户无需手动传递

    错误处理策略:
    - 网络错误、超时、429、HTTP 5xx → 自动重试（默认 2 次）
    - 重试耗尽后返回 GetEmailsResult(success=False, emails=[])
    - 参数校验错误直接抛出异常

    示例:
        info = generate_email(GenerateEmailOptions(channel="mail-tm"))
        result = get_emails(info)
        if result.success:
            print(f"收到 {len(result.emails)} 封邮件")
    """
    if info is None:
        report_telemetry("get_emails", "", False, 0, 0, "EmailInfo is required, call generate_email() first")
        raise ValueError("EmailInfo is required, call generate_email() first")

    channel = info.channel
    email = info.email
    token = info._token
    retry = options.retry if options else None
    logger = get_logger()

    if not channel:
        report_telemetry("get_emails", "", False, 0, 0, "channel is required")
        raise ValueError("channel is required")
    if not email and channel != "tempmail-lol":
        report_telemetry("get_emails", channel, False, 0, 0, "email is required")
        raise ValueError("email is required")

    logger.debug(f"获取邮件, 渠道: {channel}, 邮箱: {email}")

    r = with_retry_with_attempts(
        lambda: _get_emails_once(channel, email, token),
        retry,
    )
    if r.ok:
        emails = r.value
        assert emails is not None
        if emails:
            logger.info(f"获取到 {len(emails)} 封邮件, 渠道: {channel}")
        else:
            logger.debug(f"暂无邮件, 渠道: {channel}")
        report_telemetry("get_emails", channel, True, r.attempts, 0, "")
        return GetEmailsResult(channel=channel, email=email, emails=emails, success=True)

    err = r.error
    err_s = str(err) if err else "unknown"
    logger.error(f"获取邮件失败, 渠道: {channel}, 错误: {err_s}")
    report_telemetry("get_emails", channel, False, r.attempts, 0, err_s)
    return GetEmailsResult(channel=channel, email=email, emails=[], success=False)


# 获取邮件分发表：渠道标识 -> 接收 (email, token) 并返回 List[Email] 的可调用对象
# 用于将原先的 if-elif 链替换为 O(1) 字典查找；token 校验通过 _require_token 保持与原逻辑一致
_GET_EMAILS_DISPATCH = {
    "tempmail": lambda e, t: tempmail.get_emails(e),
    "tempmail-cn": lambda e, t: tempmail_cn.get_emails(e),
    "ta-easy": lambda e, t: ta_easy.get_emails(e, _require_token(t, "ta-easy")),
    "linshiyou": lambda e, t: linshiyou.get_emails(_require_token(t, "linshiyou"), e),
    "mffac": lambda e, t: mffac.get_emails(e, t or ""),
    "tempmail-lol": lambda e, t: tempmail_lol.get_emails(_require_token(t, "tempmail-lol"), e),
    "chatgpt-org-uk": lambda e, t: chatgpt_org_uk.get_emails(_require_token(t, "chatgpt-org-uk"), e),
    "temp-mail-io": lambda e, t: temp_mail_io.get_emails(e),
    "mail-cx": lambda e, t: mail_cx.get_emails(_require_token(t, "mail-cx"), e),
    "ddker-com": lambda e, t: mail_cx.get_emails(_require_token(t, "mail-cx"), e),
    "catchmail": lambda e, t: catchmail.get_emails(e),
    "catchmail-mailistry": lambda e, t: catchmail.get_emails(e),
    "catchmail-zeppost": lambda e, t: catchmail.get_emails(e),
    "mailforspam": lambda e, t: mailforspam.get_emails(e),
    "mailforspam-tempmail-io": lambda e, t: mailforspam.get_emails(e),
    "mailforspam-disposable": lambda e, t: mailforspam.get_emails(e),
    "tempmailc": lambda e, t: tempmailc.get_emails(e),
    "mailnesia": lambda e, t: mailnesia.get_emails(e),
    "throwawaymail": lambda e, t: throwawaymail.get_emails(_require_token(t, "throwawaymail"), e),
    "shitty-email": lambda e, t: shitty_email.get_emails(_require_token(t, "shitty-email"), e),
    "tempmailpro": lambda e, t: tempmailpro.get_emails(_require_token(t, "tempmailpro"), e),
    "devmail-uk": lambda e, t: devmail_uk.get_emails(e),
    "cleantempmail": lambda e, t: cleantempmail.get_emails(e),
    "inboxkitten": lambda e, t: inboxkitten.get_emails(e),
    "getnada": lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    "1vpn-net": lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    "abematv-com": lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    "abematv-net": lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    "abematv-org": lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    "aceh-cc": lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    "bangkabelitung-net": lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    "cctruyen-com": lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    "getnada-com": lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    "getnada-email": lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    "getnada-net": lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    "jawatengah-net": lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    "jawatimur-net": lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    "kalimantanbarat-net": lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    "kalimantanselatan-net": lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    "kalimantantengah-net": lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    "kalimantantimur-net": lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    "kalimantanutara-net": lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    "kepulauanriau-net": lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    "luxury345-com": lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    "malukuutara-net": lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    "nusatenggarabarat-net": lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    "nusatenggaratimur-net": lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    "papuabarat-net": lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    "papuabaratdaya-net": lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    "papuaselatan-net": lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    "pehol-com": lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    "ptruyen-com": lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    "pulaubali-net": lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    "riau-net": lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    "seokey-org": lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    "sulawesibarat-net": lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    "sulawesiselatan-net": lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    "sulawesitengah-net": lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    "sulawesitenggara-net": lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    "sumaterabarat-net": lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    "sumateraselatan-net": lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    "sumaterautara-net": lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    "villatogel-com": lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    "mail123": lambda e, t: mail123.get_emails(e),
    "mail10s": lambda e, t: mail10s.get_emails(e),
    "webmailtemp": lambda e, t: webmailtemp.get_emails(_require_token(t, "webmailtemp"), e),
    "tempfastmail": lambda e, t: tempfastmail.get_emails(_require_token(t, "tempfastmail"), e),
    "1sec-mail": lambda e, t: one_sec_mail.get_emails(_require_token(t, "1sec-mail"), e),
    "fakemail": lambda e, t: fakemail.get_emails(_require_token(t, "fakemail"), e),
    "openinbox": lambda e, t: openinbox.get_emails(_require_token(t, "openinbox"), e),
    "inboxes": lambda e, t: inboxes.get_emails(e),
    "uncorreotemporal": lambda e, t: uncorreotemporal.get_emails(_require_token(t, "uncorreotemporal"), e),
    "awamail": lambda e, t: awamail.get_emails(_require_token(t, "awamail"), e),
    "mail-tm": lambda e, t: mail_tm.get_emails(_require_token(t, "mail-tm"), e),
    "web-library-net": lambda e, t: mail_tm.get_emails(_require_token(t, "mail-tm"), e),
    "dropmail": lambda e, t: dropmail.get_emails(_require_token(t, "dropmail"), e),
    "guerrillamail": lambda e, t: guerrillamail.get_emails(_require_token(t, "guerrillamail"), e),
    "guerrillamail-com": lambda e, t: guerrillamail_mirrors.guerrillamail_com_get_emails(_require_token(t, "guerrillamail-com"), e),
    "maildrop": lambda e, t: maildrop.get_emails(_require_token(t, "maildrop"), e),
    "smail-pw": lambda e, t: smail_pw.get_emails(_require_token(t, "smail-pw"), e),
    "vip-215": lambda e, t: vip_215.get_emails(_require_token(t, "vip-215"), e),
    "fake-legal": lambda e, t: fake_legal.get_emails(e),
    "imgui-de": lambda e, t: fake_legal.get_emails(e),
    "pulsewebmenu-de": lambda e, t: fake_legal.get_emails(e),
    "moakt": lambda e, t: moakt.get_emails(e, _require_token(t, "moakt")),
    "drmail-in": lambda e, t: moakt.get_emails(e, _require_token(t, "moakt")),
    "teml-net": lambda e, t: moakt.get_emails(e, _require_token(t, "moakt")),
    "tmpeml-com": lambda e, t: moakt.get_emails(e, _require_token(t, "moakt")),
    "tmpbox-net": lambda e, t: moakt.get_emails(e, _require_token(t, "moakt")),
    "moakt-cc": lambda e, t: moakt.get_emails(e, _require_token(t, "moakt")),
    "disbox-net": lambda e, t: moakt.get_emails(e, _require_token(t, "moakt")),
    "tmpmail-org": lambda e, t: moakt.get_emails(e, _require_token(t, "moakt")),
    "tmpmail-net": lambda e, t: moakt.get_emails(e, _require_token(t, "moakt")),
    "tmails-net": lambda e, t: moakt.get_emails(e, _require_token(t, "moakt")),
    "disbox-org": lambda e, t: moakt.get_emails(e, _require_token(t, "moakt")),
    "moakt-co": lambda e, t: moakt.get_emails(e, _require_token(t, "moakt")),
    "moakt-ws": lambda e, t: moakt.get_emails(e, _require_token(t, "moakt")),
    "tmail-ws": lambda e, t: moakt.get_emails(e, _require_token(t, "moakt")),
    "bareed-ws": lambda e, t: moakt.get_emails(e, _require_token(t, "moakt")),
    "10minute-one": lambda e, t: tenminute_one.get_emails(e, _require_token(t, "10minute-one")),
    "xghff-com": lambda e, t: tenminute_one.get_emails(e, _require_token(t, "10minute-one")),
    "oqqaj-com": lambda e, t: tenminute_one.get_emails(e, _require_token(t, "10minute-one")),
    "psovv-com": lambda e, t: tenminute_one.get_emails(e, _require_token(t, "10minute-one")),
    "dbwot-com": lambda e, t: tenminute_one.get_emails(e, _require_token(t, "10minute-one")),
    "ygwpr-com": lambda e, t: tenminute_one.get_emails(e, _require_token(t, "10minute-one")),
    "imxwe-com": lambda e, t: tenminute_one.get_emails(e, _require_token(t, "10minute-one")),
    "email10min": lambda e, t: email10min.get_emails(e, _require_token(t, "email10min")),
    "mjj-cm": lambda e, t: mjj_cm.get_emails(e),
    "linshi-co": lambda e, t: linshi_co.get_emails(e),
    "harakirimail": lambda e, t: harakirimail.get_emails(e),
    "jqkjqk-xyz": lambda e, t: zhujump.get_emails(_require_token(t, "jqkjqk-xyz"), e),
    "lyhlevi-com": lambda e, t: zhujump.get_emails(_require_token(t, "lyhlevi-com"), e),
    "tempmail-plus": lambda e, t: tempmail_plus.get_emails(e),
    "fexpost-com": lambda e, t: tempmail_plus.get_emails(e),
    "fexbox-org": lambda e, t: tempmail_plus.get_emails(e),
    "mailbox-in-ua": lambda e, t: tempmail_plus.get_emails(e),
    "rover-info": lambda e, t: tempmail_plus.get_emails(e),
    "chitthi-in": lambda e, t: tempmail_plus.get_emails(e),
    "fextemp-com": lambda e, t: tempmail_plus.get_emails(e),
    "any-pink": lambda e, t: tempmail_plus.get_emails(e),
    "merepost-com": lambda e, t: tempmail_plus.get_emails(e),
    "tempmail-lol-v2": lambda e, t: tempmail_lol_v2.get_emails(_require_token(t, "tempmail-lol-v2"), e),
    "tempgbox": lambda e, t: tempgbox.get_emails(_require_token(t, "tempgbox"), e),
    "emailnator": lambda e, t: emailnator.get_emails(_require_token(t, "emailnator"), e),
    "temporam": lambda e, t: temporam.get_emails(t, e),
    "neighbours": lambda e, t: neighbours.get_emails(e),
    "fmail": lambda e, t: fmail.get_emails(e),
    "ockito": lambda e, t: ockito.get_emails(
        t if t else _raise(ValueError("internal error: token missing for ockito")), e
    ),
    "anonbox": lambda e, t: anonbox.get_emails(_require_token(t, "anonbox"), e),
    "mailinator": lambda e, t: mailinator.get_emails(t or "", e),
    "tempmail365": lambda e, t: tempmail365.get_emails(e),
    "duckmail": lambda e, t: duckmail.get_emails(_require_token(t, "duckmail"), e),
    "tempinbox": lambda e, t: tempinbox.get_emails(e),
    "sharklasers": lambda e, t: guerrillamail_mirrors.sharklasers_get_emails(_require_token(t, "sharklasers"), e),
    "sharklasers-com": lambda e, t: guerrillamail_mirrors.sharklasers_com_get_emails(_require_token(t, "sharklasers-com"), e),
    "grr-la": lambda e, t: guerrillamail_mirrors.grrla_get_emails(_require_token(t, "grr-la"), e),
    "grr-la-com": lambda e, t: guerrillamail_mirrors.grrla_com_get_emails(_require_token(t, "grr-la-com"), e),
    "guerrillamail-info": lambda e, t: guerrillamail_mirrors.guerrillamail_info_get_emails(_require_token(t, "guerrillamail-info"), e),
    "spam4me": lambda e, t: guerrillamail_mirrors.spam4me_get_emails(_require_token(t, "spam4me"), e),
    "guerrillamail-net": lambda e, t: guerrillamail_mirrors.guerrillamail_net_get_emails(_require_token(t, "guerrillamail-net"), e),
    "guerrillamail-org": lambda e, t: guerrillamail_mirrors.guerrillamail_org_get_emails(_require_token(t, "guerrillamail-org"), e),
    "guerrillamailblock": lambda e, t: guerrillamail_mirrors.guerrillamailblock_get_emails(_require_token(t, "guerrillamailblock"), e),
    "guerrillamail-com-www": lambda e, t: guerrillamail_mirrors.guerrillamail_com_www_get_emails(_require_token(t, "guerrillamail-com-www"), e),
    "byom": lambda e, t: byom.get_emails(e),
    "anonymmail": lambda e, t: anonymmail.get_emails(e),
    "eyepaste": lambda e, t: eyepaste.get_emails(e),
    "segamail": lambda e, t: segamail.get_emails(e),
    "mail-sunls": lambda e, t: mail_sunls.get_emails(e),
    "expressinboxhub": lambda e, t: expressinboxhub.get_emails(_require_token(t, "expressinboxhub"), e),
    "lroid": lambda e, t: lroid.get_emails(_require_token(t, "lroid"), e),
    "haribu": lambda e, t: haribu.get_emails(_require_token(t, "haribu"), e),
    "pleasenospam": lambda e, t: pleasenospam.get_emails(e),
    "spamlessmail-org": lambda e, t: pleasenospam.get_emails(e),
    "rootsh": lambda e, t: rootsh.get_emails(_require_token(t, "rootsh"), e),
    "fake-email-site": lambda e, t: fake_email_site.get_emails(e),
    "mohmal": lambda e, t: mohmal.get_emails(e, _require_token(t, "mohmal")),
    "mailgolem": lambda e, t: mailgolem.get_emails(_require_token(t, "mailgolem"), e),
    "best-temp-mail": lambda e, t: best_temp_mail.get_emails(_require_token(t, "best-temp-mail"), e),
    "disposablemail-app": lambda e, t: disposablemail_app.get_emails(e, _require_token(t, "disposablemail-app")),
    "mailtemp-cc": lambda e, t: mailtemp_cc.get_emails(e, _require_token(t, "mailtemp-cc")),
    "minuteinbox": lambda e, t: minuteinbox.get_emails(e, _require_token(t, "minuteinbox")),
    "mailcatch": lambda e, t: mailcatch.get_emails(e, _require_token(t, "mailcatch")),
    "tempemail-co": lambda e, t: tempemail_co.get_emails(e, _require_token(t, "tempemail-co")),
    "tempemails-net": lambda e, t: tempemails_net.get_emails(e, _require_token(t, "tempemails-net")),
    "altmails": lambda e, t: altmails.get_emails(e, _require_token(t, "altmails")),
    "tempemail-info": lambda e, t: tempemail_info.get_emails(e, _require_token(t, "tempemail-info")),
    "smailpro": lambda e, t: smailpro.get_emails(e, _require_token(t, "smailpro")),
    "tempmailten": lambda e, t: tempmailten.get_emails(e, _require_token(t, "tempmailten")),
    "maildrop-cc": lambda e, t: maildrop_cc.get_emails(e, _require_token(t, "maildrop-cc")),
    "10minutemail-net": lambda e, t: tenminutemail_net.get_emails(e, _require_token(t, "10minutemail-net")),
    "linshiyouxiang-net": lambda e, t: linshiyouxiang_net.get_emails(e, _require_token(t, "linshiyouxiang-net")),
    "tempmail-fyi": lambda e, t: tempmail_fyi.get_emails(e, _require_token(t, "tempmail-fyi")),
    "disposablemail-com": lambda e, t: disposablemail_com.get_emails(e, _require_token(t, "disposablemail-com")),
    "tempp-mails": lambda e, t: tempp_mails.get_emails(e, _require_token(t, "tempp-mails")),
    "emailtemp-org": lambda e, t: emailtemp_org.get_emails(e, _require_token(t, "emailtemp-org")),
}


def _get_emails_once(channel: str, email: str, token: Optional[str]) -> List[Email]:
    """单次获取邮件（不含重试逻辑）"""
    handler = _GET_EMAILS_DISPATCH.get(channel)
    if handler is None:
        raise ValueError(f"Unknown channel: {channel}")
    return handler(email, token)


class TempEmailClient:
    """
    临时邮箱客户端
    封装了邮箱创建和邮件获取的完整流程，自动管理邮箱信息和认证令牌

    示例:
        client = TempEmailClient()
        info = client.generate(GenerateEmailOptions(channel="maildrop"))
        result = client.get_emails()
        if result.success:
            print(f"收到 {len(result.emails)} 封邮件")
    """

    def __init__(self):
        self._email_info: Optional[EmailInfo] = None

    def generate(self, options: Optional[GenerateEmailOptions] = None) -> Optional[EmailInfo]:
        """创建临时邮箱并缓存邮箱信息"""
        self._email_info = generate_email(options)
        return self._email_info

    def get_emails(self, options: Optional[GetEmailsOptions] = None) -> GetEmailsResult:
        """获取当前邮箱的邮件列表，必须先调用 generate()"""
        if self._email_info is None:
            report_telemetry("get_emails", "", False, 0, 0, "No email generated. Call generate() first")
            raise RuntimeError("No email generated. Call generate() first")

        return get_emails(self._email_info, options)

    @property
    def email_info(self) -> Optional[EmailInfo]:
        """获取当前缓存的邮箱信息"""
        return self._email_info
