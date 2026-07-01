/**
 * SDK 主入口
 */

#include "tempmail_internal.h"
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#define tm_sleep_ms(ms) Sleep(ms)
#else
#include <unistd.h>
#define tm_sleep_ms(ms) usleep((ms) * 1000)
#endif

/* 公共渠道列表顺序与 Go SDK allChannels / listChannels 一致；随机尝试顺序会在本端独立洗牌，不要求跨 SDK 一致 */
static const tm_channel_t g_channel_try_order[] = {
    CHANNEL_TEMPMAIL,
    CHANNEL_TEMPMAIL_CN,
    CHANNEL_TA_EASY,
    CHANNEL_10MINUTE_ONE,
    CHANNEL_XGHFF_COM,
    CHANNEL_OQQAJ_COM,
    CHANNEL_PSOVV_COM,
    CHANNEL_DBWOT_COM,
    CHANNEL_YGWPR_COM,
    CHANNEL_IMXWE_COM,
    CHANNEL_LINSHIYOU,
    CHANNEL_MFFAC,
    CHANNEL_TEMPMAIL_LOL,
    CHANNEL_CHATGPT_ORG_UK,
    CHANNEL_TEMP_MAIL_IO,
    CHANNEL_MAIL_CX,
    CHANNEL_DDKER_COM,
    CHANNEL_CATCHMAIL,
    CHANNEL_CATCHMAIL_MAILISTRY,
    CHANNEL_CATCHMAIL_ZEPPOST,
    CHANNEL_MAILFORSPAM,
    CHANNEL_MAILFORSPAM_TEMPMAIL_IO,
    CHANNEL_MAILFORSPAM_DISPOSABLE,
    CHANNEL_TEMPMAILC,
    CHANNEL_MAILNESIA,
    CHANNEL_THROWAWAYMAIL,
    CHANNEL_SHITTY_EMAIL,
    CHANNEL_TEMPMAILPRO,
    CHANNEL_DEVMAIL_UK,
    CHANNEL_CLEANTEMPMAIL,
    CHANNEL_INBOXKITTEN,
    CHANNEL_GETNADA,
    CHANNEL_ONE_VPN_NET,
    CHANNEL_ABEMATV_COM,
    CHANNEL_ABEMATV_NET,
    CHANNEL_ABEMATV_ORG,
    CHANNEL_ACEH_CC,
    CHANNEL_BANGKABELITUNG_NET,
    CHANNEL_CCTRUYEN_COM,
    CHANNEL_GETNADA_COM,
    CHANNEL_GETNADA_EMAIL,
    CHANNEL_GETNADA_NET,
    CHANNEL_JAWATENGAH_NET,
    CHANNEL_JAWATIMUR_NET,
    CHANNEL_KALIMANTANBARAT_NET,
    CHANNEL_KALIMANTANSELATAN_NET,
    CHANNEL_KALIMANTANTENGAH_NET,
    CHANNEL_KALIMANTANTIMUR_NET,
    CHANNEL_KALIMANTANUTARA_NET,
    CHANNEL_KEPULAUANRIAU_NET,
    CHANNEL_LUXURY345_COM,
    CHANNEL_MALUKUUTARA_NET,
    CHANNEL_NUSATENGGARABARAT_NET,
    CHANNEL_NUSATENGGARATIMUR_NET,
    CHANNEL_PAPUABARAT_NET,
    CHANNEL_PAPUABARATDAYA_NET,
    CHANNEL_PAPUASELATAN_NET,
    CHANNEL_PEHOL_COM,
    CHANNEL_PTRUYEN_COM,
    CHANNEL_PULAUBALI_NET,
    CHANNEL_RIAU_NET,
    CHANNEL_SEOKEY_ORG,
    CHANNEL_SULAWESIBARAT_NET,
    CHANNEL_SULAWESISELATAN_NET,
    CHANNEL_SULAWESITENGAH_NET,
    CHANNEL_SULAWESITENGGARA_NET,
    CHANNEL_SUMATERABARAT_NET,
    CHANNEL_SUMATERASELATAN_NET,
    CHANNEL_SUMATERAUTARA_NET,
    CHANNEL_VILLATOGEL_COM,
    CHANNEL_MAIL123,
    CHANNEL_MAIL10S,
    CHANNEL_WEBMAILTEMP,
    CHANNEL_TEMPFASTMAIL,
    CHANNEL_ONE_SEC_MAIL,
    CHANNEL_FAKEMAIL,
    CHANNEL_OPENINBOX,
    CHANNEL_INBOXES,
    CHANNEL_UNCORREOTEMPORAL,
    CHANNEL_AWAMAIL,
    CHANNEL_MAIL_TM,
    CHANNEL_WEB_LIBRARY_NET,
    CHANNEL_DROPMAIL,
    CHANNEL_GUERRILLAMAIL,
    CHANNEL_GUERRILLAMAIL_COM,
    CHANNEL_MAILDROP,
    CHANNEL_SMAIL_PW,
    CHANNEL_VIP_215,
    CHANNEL_FAKE_LEGAL,
    CHANNEL_IMGUI_DE,
    CHANNEL_PULSEWEBMENU_DE,
    CHANNEL_MOAKT,
    CHANNEL_DRMAIL_IN,
    CHANNEL_TEML_NET,
    CHANNEL_TMPEML_COM,
    CHANNEL_TMPBOX_NET,
    CHANNEL_MOAKT_CC,
    CHANNEL_DISBOX_NET,
    CHANNEL_TMPMAIL_ORG,
    CHANNEL_TMPMAIL_NET,
    CHANNEL_TMAILS_NET,
    CHANNEL_DISBOX_ORG,
    CHANNEL_MOAKT_CO,
    CHANNEL_MOAKT_WS,
    CHANNEL_TMAIL_WS,
    CHANNEL_BAREED_WS,
    CHANNEL_EMAIL10MIN,
    CHANNEL_MJJ_CM,
    CHANNEL_LINSHI_CO,
    CHANNEL_HARAKIRIMAIL,
    CHANNEL_JQKJQK_XYZ,
    CHANNEL_LYHLEVI_COM,
    CHANNEL_TEMPMAIL_PLUS,
    CHANNEL_FEXPOST_COM,
    CHANNEL_FEXBOX_ORG,
    CHANNEL_MAILBOX_IN_UA,
    CHANNEL_ROVER_INFO,
    CHANNEL_CHITTHI_IN,
    CHANNEL_FEXTEMP_COM,
    CHANNEL_ANY_PINK,
    CHANNEL_MEREPOST_COM,
    CHANNEL_TEMPMAIL_LOL_V2,
    CHANNEL_TEMPGBOX,
    CHANNEL_EMAILNATOR,
    CHANNEL_TEMPORAM,
    CHANNEL_NEIGHBOURS,
    CHANNEL_SHARKLASERS,
    CHANNEL_SHARKLASERS_COM,
    CHANNEL_GRR_LA,
    CHANNEL_GRR_LA_COM,
    CHANNEL_GUERRILLAMAIL_INFO,
    CHANNEL_SPAM4ME,
    CHANNEL_GUERRILLAMAIL_NET,
    CHANNEL_GUERRILLAMAIL_ORG,
    CHANNEL_GUERRILLAMAILBLOCK,
    CHANNEL_GUERRILLAMAIL_COM_WWW,
    CHANNEL_M2U,
    CHANNEL_TEMPY_EMAIL,
    CHANNEL_FMAIL,
    CHANNEL_OCKITO,
    CHANNEL_ANONBOX,
    CHANNEL_DUCKMAIL,
    CHANNEL_MAILINATOR,
    CHANNEL_TEMPMAIL365,
    CHANNEL_TEMPINBOX,
    CHANNEL_BYOM,
    CHANNEL_ANONYMMAIL,
    CHANNEL_EYEPASTE,
    CHANNEL_SEGAMAIL,
    CHANNEL_MAIL_SUNLS,
    CHANNEL_EXPRESSINBOXHUB,
    CHANNEL_LROID,
    CHANNEL_HARIBU,
    CHANNEL_PLEASENOSPAM,
    CHANNEL_ROOTSH,
    CHANNEL_FAKE_EMAIL_SITE,
    CHANNEL_MOHMAL,
    CHANNEL_MAILGOLEM,
    CHANNEL_BEST_TEMP_MAIL,
    CHANNEL_DISPOSABLEMAIL_APP,
    CHANNEL_MAILTEMP_CC,
    CHANNEL_MINUTEINBOX,
    CHANNEL_MAILCATCH,
    CHANNEL_TEMPEMAIL_CO,
    CHANNEL_TEMPEMAILS_NET,
    CHANNEL_ALTMAILS,
    CHANNEL_TEMPEMAIL_INFO,
    CHANNEL_SMAILPRO,
    CHANNEL_TEMPMAILTEN,
    CHANNEL_MAILDROP_CC,
    CHANNEL_TENMINUTEMAIL_NET,
    CHANNEL_LINSHIYOUXIANG_NET,
    CHANNEL_TEMPMAIL_FYI,
    CHANNEL_DISPOSABLEMAIL_COM,
    CHANNEL_TEMPP_MAILS,
    CHANNEL_EMAILTEMP_ORG,
};

