<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail;

/**
 * 渠道标识常量类
 *
 * 暴露全部 279 个渠道 slug 常量，便于调用方以类型友好的方式引用渠道。
 * 常量值即渠道标识字符串。由 .channel_meta.json 生成。
 */
final class Channel
{
    /** TempMail（tempmail.ing） */
    public const TEMPMAIL = "tempmail";
    /** TempMail CN（tempmail.cn） */
    public const TEMPMAIL_CN = "tempmail-cn";
    /** TA Easy（ta-easy.com） */
    public const TA_EASY = "ta-easy";
    /** 10 Minute Email（10minutemail.one） */
    public const C_10MINUTE_ONE = "10minute-one";
    /** xghff.com（10minutemail.one） */
    public const XGHFF_COM = "xghff-com";
    /** oqqaj.com（10minutemail.one） */
    public const OQQAJ_COM = "oqqaj-com";
    /** psovv.com（10minutemail.one） */
    public const PSOVV_COM = "psovv-com";
    /** dbwot.com（10minutemail.one） */
    public const DBWOT_COM = "dbwot-com";
    /** ygwpr.com（10minutemail.one） */
    public const YGWPR_COM = "ygwpr-com";
    /** imxwe.com（10minutemail.one） */
    public const IMXWE_COM = "imxwe-com";
    /** 临时邮（linshiyou.com） */
    public const LINSHIYOU = "linshiyou";
    /** MFFAC（mffac.com） */
    public const MFFAC = "mffac";
    /** TempMail LOL（tempmail.lol） */
    public const TEMPMAIL_LOL = "tempmail-lol";
    /** ChatGPT Mail（mail.chatgpt.org.uk） */
    public const CHATGPT_ORG_UK = "chatgpt-org-uk";
    /** Temp-Mail.io（temp-mail.io） */
    public const TEMP_MAIL_IO = "temp-mail-io";
    /** Mail.cx（mail.cx） */
    public const MAIL_CX = "mail-cx";
    /** ddker.com（mail.cx） */
    public const DDKER_COM = "ddker-com";
    /** Catchmail（catchmail.io） */
    public const CATCHMAIL = "catchmail";
    /** Catchmail Mailistry（mailistry.com） */
    public const CATCHMAIL_MAILISTRY = "catchmail-mailistry";
    /** Catchmail Zeppost（zeppost.com） */
    public const CATCHMAIL_ZEPPOST = "catchmail-zeppost";
    /** MailForSpam（mailforspam.com） */
    public const MAILFORSPAM = "mailforspam";
    /** MailForSpam TempMail.io（tempmail.io） */
    public const MAILFORSPAM_TEMPMAIL_IO = "mailforspam-tempmail-io";
    /** MailForSpam Disposable（disposable.email） */
    public const MAILFORSPAM_DISPOSABLE = "mailforspam-disposable";
    /** TempMailC（tempmailc.com） */
    public const TEMPMAILC = "tempmailc";
    /** Mailnesia（mailnesia.com） */
    public const MAILNESIA = "mailnesia";
    /** ThrowawayMail（throwawaymail.app） */
    public const THROWAWAYMAIL = "throwawaymail";
    /** TempMail Fish（tempmail.fish） */
    public const TEMPMAIL_FISH = "tempmail-fish";
    /** Neighbours（neighbours.sh） */
    public const NEIGHBOURS_SH = "neighbours-sh";
    /** shitty.email（shitty.email） */
    public const SHITTY_EMAIL = "shitty-email";
    /** TempMail Pro（tempmailpro.us） */
    public const TEMPMAILPRO = "tempmailpro";
    /** DevMail UK（devmail.uk） */
    public const DEVMAIL_UK = "devmail-uk";
    /** InboxKitten（inboxkitten.com） */
    public const INBOXKITTEN = "inboxkitten";
    /** CleanTempMail（cleantempmail.com） */
    public const CLEANTEMPMAIL = "cleantempmail";
    /** GetNada（getnada.net） */
    public const GETNADA = "getnada";
    /** 1vpn.net（getnada.net） */
    public const C_1VPN_NET = "1vpn-net";
    /** abematv.com（getnada.net） */
    public const ABEMATV_COM = "abematv-com";
    /** abematv.net（getnada.net） */
    public const ABEMATV_NET = "abematv-net";
    /** abematv.org（getnada.net） */
    public const ABEMATV_ORG = "abematv-org";
    /** aceh.cc（getnada.net） */
    public const ACEH_CC = "aceh-cc";
    /** bangkabelitung.net（getnada.net） */
    public const BANGKABELITUNG_NET = "bangkabelitung-net";
    /** cctruyen.com（getnada.net） */
    public const CCTRUYEN_COM = "cctruyen-com";
    /** getnada.com（getnada.net） */
    public const GETNADA_COM = "getnada-com";
    /** getnada.email（getnada.net） */
    public const GETNADA_EMAIL = "getnada-email";
    /** getnada.net（getnada.net） */
    public const GETNADA_NET = "getnada-net";
    /** jawatengah.net（getnada.net） */
    public const JAWATENGAH_NET = "jawatengah-net";
    /** jawatimur.net（getnada.net） */
    public const JAWATIMUR_NET = "jawatimur-net";
    /** kalimantanbarat.net（getnada.net） */
    public const KALIMANTANBARAT_NET = "kalimantanbarat-net";
    /** kalimantanselatan.net（getnada.net） */
    public const KALIMANTANSELATAN_NET = "kalimantanselatan-net";
    /** kalimantantengah.net（getnada.net） */
    public const KALIMANTANTENGAH_NET = "kalimantantengah-net";
    /** kalimantantimur.net（getnada.net） */
    public const KALIMANTANTIMUR_NET = "kalimantantimur-net";
    /** kalimantanutara.net（getnada.net） */
    public const KALIMANTANUTARA_NET = "kalimantanutara-net";
    /** kepulauanriau.net（getnada.net） */
    public const KEPULAUANRIAU_NET = "kepulauanriau-net";
    /** luxury345.com（getnada.net） */
    public const LUXURY345_COM = "luxury345-com";
    /** malukuutara.net（getnada.net） */
    public const MALUKUUTARA_NET = "malukuutara-net";
    /** nusatenggarabarat.net（getnada.net） */
    public const NUSATENGGARABARAT_NET = "nusatenggarabarat-net";
    /** nusatenggaratimur.net（getnada.net） */
    public const NUSATENGGARATIMUR_NET = "nusatenggaratimur-net";
    /** papuabarat.net（getnada.net） */
    public const PAPUABARAT_NET = "papuabarat-net";
    /** papuabaratdaya.net（getnada.net） */
    public const PAPUABARATDAYA_NET = "papuabaratdaya-net";
    /** papuaselatan.net（getnada.net） */
    public const PAPUASELATAN_NET = "papuaselatan-net";
    /** pehol.com（getnada.net） */
    public const PEHOL_COM = "pehol-com";
    /** ptruyen.com（getnada.net） */
    public const PTRUYEN_COM = "ptruyen-com";
    /** pulaubali.net（getnada.net） */
    public const PULAUBALI_NET = "pulaubali-net";
    /** riau.net（getnada.net） */
    public const RIAU_NET = "riau-net";
    /** seokey.org（getnada.net） */
    public const SEOKEY_ORG = "seokey-org";
    /** sulawesibarat.net（getnada.net） */
    public const SULAWESIBARAT_NET = "sulawesibarat-net";
    /** sulawesiselatan.net（getnada.net） */
    public const SULAWESISELATAN_NET = "sulawesiselatan-net";
    /** sulawesitengah.net（getnada.net） */
    public const SULAWESITENGAH_NET = "sulawesitengah-net";
    /** sulawesitenggara.net（getnada.net） */
    public const SULAWESITENGGARA_NET = "sulawesitenggara-net";
    /** sumaterabarat.net（getnada.net） */
    public const SUMATERABARAT_NET = "sumaterabarat-net";
    /** sumateraselatan.net（getnada.net） */
    public const SUMATERASELATAN_NET = "sumateraselatan-net";
    /** sumaterautara.net（getnada.net） */
    public const SUMATERAUTARA_NET = "sumaterautara-net";
    /** villatogel.com（getnada.net） */
    public const VILLATOGEL_COM = "villatogel-com";
    /** Mail123（mail123.fr） */
    public const MAIL123 = "mail123";
    /** Mail10s（mail10s.com） */
    public const MAIL10S = "mail10s";
    /** WebMailTemp（webmailtemp.com） */
    public const WEBMAILTEMP = "webmailtemp";
    /** TempFastMail（tempfastmail.com） */
    public const TEMPFASTMAIL = "tempfastmail";
    /** 1SecMail（1sec-mail.com） */
    public const C_1SEC_MAIL = "1sec-mail";
    /** FakeMail（fakemail.net） */
    public const FAKEMAIL = "fakemail";
    /** OpenInbox（openinbox.io） */
    public const OPENINBOX = "openinbox";
    /** Inboxes（inboxes.com） */
    public const INBOXES = "inboxes";
    /** UnCorreoTemporal（uncorreotemporal.com） */
    public const UNCORREOTEMPORAL = "uncorreotemporal";
    /** AwaMail（awamail.com） */
    public const AWAMAIL = "awamail";
    /** Mail.tm（mail.tm） */
    public const MAIL_TM = "mail-tm";
    /** web-library.net（mail.tm） */
    public const WEB_LIBRARY_NET = "web-library-net";
    /** DropMail（dropmail.me） */
    public const DROPMAIL = "dropmail";
    /** Guerrilla Mail（guerrillamail.com） */
    public const GUERRILLAMAIL = "guerrillamail";
    /** GuerrillaMail Root（guerrillamail.com） */
    public const GUERRILLAMAIL_COM = "guerrillamail-com";
    /** Maildrop（maildrop.cx） */
    public const MAILDROP = "maildrop";
    /** Smail.pw（smail.pw） */
    public const SMAIL_PW = "smail-pw";
    /** VIP 215（vip.215.im） */
    public const VIP_215 = "vip-215";
    /** Fake Legal（fake.legal） */
    public const FAKE_LEGAL = "fake-legal";
    /** imgui.de（fake.legal） */
    public const IMGUI_DE = "imgui-de";
    /** pulsewebmenu.de（fake.legal） */
    public const PULSEWEBMENU_DE = "pulsewebmenu-de";
    /** Moakt（moakt.com） */
    public const MOAKT = "moakt";
    /** drmail.in（moakt.com） */
    public const DRMAIL_IN = "drmail-in";
    /** teml.net（moakt.com） */
    public const TEML_NET = "teml-net";
    /** tmpeml.com（moakt.com） */
    public const TMPEML_COM = "tmpeml-com";
    /** tmpbox.net（moakt.com） */
    public const TMPBOX_NET = "tmpbox-net";
    /** moakt.cc（moakt.com） */
    public const MOAKT_CC = "moakt-cc";
    /** disbox.net（moakt.com） */
    public const DISBOX_NET = "disbox-net";
    /** tmpmail.org（moakt.com） */
    public const TMPMAIL_ORG = "tmpmail-org";
    /** tmpmail.net（moakt.com） */
    public const TMPMAIL_NET = "tmpmail-net";
    /** tmails.net（moakt.com） */
    public const TMAILS_NET = "tmails-net";
    /** disbox.org（moakt.com） */
    public const DISBOX_ORG = "disbox-org";
    /** moakt.co（moakt.com） */
    public const MOAKT_CO = "moakt-co";
    /** moakt.ws（moakt.com） */
    public const MOAKT_WS = "moakt-ws";
    /** tmail.ws（moakt.com） */
    public const TMAIL_WS = "tmail-ws";
    /** bareed.ws（moakt.com） */
    public const BAREED_WS = "bareed-ws";
    /** Email10Min（email10min.com） */
    public const EMAIL10MIN = "email10min";
    /** MJJ Mail（mjj.cm） */
    public const MJJ_CM = "mjj-cm";
    /** 临时Co（linshi.co） */
    public const LINSHI_CO = "linshi-co";
    /** HarakiriMail（harakirimail.com） */
    public const HARAKIRIMAIL = "harakirimail";
    /** jqkjqk.xyz（mail.zhujump.com） */
    public const JQKJQK_XYZ = "jqkjqk-xyz";
    /** LyhLevi MoeMail（lyhlevi.com） */
    public const LYHLEVI_COM = "lyhlevi-com";
    /** TempMail Plus（tempmail.plus） */
    public const TEMPMAIL_PLUS = "tempmail-plus";
    /** fexpost.com（tempmail.plus） */
    public const FEXPOST_COM = "fexpost-com";
    /** fexbox.org（tempmail.plus） */
    public const FEXBOX_ORG = "fexbox-org";
    /** mailbox.in.ua（tempmail.plus） */
    public const MAILBOX_IN_UA = "mailbox-in-ua";
    /** rover.info（tempmail.plus） */
    public const ROVER_INFO = "rover-info";
    /** chitthi.in（tempmail.plus） */
    public const CHITTHI_IN = "chitthi-in";
    /** fextemp.com（tempmail.plus） */
    public const FEXTEMP_COM = "fextemp-com";
    /** any.pink（tempmail.plus） */
    public const ANY_PINK = "any-pink";
    /** merepost.com（tempmail.plus） */
    public const MEREPOST_COM = "merepost-com";
    /** TempMail LOL V2（tempmail.lol） */
    public const TEMPMAIL_LOL_V2 = "tempmail-lol-v2";
    /** TempGBox（tempgbox.net） */
    public const TEMPGBOX = "tempgbox";
    /** Emailnator（emailnator.com） */
    public const EMAILNATOR = "emailnator";
    /** Temporam（temporam.com） */
    public const TEMPORAM = "temporam";
    /** Neighbours（neighbours.sh） */
    public const NEIGHBOURS = "neighbours";
    /** SharkLasers（sharklasers.com） */
    public const SHARKLASERS = "sharklasers";
    /** SharkLasers Root（sharklasers.com） */
    public const SHARKLASERS_COM = "sharklasers-com";
    /** Grr.la（grr.la） */
    public const GRR_LA = "grr-la";
    /** Grr.la Root（grr.la） */
    public const GRR_LA_COM = "grr-la-com";
    /** GuerrillaMail Info（guerrillamail.info） */
    public const GUERRILLAMAIL_INFO = "guerrillamail-info";
    /** Spam4.me（spam4.me） */
    public const SPAM4ME = "spam4me";
    /** GuerrillaMail Net（guerrillamail.net） */
    public const GUERRILLAMAIL_NET = "guerrillamail-net";
    /** GuerrillaMail Org（guerrillamail.org） */
    public const GUERRILLAMAIL_ORG = "guerrillamail-org";
    /** GuerrillaMailBlock（guerrillamailblock.com） */
    public const GUERRILLAMAILBLOCK = "guerrillamailblock";
    /** GuerrillaMail WWW（guerrillamail.com） */
    public const GUERRILLAMAIL_COM_WWW = "guerrillamail-com-www";
    /** MailToYou（m2u.io） */
    public const M2U = "m2u";
    /** Tempy Email（tempy.email） */
    public const TEMPY_EMAIL = "tempy-email";
    /** FMail（fmail.men） */
    public const FMAIL = "fmail";
    /** Ockito（ockito.com） */
    public const OCKITO = "ockito";
    /** Anonbox（anonbox.net） */
    public const ANONBOX = "anonbox";
    /** DuckMail（duckmail.sbs） */
    public const DUCKMAIL = "duckmail";
    /** Mailinator（mailinator.com） */
    public const MAILINATOR = "mailinator";
    /** Tempmail365（https://tempmail365.cn） */
    public const TEMPMAIL365 = "tempmail365";
    /** TempInbox（https://www.tempinbox.xyz） */
    public const TEMPINBOX = "tempinbox";
    /** Byom（byom.de） */
    public const BYOM = "byom";
    /** AnonymMail（anonymmail.net） */
    public const ANONYMMAIL = "anonymmail";
    /** EyePaste（eyepaste.com） */
    public const EYEPASTE = "eyepaste";
    /** Mail Sunls（mail.sunls.de） */
    public const MAIL_SUNLS = "mail-sunls";
    /** ExpressInboxHub（expressinboxhub.com） */
    public const EXPRESSINBOXHUB = "expressinboxhub";
    /** Lroid（lroid.com） */
    public const LROID = "lroid";
    /** Haribu（haribu.net） */
    public const HARIBU = "haribu";
    /** Rootsh(BccTo)（rootsh.com） */
    public const ROOTSH = "rootsh";
    /** FakeEmailSite（fake-email.site） */
    public const FAKE_EMAIL_SITE = "fake-email-site";
    /** Mohmal（mohmal.com） */
    public const MOHMAL = "mohmal";
    /** MailGolem（mailgolem.com） */
    public const MAILGOLEM = "mailgolem";
    /** BestTempMail（best-temp-mail.com） */
    public const BEST_TEMP_MAIL = "best-temp-mail";
    /** DisposableMail（disposablemail.app） */
    public const DISPOSABLEMAIL_APP = "disposablemail-app";
    /** MailTemp.cc（mailtemp.cc） */
    public const MAILTEMP_CC = "mailtemp-cc";
    /** MinuteInbox（minuteinbox.com） */
    public const MINUTEINBOX = "minuteinbox";
    /** MailCatch（mailcatch.com） */
    public const MAILCATCH = "mailcatch";
    /** TempEmail.co（tempemail.co） */
    public const TEMPEMAIL_CO = "tempemail-co";
    /** TempEmails.net（tempemails.net） */
    public const TEMPEMAILS_NET = "tempemails-net";
    /** AltMails（tempmail.altmails.com） */
    public const ALTMAILS = "altmails";
    /** TempEmailInfo（tempemail.info） */
    public const TEMPEMAIL_INFO = "tempemail-info";
    /** SmailPro（smailpro.com） */
    public const SMAILPRO = "smailpro";
    /** TempMailTen（tempmailten.com） */
    public const TEMPMAILTEN = "tempmailten";
    /** MailDrop.cc（maildrop.cc） */
    public const MAILDROP_CC = "maildrop-cc";
    /** 10MinuteMail.net（10minutemail.net） */
    public const C_10MINUTEMAIL_NET = "10minutemail-net";
    /** 临时邮箱(linshiyouxiang)（linshiyouxiang.net） */
    public const LINSHIYOUXIANG_NET = "linshiyouxiang-net";
    /** Temp-Mail.fyi（temp-mail.fyi） */
    public const TEMPMAIL_FYI = "tempmail-fyi";
    /** DisposableMail.com（disposablemail.com） */
    public const DISPOSABLEMAIL_COM = "disposablemail-com";
    /** TemppMails（tempp-mails.com） */
    public const TEMPP_MAILS = "tempp-mails";
    /** EmailTemp（emailtemp.org） */
    public const EMAILTEMP_ORG = "emailtemp-org";
    /** MyTempMail.cc（mytempmail.cc） */
    public const MYTEMPMAIL_CC = "mytempmail-cc";
    /** TempMailNow（temp-mail.now） */
    public const TEMP_MAIL_NOW = "temp-mail-now";
    /** Mail.td（mail.td） */
    public const MAIL_TD = "mail-td";
    /** Mailhole.de（mailhole.de） */
    public const MAILHOLE_DE = "mailhole-de";
    /** TMail.link（tmail.link） */
    public const TMAIL_LINK = "tmail-link";
    /** 24Mail Chacuo（24mail.chacuo.net） */
    public const C_24MAIL_CHACUO = "24mail-chacuo";
    /** NiMail（nimail.cn） */
    public const NIMAIL = "nimail";
    /** FreeCustom.Email（freecustom.email） */
    public const FREECUSTOM = "freecustom";
    /** Mailmomy (16888888.cyou)（mailmomy.com） */
    public const C_16888888_CYOU = "16888888-cyou";
    /** Mailmomy (17666688.shop)（mailmomy.com） */
    public const C_17666688_SHOP = "17666688-shop";
    /** Mailmomy (282mail.com)（mailmomy.com） */
    public const C_282MAIL_COM = "282mail-com";
    /** Mailinator (blackhole.djurby.se)（mailinator.com） */
    public const BLACKHOLE_DJURBY_SE = "blackhole-djurby-se";
    /** Mailinator (block.bdea.cc)（mailinator.com） */
    public const BLOCK_BDEA_CC = "block-bdea-cc";
    /** Mailmomy (bsdu32.buzz)（mailmomy.com） */
    public const BSDU32_BUZZ = "bsdu32-buzz";
    /** Mailinator (b.smelly.cc)（mailinator.com） */
    public const B_SMELLY_CC = "b-smelly-cc";
    /** Mailinator (183carlton.changeip.net)（mailinator.com） */
    public const CARLTON183_CHANGEIP_NET = "carlton183-changeip-net";
    /** Mailinator (dea.soon.it)（mailinator.com） */
    public const DEA_SOON_IT = "dea-soon-it";
    /** Mailinator (disposable.al-sudani.com)（mailinator.com） */
    public const DISPOSABLE_AL_SUDANI_COM = "disposable-al-sudani-com";
    /** Mailinator (disposable.nogonad.nl)（mailinator.com） */
    public const DISPOSABLE_NOGONAD_NL = "disposable-nogonad-nl";
    /** Mailmomy (doxu243.buzz)（mailmomy.com） */
    public const DOXU243_BUZZ = "doxu243-buzz";
    /** Mailmomy (easyme.pro)（mailmomy.com） */
    public const EASYME_PRO = "easyme-pro";
    /** Mailinator (ebs.com.ar)（mailinator.com） */
    public const EBS_COM_AR = "ebs-com-ar";
    /** Mailinator (etgdev.de)（mailinator.com） */
    public const ETGDEV_DE = "etgdev-de";
    /** Mailmomy (evergreenco.shop)（mailmomy.com） */
    public const EVERGREENCO_SHOP = "evergreenco-shop";
    /** Mailinator (fwd2m.eszett.es)（mailinator.com） */
    public const FWD2M_ESZETT_ES = "fwd2m-eszett-es";
    /** Mailinator (jama.trenet.eu)（mailinator.com） */
    public const JAMA_TRENET_EU = "jama-trenet-eu";
    /** Mailinator (j.fairuse.org)（mailinator.com） */
    public const J_FAIRUSE_ORG = "j-fairuse-org";
    /** Mailmomy (layueming.pics)（mailmomy.com） */
    public const LAYUEMING_PICS = "layueming-pics";
    /** Mailinator (m.887.at)（mailinator.com） */
    public const M_887_AT = "m-887-at";
    /** Mailinator (m8r.davidfuhr.de)（mailinator.com） */
    public const M8R_DAVIDFUHR_DE = "m8r-davidfuhr-de";
    /** Mailinator (m8r.mcasal.com)（mailinator.com） */
    public const M8R_MCASAL_COM = "m8r-mcasal-com";
    /** Mailinator (mail.bentrask.com)（mailinator.com） */
    public const MAIL_BENTRASK_COM = "mail-bentrask-com";
    /** Mailinator (mail.fsmash.org)（mailinator.com） */
    public const MAIL_FSMASH_ORG = "mail-fsmash-org";
    /** Mailinator (mailinatorzz.mooo.com)（mailinator.com） */
    public const MAILINATORZZ_MOOO_COM = "mailinatorzz-mooo-com";
    /** Mailinator (mi.meon.be)（mailinator.com） */
    public const MI_MEON_BE = "mi-meon-be";
    /** Mailmomy (mingyuekeji.online)（mailmomy.com） */
    public const MINGYUEKEJI_ONLINE = "mingyuekeji-online";
    /** Mailmomy (mingyueming.click)（mailmomy.com） */
    public const MINGYUEMING_CLICK = "mingyueming-click";
    /** Mailmomy (mingyueming.shop)（mailmomy.com） */
    public const MINGYUEMING_SHOP = "mingyueming-shop";
    /** Mailmomy (mingyukeji.lol)（mailmomy.com） */
    public const MINGYUKEJI_LOL = "mingyukeji-lol";
    /** Mailinator (mn.curppa.com)（mailinator.com） */
    public const MN_CURPPA_COM = "mn-curppa-com";
    /** Mailinator (m.nik.me)（mailinator.com） */
    public const M_NIK_ME = "m-nik-me";
    /** Mailinator (mtmdev.com)（mailinator.com） */
    public const MTMDEV_COM = "mtmdev-com";
    /** Mailinator (nospam.thurstons.us)（mailinator.com） */
    public const NOSPAM_THURSTONS_US = "nospam-thurstons-us";
    /** Mailinator (notfond.404.mn)（mailinator.com） */
    public const NOTFOND_404_MN = "notfond-404-mn";
    /** Mailinator (null.k3vin.net)（mailinator.com） */
    public const NULL_K3VIN_NET = "null-k3vin-net";
    /** Mailmomy (nuxh62.space)（mailmomy.com） */
    public const NUXH62_SPACE = "nuxh62-space";
    /** Mailmomy (proid.cloud-ip.cc)（mailmomy.com） */
    public const PROID_CLOUD_IP_CC = "proid-cloud-ip-cc";
    /** Mailinator (ramjane.mooo.com)（mailinator.com） */
    public const RAMJANE_MOOO_COM = "ramjane-mooo-com";
    /** Mailinator (rauxa.seny.cat)（mailinator.com） */
    public const RAUXA_SENY_CAT = "rauxa-seny-cat";
    /** Mailinator (really.istrash.com)（mailinator.com） */
    public const REALLY_ISTRASH_COM = "really-istrash-com";
    /** Mailmomy (sbook.pics)（mailmomy.com） */
    public const SBOOK_PICS = "sbook-pics";
    /** Mailinator (spam.hortuk.ovh)（mailinator.com） */
    public const SPAM_HORTUK_OVH = "spam-hortuk-ovh";
    /** Mailinator (sp.woot.at)（mailinator.com） */
    public const SP_WOOT_AT = "sp-woot-at";
    /** Mailinator (test.unergie.com)（mailinator.com） */
    public const TEST_UNERGIE_COM = "test-unergie-com";
    /** Mailinator (torch.yi.org)（mailinator.com） */
    public const TORCH_YI_ORG = "torch-yi-org";
    /** Mailinator (t.zibet.net)（mailinator.com） */
    public const T_ZIBET_NET = "t-zibet-net";
    /** Mailmomy (xue32.buzz)（mailmomy.com） */
    public const XUE32_BUZZ = "xue32-buzz";
    /** ApiHz TempMail（apihz.cn） */
    public const APIHZ = "apihz";
    /** Mailinator (sogetthis.com)（mailinator.com） */
    public const SOGETTHIS_COM = "sogetthis-com";
    /** Mailinator (bobmail.info)（mailinator.com） */
    public const BOBMAIL_INFO = "bobmail-info";
    /** Mailinator (suremail.info)（mailinator.com） */
    public const SUREMAIL_INFO = "suremail-info";
    /** Mailinator (binkmail.com)（mailinator.com） */
    public const BINKMAIL_COM = "binkmail-com";
    /** Mailinator (veryrealemail.com)（mailinator.com） */
    public const VERYREALEMAIL_COM = "veryrealemail-com";
    /** Mailmomy（mailmomy.com） */
    public const MAILMOMY = "mailmomy";
    /** Mailinator (chammy.info)（mailinator.com） */
    public const CHAMMY_INFO = "chammy-info";
    /** Mailinator (thisisnotmyrealemail.com)（mailinator.com） */
    public const THISISNOTMYREALEMAIL_COM = "thisisnotmyrealemail-com";
    /** Mailinator (notmailinator.com)（mailinator.com） */
    public const NOTMAILINATOR_COM = "notmailinator-com";
    /** Mailinator (spamhereplease.com)（mailinator.com） */
    public const SPAMHEREPLEASE_COM = "spamhereplease-com";
    /** Mailinator (sendspamhere.com)（mailinator.com） */
    public const SENDSPAMHERE_COM = "sendspamhere-com";
    /** Mailinator (sendfree.org)（mailinator.com） */
    public const SENDFREE_ORG = "sendfree-org";
    /** Mailinator (junk.beats.org)（mailinator.com） */
    public const JUNK_BEATS_ORG = "junk-beats-org";
    /** Mailinator (junk.ihmehl.com)（mailinator.com） */
    public const JUNK_IHMEHL_COM = "junk-ihmehl-com";
    /** Mailinator (junk.noplay.org)（mailinator.com） */
    public const JUNK_NOPLAY_ORG = "junk-noplay-org";
    /** Mailinator (junk.vanillasystem.com)（mailinator.com） */
    public const JUNK_VANILLASYSTEM_COM = "junk-vanillasystem-com";
    /** Mailinator (spam.jasonpearce.com)（mailinator.com） */
    public const SPAM_JASONPEARCE_COM = "spam-jasonpearce-com";
    /** Mailinator (fish.skytale.net)（mailinator.com） */
    public const FISH_SKYTALE_NET = "fish-skytale-net";
    /** Mailinator (spam.mccrew.com)（mailinator.com） */
    public const SPAM_MCCREW_COM = "spam-mccrew-com";
    /** DropMail.click（dropmail.click） */
    public const DROPMAIL_CLICK = "dropmail-click";
    /** Mailinator (spam.coroiu.com)（mailinator.com） */
    public const SPAM_COROIU_COM = "spam-coroiu-com";
    /** Mailinator (spam.deluser.net)（mailinator.com） */
    public const SPAM_DELUSER_NET = "spam-deluser-net";
    /** Mailinator (spam.dhsf.net)（mailinator.com） */
    public const SPAM_DHSF_NET = "spam-dhsf-net";
    /** Mailinator (spam.lucatnt.com)（mailinator.com） */
    public const SPAM_LUCATNT_COM = "spam-lucatnt-com";
    /** Mailinator (spam.lyceum-life.com.ru)（mailinator.com） */
    public const SPAM_LYCEUM_LIFE_COM_RU = "spam-lyceum-life-com-ru";
    /** Mailinator (spam.netpirates.net)（mailinator.com） */
    public const SPAM_NETPIRATES_NET = "spam-netpirates-net";
    /** Mailinator (spam.no-ip.net)（mailinator.com） */
    public const SPAM_NO_IP_NET = "spam-no-ip-net";
    /** Mailinator (spam.ozh.org)（mailinator.com） */
    public const SPAM_OZH_ORG = "spam-ozh-org";
    /** Mailinator (spam.pyphus.org)（mailinator.com） */
    public const SPAM_PYPHUS_ORG = "spam-pyphus-org";
    /** Mailinator (spam.shep.pw)（mailinator.com） */
    public const SPAM_SHEP_PW = "spam-shep-pw";
    /** Mailinator (spam.wtf.at)（mailinator.com） */
    public const SPAM_WTF_AT = "spam-wtf-at";
    /** Mailinator (spam.wulczer.org)（mailinator.com） */
    public const SPAM_WULCZER_ORG = "spam-wulczer-org";
    /** Mailinator (crap.kakadua.net)（mailinator.com） */
    public const CRAP_KAKADUA_NET = "crap-kakadua-net";
    /** Mailinator (spam.janlugt.nl)（mailinator.com） */
    public const SPAM_JANLUGT_NL = "spam-janlugt-nl";
    /** Mailinator (min.burningfish.net)（mailinator.com） */
    public const MIN_BURNINGFISH_NET = "min-burningfish-net";
    /** Mailinator (sink.fblay.com)（mailinator.com） */
    public const SINK_FBLAY_COM = "sink-fblay-com";
    /** TempGmailer（tempgmailer.com） */
    public const TEMPGMAILER = "tempgmailer";
    /** Temp-Mail.org（temp-mail.org） */
    public const TEMP_MAIL_ORG = "temp-mail-org";
    /** XKX.me（xkx.me） */
    public const XKX_ME = "xkx-me";
    /** Gonebox Email（gonebox.email） */
    public const GONEBOX_EMAIL = "gonebox-email";
    /** Mailcat AI（mailcat.ai） */
    public const MAILCAT_AI = "mailcat-ai";
    /** TempGo Email（tempgo.email） */
    public const TEMPGO_EMAIL = "tempgo-email";
    /** Restmail.net（restmail.net） */
    public const RESTMAIL_NET = "restmail-net";
    /** DropMail.me（dropmail.me） */
    public const DROPMAIL_ME = "dropmail-me";
    /** 10MinuteMail.net（10minutemail.net） */
    public const TEN_MINUTE_MAIL_NET = "ten-minute-mail-net";
}