#define TM_CHANNEL_TRY_N ((int)(sizeof(g_channel_try_order) / sizeof(g_channel_try_order[0])))

static void tm_seed_random_once(void) {
    static int seeded = 0;
    if (!seeded) {
        srand((unsigned int)time(NULL) ^ (unsigned int)clock());
        seeded = 1;
    }
}

static const tm_channel_info_t g_channel_infos[] = {
    { CHANNEL_TEMPMAIL,       "TempMail",       "tempmail.ing" },
    { CHANNEL_TEMPMAIL_CN,    "TempMail CN",    "tempmail.cn" },
    { CHANNEL_TA_EASY,        "TA Easy",        "ta-easy.com" },
    { CHANNEL_10MINUTE_ONE,   "10 Minute Email", "10minutemail.one" },
    { CHANNEL_XGHFF_COM,      "xghff.com",      "10minutemail.one" },
    { CHANNEL_OQQAJ_COM,      "oqqaj.com",      "10minutemail.one" },
    { CHANNEL_PSOVV_COM,      "psovv.com",      "10minutemail.one" },
    { CHANNEL_DBWOT_COM,      "dbwot.com",      "10minutemail.one" },
    { CHANNEL_YGWPR_COM,      "ygwpr.com",      "10minutemail.one" },
    { CHANNEL_IMXWE_COM,      "imxwe.com",      "10minutemail.one" },
    { CHANNEL_LINSHIYOU,      "临时邮",         "linshiyou.com" },
    { CHANNEL_MFFAC,          "MFFAC",          "mffac.com" },
    { CHANNEL_TEMPMAIL_LOL,   "TempMail LOL",   "tempmail.lol" },
    { CHANNEL_CHATGPT_ORG_UK, "ChatGPT Mail",   "mail.chatgpt.org.uk" },
    { CHANNEL_TEMP_MAIL_IO,   "Temp-Mail.io",   "temp-mail.io" },
    { CHANNEL_MAIL_CX,        "Mail.cx",        "mail.cx" },
    { CHANNEL_DDKER_COM,      "ddker.com",      "mail.cx" },
    { CHANNEL_CATCHMAIL,      "Catchmail",      "catchmail.io" },
    { CHANNEL_CATCHMAIL_MAILISTRY, "Catchmail Mailistry", "mailistry.com" },
    { CHANNEL_CATCHMAIL_ZEPPOST, "Catchmail Zeppost", "zeppost.com" },
    { CHANNEL_MAILFORSPAM,    "MailForSpam",    "mailforspam.com" },
    { CHANNEL_MAILFORSPAM_TEMPMAIL_IO, "MailForSpam TempMail.io", "tempmail.io" },
    { CHANNEL_MAILFORSPAM_DISPOSABLE, "MailForSpam Disposable", "disposable.email" },
    { CHANNEL_TEMPMAILC,      "TempMailC",      "tempmailc.com" },
    { CHANNEL_MAILNESIA,      "Mailnesia",      "mailnesia.com" },
    { CHANNEL_THROWAWAYMAIL,  "ThrowawayMail",  "throwawaymail.app" },
    { CHANNEL_SHITTY_EMAIL,   "shitty.email",   "shitty.email" },
    { CHANNEL_TEMPMAILPRO,    "TempMail Pro",   "tempmailpro.us" },
    { CHANNEL_DEVMAIL_UK,     "DevMail UK",     "devmail.uk" },
    { CHANNEL_CLEANTEMPMAIL,  "CleanTempMail",  "cleantempmail.com" },
    { CHANNEL_INBOXKITTEN,    "InboxKitten",    "inboxkitten.com" },
    { CHANNEL_GETNADA,        "GetNada",        "getnada.net" },
    { CHANNEL_ONE_VPN_NET,        "1vpn.net",        "getnada.net" },
    { CHANNEL_ABEMATV_COM,        "abematv.com",        "getnada.net" },
    { CHANNEL_ABEMATV_NET,        "abematv.net",        "getnada.net" },
    { CHANNEL_ABEMATV_ORG,        "abematv.org",        "getnada.net" },
    { CHANNEL_ACEH_CC,        "aceh.cc",        "getnada.net" },
    { CHANNEL_BANGKABELITUNG_NET,        "bangkabelitung.net",        "getnada.net" },
    { CHANNEL_CCTRUYEN_COM,        "cctruyen.com",        "getnada.net" },
    { CHANNEL_GETNADA_COM,        "getnada.com",        "getnada.net" },
    { CHANNEL_GETNADA_EMAIL,        "getnada.email",        "getnada.net" },
    { CHANNEL_GETNADA_NET,        "getnada.net",        "getnada.net" },
    { CHANNEL_JAWATENGAH_NET,        "jawatengah.net",        "getnada.net" },
    { CHANNEL_JAWATIMUR_NET,        "jawatimur.net",        "getnada.net" },
    { CHANNEL_KALIMANTANBARAT_NET,        "kalimantanbarat.net",        "getnada.net" },
    { CHANNEL_KALIMANTANSELATAN_NET,        "kalimantanselatan.net",        "getnada.net" },
    { CHANNEL_KALIMANTANTENGAH_NET,        "kalimantantengah.net",        "getnada.net" },
    { CHANNEL_KALIMANTANTIMUR_NET,        "kalimantantimur.net",        "getnada.net" },
    { CHANNEL_KALIMANTANUTARA_NET,        "kalimantanutara.net",        "getnada.net" },
    { CHANNEL_KEPULAUANRIAU_NET,        "kepulauanriau.net",        "getnada.net" },
    { CHANNEL_LUXURY345_COM,        "luxury345.com",        "getnada.net" },
    { CHANNEL_MALUKUUTARA_NET,        "malukuutara.net",        "getnada.net" },
    { CHANNEL_NUSATENGGARABARAT_NET,        "nusatenggarabarat.net",        "getnada.net" },
    { CHANNEL_NUSATENGGARATIMUR_NET,        "nusatenggaratimur.net",        "getnada.net" },
    { CHANNEL_PAPUABARAT_NET,        "papuabarat.net",        "getnada.net" },
    { CHANNEL_PAPUABARATDAYA_NET,        "papuabaratdaya.net",        "getnada.net" },
    { CHANNEL_PAPUASELATAN_NET,        "papuaselatan.net",        "getnada.net" },
    { CHANNEL_PEHOL_COM,        "pehol.com",        "getnada.net" },
    { CHANNEL_PTRUYEN_COM,        "ptruyen.com",        "getnada.net" },
    { CHANNEL_PULAUBALI_NET,        "pulaubali.net",        "getnada.net" },
    { CHANNEL_RIAU_NET,        "riau.net",        "getnada.net" },
    { CHANNEL_SEOKEY_ORG,        "seokey.org",        "getnada.net" },
    { CHANNEL_SULAWESIBARAT_NET,        "sulawesibarat.net",        "getnada.net" },
    { CHANNEL_SULAWESISELATAN_NET,        "sulawesiselatan.net",        "getnada.net" },
    { CHANNEL_SULAWESITENGAH_NET,        "sulawesitengah.net",        "getnada.net" },
    { CHANNEL_SULAWESITENGGARA_NET,        "sulawesitenggara.net",        "getnada.net" },
    { CHANNEL_SUMATERABARAT_NET,        "sumaterabarat.net",        "getnada.net" },
    { CHANNEL_SUMATERASELATAN_NET,        "sumateraselatan.net",        "getnada.net" },
    { CHANNEL_SUMATERAUTARA_NET,        "sumaterautara.net",        "getnada.net" },
    { CHANNEL_VILLATOGEL_COM,        "villatogel.com",        "getnada.net" },
    { CHANNEL_MAIL123,        "Mail123",        "mail123.fr" },
    { CHANNEL_MAIL10S,        "Mail10s",        "mail10s.com" },
    { CHANNEL_WEBMAILTEMP,    "WebMailTemp",    "webmailtemp.com" },
    { CHANNEL_TEMPFASTMAIL,   "TempFastMail",   "tempfastmail.com" },
    { CHANNEL_ONE_SEC_MAIL,   "1SecMail",       "1sec-mail.com" },
    { CHANNEL_FAKEMAIL,       "FakeMail",       "fakemail.net" },
    { CHANNEL_OPENINBOX,      "OpenInbox",      "openinbox.io" },
    { CHANNEL_INBOXES,        "Inboxes",        "inboxes.com" },
    { CHANNEL_UNCORREOTEMPORAL, "UnCorreoTemporal", "uncorreotemporal.com" },
    { CHANNEL_AWAMAIL,        "AwaMail",        "awamail.com" },
    { CHANNEL_MAIL_TM,        "Mail.tm",        "mail.tm" },
    { CHANNEL_WEB_LIBRARY_NET, "web-library.net", "mail.tm" },
    { CHANNEL_DROPMAIL,       "DropMail",       "dropmail.me" },
    { CHANNEL_GUERRILLAMAIL,  "Guerrilla Mail", "guerrillamail.com" },
    { CHANNEL_GUERRILLAMAIL_COM, "GuerrillaMail Root", "guerrillamail.com" },
    { CHANNEL_MAILDROP,       "Maildrop",       "maildrop.cx" },
    { CHANNEL_SMAIL_PW,       "Smail.pw",       "smail.pw" },
    { CHANNEL_VIP_215,        "VIP 215",        "vip.215.im" },
    { CHANNEL_FAKE_LEGAL,     "Fake Legal",     "fake.legal" },
    { CHANNEL_IMGUI_DE,       "imgui.de",       "fake.legal" },
    { CHANNEL_PULSEWEBMENU_DE, "pulsewebmenu.de", "fake.legal" },
    { CHANNEL_MOAKT,          "Moakt",          "moakt.com" },
    { CHANNEL_DRMAIL_IN,      "drmail.in",      "moakt.com" },
    { CHANNEL_TEML_NET,      "teml.net",      "moakt.com" },
    { CHANNEL_TMPEML_COM,      "tmpeml.com",      "moakt.com" },
    { CHANNEL_TMPBOX_NET,      "tmpbox.net",      "moakt.com" },
    { CHANNEL_MOAKT_CC,      "moakt.cc",      "moakt.com" },
    { CHANNEL_DISBOX_NET,      "disbox.net",      "moakt.com" },
    { CHANNEL_TMPMAIL_ORG,      "tmpmail.org",      "moakt.com" },
    { CHANNEL_TMPMAIL_NET,      "tmpmail.net",      "moakt.com" },
    { CHANNEL_TMAILS_NET,      "tmails.net",      "moakt.com" },
    { CHANNEL_DISBOX_ORG,      "disbox.org",      "moakt.com" },
    { CHANNEL_MOAKT_CO,      "moakt.co",      "moakt.com" },
    { CHANNEL_MOAKT_WS,      "moakt.ws",      "moakt.com" },
    { CHANNEL_TMAIL_WS,      "tmail.ws",      "moakt.com" },
    { CHANNEL_BAREED_WS,      "bareed.ws",      "moakt.com" },
    { CHANNEL_EMAIL10MIN,     "Email10Min",     "email10min.com" },
    { CHANNEL_MJJ_CM,         "MJJ Mail",       "mjj.cm" },
    { CHANNEL_LINSHI_CO,      "临时Co",         "linshi.co" },
    { CHANNEL_HARAKIRIMAIL,   "HarakiriMail",   "harakirimail.com" },
    { CHANNEL_JQKJQK_XYZ,     "jqkjqk.xyz",     "mail.zhujump.com" },
    { CHANNEL_LYHLEVI_COM,     "LyhLevi MoeMail", "lyhlevi.com" },
    { CHANNEL_TEMPMAIL_PLUS,  "TempMail Plus",  "tempmail.plus" },
    { CHANNEL_FEXPOST_COM,    "fexpost.com",    "tempmail.plus" },
    { CHANNEL_FEXBOX_ORG,     "fexbox.org",     "tempmail.plus" },
    { CHANNEL_MAILBOX_IN_UA,  "mailbox.in.ua",  "tempmail.plus" },
    { CHANNEL_ROVER_INFO,     "rover.info",     "tempmail.plus" },
    { CHANNEL_CHITTHI_IN,     "chitthi.in",     "tempmail.plus" },
    { CHANNEL_FEXTEMP_COM,    "fextemp.com",    "tempmail.plus" },
    { CHANNEL_ANY_PINK,       "any.pink",       "tempmail.plus" },
    { CHANNEL_MEREPOST_COM,   "merepost.com",   "tempmail.plus" },
    { CHANNEL_TEMPMAIL_LOL_V2, "TempMail LOL V2", "tempmail.lol" },
    { CHANNEL_TEMPGBOX,       "TempGBox",       "tempgbox.net" },
    { CHANNEL_EMAILNATOR,     "Emailnator",     "emailnator.com" },
    { CHANNEL_TEMPORAM,       "Temporam",       "temporam.com" },
    { CHANNEL_NEIGHBOURS,     "Neighbours",     "neighbours.sh" },
    { CHANNEL_SHARKLASERS,    "SharkLasers",    "sharklasers.com" },
    { CHANNEL_SHARKLASERS_COM, "SharkLasers Root", "sharklasers.com" },
    { CHANNEL_GRR_LA,         "Grr.la",         "grr.la" },
    { CHANNEL_GRR_LA_COM,     "Grr.la Root",    "grr.la" },
    { CHANNEL_GUERRILLAMAIL_INFO, "GuerrillaMail Info", "guerrillamail.info" },
    { CHANNEL_SPAM4ME,        "Spam4.me",       "spam4.me" },
    { CHANNEL_GUERRILLAMAIL_NET, "GuerrillaMail Net", "guerrillamail.net" },
    { CHANNEL_GUERRILLAMAIL_ORG, "GuerrillaMail Org", "guerrillamail.org" },
    { CHANNEL_GUERRILLAMAILBLOCK, "GuerrillaMailBlock", "guerrillamailblock.com" },
    { CHANNEL_GUERRILLAMAIL_COM_WWW, "GuerrillaMail WWW", "guerrillamail.com" },
    { CHANNEL_M2U,            "MailToYou",      "m2u.io" },
    { CHANNEL_TEMPY_EMAIL,    "Tempy Email",    "tempy.email" },
    { CHANNEL_FMAIL,          "FMail",          "fmail.men" },
    { CHANNEL_OCKITO,         "Ockito",         "ockito.com" },
    { CHANNEL_ANONBOX,        "Anonbox",        "anonbox.net" },
    { CHANNEL_DUCKMAIL,       "DuckMail",       "duckmail.sbs" },
    { CHANNEL_MAILINATOR,     "Mailinator",     "mailinator.com" },
    { CHANNEL_TEMPMAIL365,   "Tempmail365",     "https://tempmail365.cn" },
    { CHANNEL_TEMPINBOX,     "TempInbox",       "https://www.tempinbox.xyz" },
    { CHANNEL_BYOM,          "Byom",            "byom.de" },
    { CHANNEL_ANONYMMAIL,    "AnonymMail",      "anonymmail.net" },
    { CHANNEL_EYEPASTE,      "EyePaste",        "eyepaste.com" },
    { CHANNEL_SEGAMAIL,      "Segamail",        "segamail.com" },
    { CHANNEL_MAIL_SUNLS,    "Mail Sunls",      "mail.sunls.de" },
    { CHANNEL_EXPRESSINBOXHUB, "ExpressInboxHub", "expressinboxhub.com" },
    { CHANNEL_LROID,          "Lroid",           "lroid.com" },
    { CHANNEL_HARIBU,         "Haribu",          "haribu.net" },
    { CHANNEL_PLEASENOSPAM,   "PleeaseNoSpam",   "pleasenospam.email" },
    { CHANNEL_ROOTSH,         "Rootsh(BccTo)",    "rootsh.com" },
    { CHANNEL_FAKE_EMAIL_SITE, "FakeEmailSite",    "fake-email.site" },
    { CHANNEL_MOHMAL,         "Mohmal",            "mohmal.com" },
    { CHANNEL_MAILGOLEM,      "MailGolem",          "mailgolem.com" },
    { CHANNEL_BEST_TEMP_MAIL, "BestTempMail",       "best-temp-mail.com" },
    { CHANNEL_DISPOSABLEMAIL_APP, "DisposableMail",  "disposablemail.app" },
    { CHANNEL_MAILTEMP_CC,   "MailTemp.cc",          "mailtemp.cc" },
    { CHANNEL_MINUTEINBOX,   "MinuteInbox",          "minuteinbox.com" },
    { CHANNEL_MAILCATCH,     "MailCatch",            "mailcatch.com" },
    { CHANNEL_TEMPEMAIL_CO,  "TempEmail.co",         "tempemail.co" },
    { CHANNEL_TEMPEMAILS_NET, "TempEmails.net",      "tempemails.net" },
    { CHANNEL_ALTMAILS,      "AltMails",             "tempmail.altmails.com" },
    { CHANNEL_TEMPEMAIL_INFO, "TempEmailInfo",       "tempemail.info" },
    { CHANNEL_SMAILPRO,      "SmailPro",             "smailpro.com" },
    { CHANNEL_TEMPMAILTEN,   "TempMailTen",          "tempmailten.com" },
    { CHANNEL_MAILDROP_CC,   "MailDrop.cc",          "maildrop.cc" },
    { CHANNEL_TENMINUTEMAIL_NET, "10MinuteMail.net", "10minutemail.net" },
    { CHANNEL_LINSHIYOUXIANG_NET, "LinshiYouxiang", "linshiyouxiang.net" },
    { CHANNEL_TEMPMAIL_FYI,   "TempMail.fyi",         "temp-mail.fyi" },
    { CHANNEL_DISPOSABLEMAIL_COM, "DisposableMail.com", "disposablemail.com" },
    { CHANNEL_TEMPP_MAILS,    "TemppMails",           "tempp-mails.com" },
    { CHANNEL_EMAILTEMP_ORG,  "EmailTemp",            "emailtemp.org" },
};

const tm_channel_info_t* tm_list_channels(int *count) {
    if (count) *count = TM_CHANNEL_TRY_N;
    return g_channel_infos;
}

static tm_email_info_t* tm_with_channel(tm_email_info_t *info, tm_channel_t channel) {
    if (info) info->channel = channel;
    return info;
}

/*
 * tm_try_generate 在指定渠道上尝试创建邮箱（含重试）
 * 成功返回 EmailInfo 指针，失败返回 NULL
 */
static tm_email_info_t* tm_try_generate(tm_channel_t channel, int duration, const char *domain,
                                         const tm_retry_config_t *retry_cfg, int *out_attempts) {
    tm_retry_config_t cfg = retry_cfg ? *retry_cfg : tm_default_retry_config();
    tm_email_info_t *result = NULL;

    for (int attempt = 0; attempt <= cfg.max_retries; attempt++) {
        switch (channel) {
            case CHANNEL_TEMPMAIL:       result = tm_provider_tempmail_generate(duration); break;
            case CHANNEL_TEMPMAIL_CN:    result = tm_provider_tempmail_cn_generate(domain); break;
            case CHANNEL_LINSHIYOU:      result = tm_provider_linshiyou_generate(); break;
            case CHANNEL_TEMPMAIL_LOL:   result = tm_provider_tempmail_lol_generate(domain); break;
            case CHANNEL_CHATGPT_ORG_UK: result = tm_provider_chatgpt_org_uk_generate(); break;
            case CHANNEL_AWAMAIL:        result = tm_provider_awamail_generate(); break;
            case CHANNEL_MAIL_TM:        result = tm_provider_mail_tm_generate(); break;
            case CHANNEL_DROPMAIL:       result = tm_provider_dropmail_generate(); break;
            case CHANNEL_GUERRILLAMAIL:  result = tm_provider_guerrillamail_generate(); break;
            case CHANNEL_MAILDROP:       result = tm_provider_maildrop_generate(domain); break;
            case CHANNEL_SMAIL_PW:       result = tm_provider_smail_pw_generate(); break;
            case CHANNEL_VIP_215:        result = tm_provider_vip215_generate(); break;
            case CHANNEL_FAKE_LEGAL:     result = tm_provider_fake_legal_generate(domain); break;
            case CHANNEL_IMGUI_DE:       result = tm_with_channel(tm_provider_fake_legal_generate("imgui.de"), CHANNEL_IMGUI_DE); break;
            case CHANNEL_PULSEWEBMENU_DE: result = tm_with_channel(tm_provider_fake_legal_generate("pulsewebmenu.de"), CHANNEL_PULSEWEBMENU_DE); break;
            case CHANNEL_MFFAC:          result = tm_provider_mffac_generate(); break;
            case CHANNEL_TA_EASY:        result = tm_provider_ta_easy_generate(); break;
            case CHANNEL_MOAKT:          result = tm_provider_moakt_generate(domain); break;
            case CHANNEL_DRMAIL_IN:       result = tm_with_channel(tm_provider_moakt_generate("drmail.in"), CHANNEL_DRMAIL_IN); break;
            case CHANNEL_TEML_NET:       result = tm_with_channel(tm_provider_moakt_generate("teml.net"), CHANNEL_TEML_NET); break;
            case CHANNEL_TMPEML_COM:       result = tm_with_channel(tm_provider_moakt_generate("tmpeml.com"), CHANNEL_TMPEML_COM); break;
            case CHANNEL_TMPBOX_NET:       result = tm_with_channel(tm_provider_moakt_generate("tmpbox.net"), CHANNEL_TMPBOX_NET); break;
            case CHANNEL_MOAKT_CC:       result = tm_with_channel(tm_provider_moakt_generate("moakt.cc"), CHANNEL_MOAKT_CC); break;
            case CHANNEL_DISBOX_NET:       result = tm_with_channel(tm_provider_moakt_generate("disbox.net"), CHANNEL_DISBOX_NET); break;
            case CHANNEL_TMPMAIL_ORG:       result = tm_with_channel(tm_provider_moakt_generate("tmpmail.org"), CHANNEL_TMPMAIL_ORG); break;
            case CHANNEL_TMPMAIL_NET:       result = tm_with_channel(tm_provider_moakt_generate("tmpmail.net"), CHANNEL_TMPMAIL_NET); break;
            case CHANNEL_TMAILS_NET:       result = tm_with_channel(tm_provider_moakt_generate("tmails.net"), CHANNEL_TMAILS_NET); break;
            case CHANNEL_DISBOX_ORG:       result = tm_with_channel(tm_provider_moakt_generate("disbox.org"), CHANNEL_DISBOX_ORG); break;
            case CHANNEL_MOAKT_CO:       result = tm_with_channel(tm_provider_moakt_generate("moakt.co"), CHANNEL_MOAKT_CO); break;
            case CHANNEL_MOAKT_WS:       result = tm_with_channel(tm_provider_moakt_generate("moakt.ws"), CHANNEL_MOAKT_WS); break;
            case CHANNEL_TMAIL_WS:       result = tm_with_channel(tm_provider_moakt_generate("tmail.ws"), CHANNEL_TMAIL_WS); break;
            case CHANNEL_BAREED_WS:       result = tm_with_channel(tm_provider_moakt_generate("bareed.ws"), CHANNEL_BAREED_WS); break;
            case CHANNEL_10MINUTE_ONE:   result = tm_provider_tenminute_one_generate(domain); break;
            case CHANNEL_XGHFF_COM:      result = tm_with_channel(tm_provider_tenminute_one_generate("xghff.com"), CHANNEL_XGHFF_COM); break;
            case CHANNEL_OQQAJ_COM:      result = tm_with_channel(tm_provider_tenminute_one_generate("oqqaj.com"), CHANNEL_OQQAJ_COM); break;
            case CHANNEL_PSOVV_COM:      result = tm_with_channel(tm_provider_tenminute_one_generate("psovv.com"), CHANNEL_PSOVV_COM); break;
            case CHANNEL_DBWOT_COM:      result = tm_with_channel(tm_provider_tenminute_one_generate("dbwot.com"), CHANNEL_DBWOT_COM); break;
            case CHANNEL_YGWPR_COM:      result = tm_with_channel(tm_provider_tenminute_one_generate("ygwpr.com"), CHANNEL_YGWPR_COM); break;
            case CHANNEL_IMXWE_COM:      result = tm_with_channel(tm_provider_tenminute_one_generate("imxwe.com"), CHANNEL_IMXWE_COM); break;
            case CHANNEL_EMAIL10MIN:     result = tm_provider_email10min_generate(); break;
            case CHANNEL_MJJ_CM:         /* Socket.IO 渠道 C SDK 不支持 */ break;
            case CHANNEL_LINSHI_CO:      /* Socket.IO 渠道 C SDK 不支持 */ break;
            case CHANNEL_HARAKIRIMAIL:   result = tm_provider_harakirimail_generate(); break;
            case CHANNEL_JQKJQK_XYZ:     result = tm_provider_zhujump_generate("jqkjqk.xyz", CHANNEL_JQKJQK_XYZ); break;
            case CHANNEL_LYHLEVI_COM:    result = tm_provider_moemail_generate("https://lyhlevi.com", "lyhlevi.com", CHANNEL_LYHLEVI_COM, 24 * 60 * 60 * 1000); break;
            case CHANNEL_TEMPMAIL_PLUS:  result = tm_provider_tempmail_plus_generate(NULL, CHANNEL_TEMPMAIL_PLUS); break;
            case CHANNEL_FEXPOST_COM:    result = tm_provider_tempmail_plus_generate("fexpost.com", CHANNEL_FEXPOST_COM); break;
            case CHANNEL_FEXBOX_ORG:     result = tm_provider_tempmail_plus_generate("fexbox.org", CHANNEL_FEXBOX_ORG); break;
            case CHANNEL_MAILBOX_IN_UA:  result = tm_provider_tempmail_plus_generate("mailbox.in.ua", CHANNEL_MAILBOX_IN_UA); break;
            case CHANNEL_ROVER_INFO:     result = tm_provider_tempmail_plus_generate("rover.info", CHANNEL_ROVER_INFO); break;
            case CHANNEL_CHITTHI_IN:     result = tm_provider_tempmail_plus_generate("chitthi.in", CHANNEL_CHITTHI_IN); break;
            case CHANNEL_FEXTEMP_COM:    result = tm_provider_tempmail_plus_generate("fextemp.com", CHANNEL_FEXTEMP_COM); break;
            case CHANNEL_ANY_PINK:       result = tm_provider_tempmail_plus_generate("any.pink", CHANNEL_ANY_PINK); break;
            case CHANNEL_MEREPOST_COM:   result = tm_provider_tempmail_plus_generate("merepost.com", CHANNEL_MEREPOST_COM); break;
            case CHANNEL_TEMPMAIL_LOL_V2: result = tm_provider_tempmail_lol_v2_generate(); break;
            case CHANNEL_TEMPGBOX:       result = tm_provider_tempgbox_generate(); break;
            case CHANNEL_EMAILNATOR:     result = tm_provider_emailnator_generate(); break;
            case CHANNEL_TEMPORAM:       result = tm_provider_temporam_generate(domain); break;
            case CHANNEL_NEIGHBOURS:     result = tm_provider_neighbours_generate(domain); break;
            case CHANNEL_M2U:            result = tm_provider_m2u_generate(); break;
            case CHANNEL_TEMPY_EMAIL:    result = tm_provider_tempy_email_generate(domain); break;
            case CHANNEL_FMAIL:          result = tm_provider_fmail_generate(domain); break;
            case CHANNEL_OCKITO:         result = tm_provider_ockito_generate(); break;
            case CHANNEL_ANONBOX:        result = tm_provider_anonbox_generate(); break;
            case CHANNEL_DUCKMAIL:       result = tm_provider_duckmail_generate(); break;
            case CHANNEL_MAILINATOR:     result = tm_provider_mailinator_generate(); break;
            case CHANNEL_TEMPMAIL365:    result = tm_tempmail365_generate(domain); break;
            case CHANNEL_TEMPINBOX:      result = tm_provider_tempinbox_generate(domain); break;
            case CHANNEL_SHARKLASERS:    result = tm_provider_guerrillamail_mirror_generate(CHANNEL_SHARKLASERS, "https://www.sharklasers.com/ajax.php"); break;
            case CHANNEL_SHARKLASERS_COM: result = tm_provider_guerrillamail_mirror_generate(CHANNEL_SHARKLASERS_COM, "https://sharklasers.com/ajax.php"); break;
            case CHANNEL_GRR_LA:         result = tm_provider_guerrillamail_mirror_generate(CHANNEL_GRR_LA, "https://www.grr.la/ajax.php"); break;
            case CHANNEL_GRR_LA_COM:     result = tm_provider_guerrillamail_mirror_generate(CHANNEL_GRR_LA_COM, "https://grr.la/ajax.php"); break;
            case CHANNEL_GUERRILLAMAIL_INFO: result = tm_provider_guerrillamail_mirror_generate(CHANNEL_GUERRILLAMAIL_INFO, "https://www.guerrillamail.info/ajax.php"); break;
            case CHANNEL_GUERRILLAMAIL_COM: result = tm_provider_guerrillamail_mirror_generate(CHANNEL_GUERRILLAMAIL_COM, "https://guerrillamail.com/ajax.php"); break;
            case CHANNEL_SPAM4ME:        result = tm_provider_guerrillamail_mirror_generate(CHANNEL_SPAM4ME, "https://www.spam4.me/ajax.php"); break;
            case CHANNEL_GUERRILLAMAIL_NET: result = tm_provider_guerrillamail_mirror_generate(CHANNEL_GUERRILLAMAIL_NET, "https://www.guerrillamail.net/ajax.php"); break;
            case CHANNEL_GUERRILLAMAIL_ORG: result = tm_provider_guerrillamail_mirror_generate(CHANNEL_GUERRILLAMAIL_ORG, "https://www.guerrillamail.org/ajax.php"); break;
            case CHANNEL_GUERRILLAMAILBLOCK: result = tm_provider_guerrillamail_mirror_generate(CHANNEL_GUERRILLAMAILBLOCK, "https://www.guerrillamailblock.com/ajax.php"); break;
            case CHANNEL_GUERRILLAMAIL_COM_WWW: result = tm_provider_guerrillamail_mirror_generate(CHANNEL_GUERRILLAMAIL_COM_WWW, "https://www.guerrillamail.com/ajax.php"); break;
            case CHANNEL_TEMP_MAIL_IO:   result = tm_provider_temp_mail_io_generate(); break;
            case CHANNEL_MAIL_CX:        result = tm_provider_mail_cx_generate(domain); break;
            case CHANNEL_DDKER_COM:      result = tm_with_channel(tm_provider_mail_cx_generate("ddker.com"), CHANNEL_DDKER_COM); break;
            case CHANNEL_CATCHMAIL:      result = tm_provider_catchmail_generate(domain); break;
            case CHANNEL_CATCHMAIL_MAILISTRY: result = tm_with_channel(tm_provider_catchmail_generate("mailistry.com"), CHANNEL_CATCHMAIL_MAILISTRY); break;
            case CHANNEL_CATCHMAIL_ZEPPOST: result = tm_with_channel(tm_provider_catchmail_generate("zeppost.com"), CHANNEL_CATCHMAIL_ZEPPOST); break;
            case CHANNEL_MAILFORSPAM:    result = tm_provider_mailforspam_generate(domain); break;
            case CHANNEL_MAILFORSPAM_TEMPMAIL_IO: result = tm_with_channel(tm_provider_mailforspam_generate("tempmail.io"), CHANNEL_MAILFORSPAM_TEMPMAIL_IO); break;
            case CHANNEL_MAILFORSPAM_DISPOSABLE: result = tm_with_channel(tm_provider_mailforspam_generate("disposable.email"), CHANNEL_MAILFORSPAM_DISPOSABLE); break;
            case CHANNEL_TEMPMAILC:      result = tm_provider_tempmailc_generate(); break;
            case CHANNEL_MAILNESIA:      result = tm_provider_mailnesia_generate(); break;
            case CHANNEL_THROWAWAYMAIL:  result = tm_provider_throwawaymail_generate(); break;
            case CHANNEL_SHITTY_EMAIL:   result = tm_provider_shitty_email_generate(); break;
            case CHANNEL_TEMPMAILPRO:    result = tm_provider_tempmailpro_generate(); break;
            case CHANNEL_DEVMAIL_UK:     result = tm_provider_devmail_uk_generate(); break;
            case CHANNEL_CLEANTEMPMAIL:  result = tm_provider_cleantempmail_generate(); break;
            case CHANNEL_INBOXKITTEN:    result = tm_provider_inboxkitten_generate(); break;
            case CHANNEL_GETNADA:        result = tm_provider_getnada_generate(domain); break;
            case CHANNEL_ONE_VPN_NET: result = tm_with_channel(tm_provider_getnada_generate("1vpn.net"), CHANNEL_ONE_VPN_NET); break;
            case CHANNEL_ABEMATV_COM: result = tm_with_channel(tm_provider_getnada_generate("abematv.com"), CHANNEL_ABEMATV_COM); break;
            case CHANNEL_ABEMATV_NET: result = tm_with_channel(tm_provider_getnada_generate("abematv.net"), CHANNEL_ABEMATV_NET); break;
            case CHANNEL_ABEMATV_ORG: result = tm_with_channel(tm_provider_getnada_generate("abematv.org"), CHANNEL_ABEMATV_ORG); break;
            case CHANNEL_ACEH_CC: result = tm_with_channel(tm_provider_getnada_generate("aceh.cc"), CHANNEL_ACEH_CC); break;
            case CHANNEL_BANGKABELITUNG_NET: result = tm_with_channel(tm_provider_getnada_generate("bangkabelitung.net"), CHANNEL_BANGKABELITUNG_NET); break;
            case CHANNEL_CCTRUYEN_COM: result = tm_with_channel(tm_provider_getnada_generate("cctruyen.com"), CHANNEL_CCTRUYEN_COM); break;
            case CHANNEL_GETNADA_COM: result = tm_with_channel(tm_provider_getnada_generate("getnada.com"), CHANNEL_GETNADA_COM); break;
            case CHANNEL_GETNADA_EMAIL: result = tm_with_channel(tm_provider_getnada_generate("getnada.email"), CHANNEL_GETNADA_EMAIL); break;
            case CHANNEL_GETNADA_NET: result = tm_with_channel(tm_provider_getnada_generate("getnada.net"), CHANNEL_GETNADA_NET); break;
            case CHANNEL_JAWATENGAH_NET: result = tm_with_channel(tm_provider_getnada_generate("jawatengah.net"), CHANNEL_JAWATENGAH_NET); break;
            case CHANNEL_JAWATIMUR_NET: result = tm_with_channel(tm_provider_getnada_generate("jawatimur.net"), CHANNEL_JAWATIMUR_NET); break;
            case CHANNEL_KALIMANTANBARAT_NET: result = tm_with_channel(tm_provider_getnada_generate("kalimantanbarat.net"), CHANNEL_KALIMANTANBARAT_NET); break;
            case CHANNEL_KALIMANTANSELATAN_NET: result = tm_with_channel(tm_provider_getnada_generate("kalimantanselatan.net"), CHANNEL_KALIMANTANSELATAN_NET); break;
            case CHANNEL_KALIMANTANTENGAH_NET: result = tm_with_channel(tm_provider_getnada_generate("kalimantantengah.net"), CHANNEL_KALIMANTANTENGAH_NET); break;
            case CHANNEL_KALIMANTANTIMUR_NET: result = tm_with_channel(tm_provider_getnada_generate("kalimantantimur.net"), CHANNEL_KALIMANTANTIMUR_NET); break;
            case CHANNEL_KALIMANTANUTARA_NET: result = tm_with_channel(tm_provider_getnada_generate("kalimantanutara.net"), CHANNEL_KALIMANTANUTARA_NET); break;
            case CHANNEL_KEPULAUANRIAU_NET: result = tm_with_channel(tm_provider_getnada_generate("kepulauanriau.net"), CHANNEL_KEPULAUANRIAU_NET); break;
            case CHANNEL_LUXURY345_COM: result = tm_with_channel(tm_provider_getnada_generate("luxury345.com"), CHANNEL_LUXURY345_COM); break;
            case CHANNEL_MALUKUUTARA_NET: result = tm_with_channel(tm_provider_getnada_generate("malukuutara.net"), CHANNEL_MALUKUUTARA_NET); break;
            case CHANNEL_NUSATENGGARABARAT_NET: result = tm_with_channel(tm_provider_getnada_generate("nusatenggarabarat.net"), CHANNEL_NUSATENGGARABARAT_NET); break;
            case CHANNEL_NUSATENGGARATIMUR_NET: result = tm_with_channel(tm_provider_getnada_generate("nusatenggaratimur.net"), CHANNEL_NUSATENGGARATIMUR_NET); break;
            case CHANNEL_PAPUABARAT_NET: result = tm_with_channel(tm_provider_getnada_generate("papuabarat.net"), CHANNEL_PAPUABARAT_NET); break;
            case CHANNEL_PAPUABARATDAYA_NET: result = tm_with_channel(tm_provider_getnada_generate("papuabaratdaya.net"), CHANNEL_PAPUABARATDAYA_NET); break;
            case CHANNEL_PAPUASELATAN_NET: result = tm_with_channel(tm_provider_getnada_generate("papuaselatan.net"), CHANNEL_PAPUASELATAN_NET); break;
            case CHANNEL_PEHOL_COM: result = tm_with_channel(tm_provider_getnada_generate("pehol.com"), CHANNEL_PEHOL_COM); break;
            case CHANNEL_PTRUYEN_COM: result = tm_with_channel(tm_provider_getnada_generate("ptruyen.com"), CHANNEL_PTRUYEN_COM); break;
            case CHANNEL_PULAUBALI_NET: result = tm_with_channel(tm_provider_getnada_generate("pulaubali.net"), CHANNEL_PULAUBALI_NET); break;
            case CHANNEL_RIAU_NET: result = tm_with_channel(tm_provider_getnada_generate("riau.net"), CHANNEL_RIAU_NET); break;
            case CHANNEL_SEOKEY_ORG: result = tm_with_channel(tm_provider_getnada_generate("seokey.org"), CHANNEL_SEOKEY_ORG); break;
            case CHANNEL_SULAWESIBARAT_NET: result = tm_with_channel(tm_provider_getnada_generate("sulawesibarat.net"), CHANNEL_SULAWESIBARAT_NET); break;
            case CHANNEL_SULAWESISELATAN_NET: result = tm_with_channel(tm_provider_getnada_generate("sulawesiselatan.net"), CHANNEL_SULAWESISELATAN_NET); break;
            case CHANNEL_SULAWESITENGAH_NET: result = tm_with_channel(tm_provider_getnada_generate("sulawesitengah.net"), CHANNEL_SULAWESITENGAH_NET); break;
            case CHANNEL_SULAWESITENGGARA_NET: result = tm_with_channel(tm_provider_getnada_generate("sulawesitenggara.net"), CHANNEL_SULAWESITENGGARA_NET); break;
            case CHANNEL_SUMATERABARAT_NET: result = tm_with_channel(tm_provider_getnada_generate("sumaterabarat.net"), CHANNEL_SUMATERABARAT_NET); break;
            case CHANNEL_SUMATERASELATAN_NET: result = tm_with_channel(tm_provider_getnada_generate("sumateraselatan.net"), CHANNEL_SUMATERASELATAN_NET); break;
            case CHANNEL_SUMATERAUTARA_NET: result = tm_with_channel(tm_provider_getnada_generate("sumaterautara.net"), CHANNEL_SUMATERAUTARA_NET); break;
            case CHANNEL_VILLATOGEL_COM: result = tm_with_channel(tm_provider_getnada_generate("villatogel.com"), CHANNEL_VILLATOGEL_COM); break;
            case CHANNEL_MAIL123:        result = tm_provider_mail123_generate(); break;
            case CHANNEL_MAIL10S:        result = tm_provider_mail10s_generate(); break;
            case CHANNEL_WEBMAILTEMP:    result = tm_provider_webmailtemp_generate(); break;
            case CHANNEL_TEMPFASTMAIL:   result = tm_provider_tempfastmail_generate(); break;
            case CHANNEL_ONE_SEC_MAIL:   result = tm_provider_one_sec_mail_generate(); break;
            case CHANNEL_FAKEMAIL:       result = tm_provider_fakemail_generate(); break;
            case CHANNEL_OPENINBOX:      result = tm_provider_openinbox_generate(); break;
            case CHANNEL_INBOXES:        result = tm_provider_inboxes_generate(domain); break;
            case CHANNEL_UNCORREOTEMPORAL: result = tm_provider_uncorreotemporal_generate(); break;
            case CHANNEL_WEB_LIBRARY_NET: result = tm_with_channel(tm_provider_mail_tm_generate(), CHANNEL_WEB_LIBRARY_NET); break;
            case CHANNEL_BYOM:           result = tm_provider_byom_generate(); break;
            case CHANNEL_ANONYMMAIL:     result = tm_provider_anonymmail_generate(); break;
            case CHANNEL_EYEPASTE:       result = tm_provider_eyepaste_generate(); break;
            case CHANNEL_SEGAMAIL:       result = tm_provider_segamail_generate(); break;
            case CHANNEL_MAIL_SUNLS:     result = tm_provider_mail_sunls_generate(); break;
            case CHANNEL_EXPRESSINBOXHUB: result = tm_provider_expressinboxhub_generate(); break;
            case CHANNEL_LROID:          result = tm_provider_lroid_generate(); break;
            case CHANNEL_HARIBU:         result = tm_provider_haribu_generate(); break;
            case CHANNEL_PLEASENOSPAM:   result = tm_provider_pleasenospam_generate(); break;
            case CHANNEL_ROOTSH:         result = tm_provider_rootsh_generate(); break;
            case CHANNEL_FAKE_EMAIL_SITE: result = tm_provider_fake_email_site_generate(); break;
            case CHANNEL_MOHMAL:         result = tm_provider_mohmal_generate(); break;
            case CHANNEL_MAILGOLEM:      result = tm_provider_mailgolem_generate(); break;
            case CHANNEL_BEST_TEMP_MAIL: result = tm_provider_best_temp_mail_generate(); break;
            case CHANNEL_DISPOSABLEMAIL_APP: result = tm_provider_disposablemail_app_generate(); break;
            case CHANNEL_MAILTEMP_CC: result = tm_provider_mailtemp_cc_generate(); break;
            case CHANNEL_MINUTEINBOX: result = tm_provider_minuteinbox_generate(); break;
            case CHANNEL_MAILCATCH:  /* minuteinbox 复用渠道，暂不实现 */ break;
            case CHANNEL_TEMPEMAIL_CO: result = tm_provider_tempemail_co_generate(); break;
            case CHANNEL_TEMPEMAILS_NET: result = tm_provider_tempemails_net_generate(); break;
            case CHANNEL_ALTMAILS: result = tm_provider_altmails_generate(); break;
            case CHANNEL_TEMPEMAIL_INFO: result = tm_provider_tempemail_info_generate(); break;
            case CHANNEL_SMAILPRO: result = tm_provider_smailpro_generate(); break;
            case CHANNEL_TEMPMAILTEN: result = tm_provider_tempmailten_generate(); break;
            case CHANNEL_MAILDROP_CC: result = tm_provider_maildrop_cc_generate(); break;
            case CHANNEL_TENMINUTEMAIL_NET: result = tm_provider_tenminutemail_net_generate(); break;
            case CHANNEL_LINSHIYOUXIANG_NET: result = tm_provider_linshiyouxiang_net_generate(); break;
            case CHANNEL_TEMPMAIL_FYI: result = tm_provider_tempmail_fyi_generate(); break;
            case CHANNEL_DISPOSABLEMAIL_COM: result = tm_provider_disposablemail_com_generate(); break;
            case CHANNEL_TEMPP_MAILS: result = tm_provider_tempp_mails_generate(); break;
            case CHANNEL_EMAILTEMP_ORG: result = tm_provider_emailtemp_org_generate(); break;
            default: return NULL;
        }
        if (result) {
            if (out_attempts) *out_attempts = attempt + 1;
            return result;
        }

        if (attempt < cfg.max_retries) {
            int delay = cfg.initial_delay_ms * (1 << attempt);
            if (delay > cfg.max_delay_ms) delay = cfg.max_delay_ms;
            tm_sleep_ms(delay);
        }
    }
    if (out_attempts) *out_attempts = cfg.max_retries + 1;
    return NULL;
}

/*
 * tm_build_channel_order 构建渠道尝试顺序（Fisher-Yates 洗牌）
 * 指定渠道时优先尝试该渠道，其余渠道打乱追加
 * 未指定（CHANNEL_RANDOM）时打乱全部渠道
 * 返回值是本端运行时随机顺序，不作为跨 SDK 一致性约束
 * 调用者需 free 返回的数组
 */
static tm_channel_t* tm_build_channel_order(tm_channel_t preferred, int *out_count) {
    *out_count = TM_CHANNEL_TRY_N;
    tm_channel_t *order = (tm_channel_t*)malloc(sizeof(tm_channel_t) * (size_t)TM_CHANNEL_TRY_N);
    if (!order) return NULL;

    for (int i = 0; i < TM_CHANNEL_TRY_N; i++) order[i] = g_channel_try_order[i];

    for (int i = TM_CHANNEL_TRY_N - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        tm_channel_t tmp = order[i]; order[i] = order[j]; order[j] = tmp;
    }

    if (preferred != CHANNEL_RANDOM && preferred >= 0 && preferred < CHANNEL_COUNT) {
        for (int i = 0; i < TM_CHANNEL_TRY_N; i++) {
            if (order[i] == preferred) {
                tm_channel_t tmp = order[0]; order[0] = order[i]; order[i] = tmp;
                break;
            }
        }
    }
    return order;
}

/*
 * tm_generate_email 创建临时邮箱
 *
 * 错误处理策略:
 * - 指定渠道失败时，自动尝试其他可用渠道（打乱顺序逐个尝试）
 * - 未指定渠道时，打乱全部渠道逐个尝试，直到成功
 * - 所有渠道均不可用时返回 NULL（不崩溃）
 */
tm_email_info_t* tm_generate_email(const tm_generate_options_t *options) {
    tm_channel_t preferred = options ? options->channel : CHANNEL_RANDOM;
    int duration = (options && options->duration > 0) ? options->duration : 30;
    const char *domain = options ? options->domain : NULL;
    const tm_retry_config_t *retry_cfg = options ? options->retry : NULL;

    tm_seed_random_once();

    int count = 0;
    tm_channel_t *try_order = tm_build_channel_order(preferred, &count);
    if (!try_order) return NULL;

    int channels_tried = 0;
    for (int i = 0; i < count; i++) {
        channels_tried++;
        tm_channel_t ch = try_order[i];
        TM_LOG_INF("创建临时邮箱, 渠道: %s", tm_channel_name(ch));

        int attempts = 0;
        tm_email_info_t *result = tm_try_generate(ch, duration, domain, retry_cfg, &attempts);
        if (result) {
            TM_LOG_INF("邮箱创建成功: %s (渠道: %s)", result->email, tm_channel_name(ch));
            tm_telemetry_report("generate_email", tm_channel_name(ch), true, attempts, channels_tried, NULL);
            free(try_order);
            return result;
        }

        TM_LOG_WARN("渠道 %s 不可用，尝试下一个渠道", tm_channel_name(ch));
    }

    free(try_order);
    TM_LOG_ERR("所有渠道均不可用，创建邮箱失败");
    tm_telemetry_report("generate_email", "", false, 0, channels_tried, "all channels failed");
    return NULL;
}

tm_get_emails_result_t* tm_get_emails(const tm_email_info_t *email_info, const tm_get_emails_options_t *options) {
    if (!email_info) {
        tm_telemetry_report("get_emails", "", false, 0, 0, "email_info is NULL");
        return NULL;
    }

    tm_get_emails_result_t *result = (tm_get_emails_result_t*)calloc(1, sizeof(tm_get_emails_result_t));
    if (!result) return NULL;

    result->channel = email_info->channel;
    result->email = tm_strdup(email_info->email);

    TM_LOG_DBG("获取邮件, 渠道: %s, 邮箱: %s", tm_channel_name(email_info->channel), email_info->email);

    tm_retry_config_t cfg = (options && options->retry) ? *options->retry : tm_default_retry_config();
    int count = 0;
    tm_email_t *emails = NULL;

    for (int attempt = 0; attempt <= cfg.max_retries; attempt++) {
        switch (email_info->channel) {
            case CHANNEL_TEMPMAIL:       emails = tm_provider_tempmail_get_emails(email_info->email, &count); break;
            case CHANNEL_TEMPMAIL_CN:    emails = tm_provider_tempmail_cn_get_emails(email_info->email, &count); break;
            case CHANNEL_LINSHIYOU:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_linshiyou_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_TEMPMAIL_LOL:   emails = tm_provider_tempmail_lol_get_emails(email_info->token, email_info->email, &count); break;
            case CHANNEL_CHATGPT_ORG_UK: emails = tm_provider_chatgpt_org_uk_get_emails(email_info->token, email_info->email, &count); break;
            case CHANNEL_AWAMAIL:        emails = tm_provider_awamail_get_emails(email_info->token, email_info->email, &count); break;
            case CHANNEL_MAIL_TM:
            case CHANNEL_WEB_LIBRARY_NET:
                emails = tm_provider_mail_tm_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_DROPMAIL:       emails = tm_provider_dropmail_get_emails(email_info->token, email_info->email, &count); break;
            case CHANNEL_GUERRILLAMAIL:  emails = tm_provider_guerrillamail_get_emails(email_info->token, email_info->email, &count); break;
            case CHANNEL_MAILDROP:       emails = tm_provider_maildrop_get_emails(email_info->token, email_info->email, &count); break;
            case CHANNEL_SMAIL_PW:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_smail_pw_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_VIP_215:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_vip215_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_FAKE_LEGAL:
            case CHANNEL_IMGUI_DE:
            case CHANNEL_PULSEWEBMENU_DE:
                emails = tm_provider_fake_legal_get_emails(email_info->email, &count);
                break;
            case CHANNEL_MFFAC:
                emails = tm_provider_mffac_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_TA_EASY:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_ta_easy_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_MOAKT:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_moakt_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_10MINUTE_ONE:
            case CHANNEL_XGHFF_COM:
            case CHANNEL_OQQAJ_COM:
            case CHANNEL_PSOVV_COM:
            case CHANNEL_DBWOT_COM:
            case CHANNEL_YGWPR_COM:
            case CHANNEL_IMXWE_COM:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_tenminute_one_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_EMAIL10MIN:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_email10min_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_MJJ_CM:         /* Socket.IO 渠道 C SDK 不支持 */ count = -1; break;
            case CHANNEL_LINSHI_CO:      /* Socket.IO 渠道 C SDK 不支持 */ count = -1; break;
            case CHANNEL_HARAKIRIMAIL:
                emails = tm_provider_harakirimail_get_emails(email_info->email, &count);
                break;
            case CHANNEL_JQKJQK_XYZ:
            case CHANNEL_LYHLEVI_COM:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_zhujump_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_TEMPMAIL_PLUS:
            case CHANNEL_FEXPOST_COM:
            case CHANNEL_FEXBOX_ORG:
            case CHANNEL_MAILBOX_IN_UA:
            case CHANNEL_ROVER_INFO:
            case CHANNEL_CHITTHI_IN:
            case CHANNEL_FEXTEMP_COM:
            case CHANNEL_ANY_PINK:
            case CHANNEL_MEREPOST_COM:
                emails = tm_provider_tempmail_plus_get_emails(email_info->email, &count);
                break;
            case CHANNEL_TEMPMAIL_LOL_V2:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_tempmail_lol_v2_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_TEMPGBOX:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_tempgbox_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_EMAILNATOR:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_emailnator_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_TEMPORAM:
                emails = tm_provider_temporam_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_NEIGHBOURS:
                emails = tm_provider_neighbours_get_emails(email_info->email, &count);
                break;
            case CHANNEL_M2U:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_m2u_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_TEMPY_EMAIL:
                emails = tm_provider_tempy_email_get_emails(email_info->email, &count);
                break;
            case CHANNEL_FMAIL:
                emails = tm_provider_fmail_get_emails(email_info->email, &count);
                break;
            case CHANNEL_OCKITO:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_ockito_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_ANONBOX:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_anonbox_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_DUCKMAIL:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_duckmail_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_MAILINATOR:
                emails = tm_provider_mailinator_get_emails(email_info->email, &count);
                break;
            case CHANNEL_TEMPMAIL365:
                emails = tm_tempmail365_get_emails(email_info->email, &count);
                break;
            case CHANNEL_TEMPINBOX:
                emails = tm_provider_tempinbox_get_emails(email_info->email, &count);
                break;
            case CHANNEL_SHARKLASERS:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_guerrillamail_mirror_get_emails("https://www.sharklasers.com/ajax.php", email_info->token, email_info->email, &count);
                break;
            case CHANNEL_SHARKLASERS_COM:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_guerrillamail_mirror_get_emails("https://sharklasers.com/ajax.php", email_info->token, email_info->email, &count);
                break;
            case CHANNEL_GRR_LA:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_guerrillamail_mirror_get_emails("https://www.grr.la/ajax.php", email_info->token, email_info->email, &count);
                break;
            case CHANNEL_GRR_LA_COM:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_guerrillamail_mirror_get_emails("https://grr.la/ajax.php", email_info->token, email_info->email, &count);
                break;
            case CHANNEL_GUERRILLAMAIL_INFO:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_guerrillamail_mirror_get_emails("https://www.guerrillamail.info/ajax.php", email_info->token, email_info->email, &count);
                break;
            case CHANNEL_GUERRILLAMAIL_COM:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_guerrillamail_mirror_get_emails("https://guerrillamail.com/ajax.php", email_info->token, email_info->email, &count);
                break;
            case CHANNEL_SPAM4ME:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_guerrillamail_mirror_get_emails("https://www.spam4.me/ajax.php", email_info->token, email_info->email, &count);
                break;
            case CHANNEL_GUERRILLAMAIL_NET:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_guerrillamail_mirror_get_emails("https://www.guerrillamail.net/ajax.php", email_info->token, email_info->email, &count);
                break;
            case CHANNEL_GUERRILLAMAIL_ORG:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_guerrillamail_mirror_get_emails("https://www.guerrillamail.org/ajax.php", email_info->token, email_info->email, &count);
                break;
            case CHANNEL_GUERRILLAMAILBLOCK:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_guerrillamail_mirror_get_emails("https://www.guerrillamailblock.com/ajax.php", email_info->token, email_info->email, &count);
                break;
            case CHANNEL_GUERRILLAMAIL_COM_WWW:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_guerrillamail_mirror_get_emails("https://www.guerrillamail.com/ajax.php", email_info->token, email_info->email, &count);
                break;
            case CHANNEL_TEMP_MAIL_IO:
                emails = tm_provider_temp_mail_io_get_emails(email_info->email, &count);
                break;
            case CHANNEL_MAIL_CX:
            case CHANNEL_DDKER_COM:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_mail_cx_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_CATCHMAIL:
                emails = tm_provider_catchmail_get_emails(email_info->email, &count);
                break;
            case CHANNEL_CATCHMAIL_MAILISTRY:
                emails = tm_provider_catchmail_get_emails(email_info->email, &count);
                break;
            case CHANNEL_CATCHMAIL_ZEPPOST:
                emails = tm_provider_catchmail_get_emails(email_info->email, &count);
                break;
            case CHANNEL_MAILFORSPAM:
                emails = tm_provider_mailforspam_get_emails(email_info->email, &count);
                break;
            case CHANNEL_MAILFORSPAM_TEMPMAIL_IO:
                emails = tm_provider_mailforspam_get_emails(email_info->email, &count);
                break;
            case CHANNEL_MAILFORSPAM_DISPOSABLE:
                emails = tm_provider_mailforspam_get_emails(email_info->email, &count);
                break;
            case CHANNEL_TEMPMAILC:
                emails = tm_provider_tempmailc_get_emails(email_info->email, &count);
                break;
            case CHANNEL_MAILNESIA:
                emails = tm_provider_mailnesia_get_emails(email_info->email, &count);
                break;
            case CHANNEL_THROWAWAYMAIL:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_throwawaymail_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_SHITTY_EMAIL:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_shitty_email_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_TEMPMAILPRO:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_tempmailpro_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_DEVMAIL_UK:
                emails = tm_provider_devmail_uk_get_emails(email_info->email, &count);
                break;
            case CHANNEL_CLEANTEMPMAIL:
                emails = tm_provider_cleantempmail_get_emails(email_info->email, &count);
                break;
            case CHANNEL_INBOXKITTEN:
                emails = tm_provider_inboxkitten_get_emails(email_info->email, &count);
                break;
            case CHANNEL_GETNADA:
            case CHANNEL_ONE_VPN_NET:
            case CHANNEL_ABEMATV_COM:
            case CHANNEL_ABEMATV_NET:
            case CHANNEL_ABEMATV_ORG:
            case CHANNEL_ACEH_CC:
            case CHANNEL_BANGKABELITUNG_NET:
            case CHANNEL_CCTRUYEN_COM:
            case CHANNEL_GETNADA_COM:
            case CHANNEL_GETNADA_EMAIL:
            case CHANNEL_GETNADA_NET:
            case CHANNEL_JAWATENGAH_NET:
            case CHANNEL_JAWATIMUR_NET:
            case CHANNEL_KALIMANTANBARAT_NET:
            case CHANNEL_KALIMANTANSELATAN_NET:
            case CHANNEL_KALIMANTANTENGAH_NET:
            case CHANNEL_KALIMANTANTIMUR_NET:
            case CHANNEL_KALIMANTANUTARA_NET:
            case CHANNEL_KEPULAUANRIAU_NET:
            case CHANNEL_LUXURY345_COM:
            case CHANNEL_MALUKUUTARA_NET:
            case CHANNEL_NUSATENGGARABARAT_NET:
            case CHANNEL_NUSATENGGARATIMUR_NET:
            case CHANNEL_PAPUABARAT_NET:
            case CHANNEL_PAPUABARATDAYA_NET:
            case CHANNEL_PAPUASELATAN_NET:
            case CHANNEL_PEHOL_COM:
            case CHANNEL_PTRUYEN_COM:
            case CHANNEL_PULAUBALI_NET:
            case CHANNEL_RIAU_NET:
            case CHANNEL_SEOKEY_ORG:
            case CHANNEL_SULAWESIBARAT_NET:
            case CHANNEL_SULAWESISELATAN_NET:
            case CHANNEL_SULAWESITENGAH_NET:
            case CHANNEL_SULAWESITENGGARA_NET:
            case CHANNEL_SUMATERABARAT_NET:
            case CHANNEL_SUMATERASELATAN_NET:
            case CHANNEL_SUMATERAUTARA_NET:
            case CHANNEL_VILLATOGEL_COM:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_getnada_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_MAIL123:
                emails = tm_provider_mail123_get_emails(email_info->email, &count);
                break;
            case CHANNEL_MAIL10S:
                emails = tm_provider_mail10s_get_emails(email_info->email, &count);
                break;
            case CHANNEL_WEBMAILTEMP:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_webmailtemp_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_TEMPFASTMAIL:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_tempfastmail_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_ONE_SEC_MAIL:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_one_sec_mail_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_FAKEMAIL:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_fakemail_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_OPENINBOX:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_openinbox_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_INBOXES:
                emails = tm_provider_inboxes_get_emails(email_info->email, &count);
                break;
            case CHANNEL_UNCORREOTEMPORAL:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_uncorreotemporal_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_BYOM:
                emails = tm_provider_byom_get_emails(email_info->email, &count);
                break;
            case CHANNEL_ANONYMMAIL:
                emails = tm_provider_anonymmail_get_emails(email_info->email, &count);
                break;
            case CHANNEL_EYEPASTE:
                emails = tm_provider_eyepaste_get_emails(email_info->email, &count);
                break;
            case CHANNEL_SEGAMAIL:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_segamail_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_MAIL_SUNLS:
                emails = tm_provider_mail_sunls_get_emails(email_info->email, &count);
                break;
            case CHANNEL_EXPRESSINBOXHUB:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_expressinboxhub_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_LROID:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_lroid_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_HARIBU:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_haribu_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_PLEASENOSPAM:
                emails = tm_provider_pleasenospam_get_emails(email_info->email, &count);
                break;
            case CHANNEL_ROOTSH:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_rootsh_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_FAKE_EMAIL_SITE:
                emails = tm_provider_fake_email_site_get_emails(email_info->email, &count);
                break;
            case CHANNEL_MOHMAL:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_mohmal_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_MAILGOLEM:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_mailgolem_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_BEST_TEMP_MAIL:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_best_temp_mail_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_DISPOSABLEMAIL_APP:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_disposablemail_app_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_MAILTEMP_CC:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_mailtemp_cc_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_MINUTEINBOX:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_minuteinbox_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_MAILCATCH:    /* 暂不实现 */ count = -1; break;
            case CHANNEL_TEMPEMAIL_CO:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_tempemail_co_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_TEMPEMAILS_NET:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_tempemails_net_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_ALTMAILS:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_altmails_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_TEMPEMAIL_INFO:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_tempemail_info_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_SMAILPRO:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_smailpro_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_TEMPMAILTEN:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_tempmailten_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_MAILDROP_CC:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_maildrop_cc_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_TENMINUTEMAIL_NET:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_tenminutemail_net_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_LINSHIYOUXIANG_NET:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_linshiyouxiang_net_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_TEMPMAIL_FYI:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_tempmail_fyi_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_DISPOSABLEMAIL_COM:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_disposablemail_com_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_TEMPP_MAILS:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_tempp_mails_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_EMAILTEMP_ORG:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_emailtemp_org_get_emails(email_info->token, email_info->email, &count);
                break;
            default: break;
        }

        /* count >= 0 表示请求成功（即使邮件为空） */
        if (count >= 0 && (emails || count == 0)) {
            if (attempt > 0) TM_LOG_INF("第 %d 次尝试成功", attempt + 1);
            result->emails = emails;
            result->email_count = count;
            result->success = true;
            if (count > 0) {
                TM_LOG_INF("获取到 %d 封邮件, 渠道: %s", count, tm_channel_name(email_info->channel));
            } else {
                TM_LOG_DBG("暂无邮件, 渠道: %s", tm_channel_name(email_info->channel));
            }
            tm_telemetry_report("get_emails", tm_channel_name(email_info->channel), true, attempt + 1, 0, NULL);
            return result;
        }

        if (attempt < cfg.max_retries) {
            int delay = cfg.initial_delay_ms * (1 << attempt);
            if (delay > cfg.max_delay_ms) delay = cfg.max_delay_ms;
            TM_LOG_WARN("请求失败，%dms 后第 %d 次重试...", delay, attempt + 2);
            tm_sleep_ms(delay);
        }
    }

    TM_LOG_ERR("获取邮件失败, 渠道: %s", tm_channel_name(email_info->channel));
    result->success = false;
    result->error = tm_strdup("重试耗尽后仍失败");
    tm_telemetry_report("get_emails", tm_channel_name(email_info->channel), false, cfg.max_retries + 1, 0,
        result->error);
    return result;
}
