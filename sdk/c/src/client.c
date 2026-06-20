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

/* 与 Go SDK allChannels / listChannels 顺序一致 */
static const tm_channel_t g_channel_try_order[] = {
    CHANNEL_TEMPMAIL,
    CHANNEL_TEMPMAIL_CN,
    CHANNEL_TA_EASY,
    CHANNEL_10MINUTE_ONE,
    CHANNEL_LINSHIYOU,
    CHANNEL_MFFAC,
    CHANNEL_TEMPMAIL_LOL,
    CHANNEL_CHATGPT_ORG_UK,
    CHANNEL_TEMP_MAIL_IO,
    CHANNEL_MAIL_CX,
    CHANNEL_CATCHMAIL,
    CHANNEL_CATCHMAIL_MAILISTRY,
    CHANNEL_CATCHMAIL_ZEPPOST,
    CHANNEL_MAILFORSPAM,
    CHANNEL_MAILFORSPAM_TEMPMAIL_IO,
    CHANNEL_MAILFORSPAM_DISPOSABLE,
    CHANNEL_TEMPMAILO,
    CHANNEL_TEMPMAILC,
    CHANNEL_MAILNESIA,
    CHANNEL_THROWAWAYMAIL,
    CHANNEL_INBOXKITTEN,
    CHANNEL_GETNADA,
    CHANNEL_MAIL123,
    CHANNEL_ONE_SEC_MAIL,
    CHANNEL_FAKEMAIL,
    CHANNEL_OPENINBOX,
    CHANNEL_INBOXES,
    CHANNEL_UNCORREOTEMPORAL,
    CHANNEL_AWAMAIL,
    CHANNEL_MAIL_TM,
    CHANNEL_DROPMAIL,
    CHANNEL_GUERRILLAMAIL,
    CHANNEL_GUERRILLAMAIL_COM,
    CHANNEL_MAILDROP,
    CHANNEL_SMAIL_PW,
    CHANNEL_VIP_215,
    CHANNEL_FAKE_LEGAL,
    CHANNEL_MOAKT,
    CHANNEL_EMAIL10MIN,
    CHANNEL_MJJ_CM,
    CHANNEL_LINSHI_CO,
    CHANNEL_HARAKIRIMAIL,
    CHANNEL_TEMPMAIL_PLUS,
    CHANNEL_MAIL_GW,
    CHANNEL_TEMPMAIL_LOL_V2,
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
};

#define TM_CHANNEL_TRY_N ((int)(sizeof(g_channel_try_order) / sizeof(g_channel_try_order[0])))

static const tm_channel_info_t g_channel_infos[] = {
    { CHANNEL_TEMPMAIL,       "TempMail",       "tempmail.ing" },
    { CHANNEL_TEMPMAIL_CN,    "TempMail CN",    "tempmail.cn" },
    { CHANNEL_TA_EASY,        "TA Easy",        "ta-easy.com" },
    { CHANNEL_10MINUTE_ONE,   "10 Minute Email", "10minutemail.one" },
    { CHANNEL_LINSHIYOU,      "临时邮",         "linshiyou.com" },
    { CHANNEL_MFFAC,          "MFFAC",          "mffac.com" },
    { CHANNEL_TEMPMAIL_LOL,   "TempMail LOL",   "tempmail.lol" },
    { CHANNEL_CHATGPT_ORG_UK, "ChatGPT Mail",   "mail.chatgpt.org.uk" },
    { CHANNEL_TEMP_MAIL_IO,   "Temp-Mail.io",   "temp-mail.io" },
    { CHANNEL_MAIL_CX,        "Mail.cx",        "mail.cx" },
    { CHANNEL_CATCHMAIL,      "Catchmail",      "catchmail.io" },
    { CHANNEL_CATCHMAIL_MAILISTRY, "Catchmail Mailistry", "mailistry.com" },
    { CHANNEL_CATCHMAIL_ZEPPOST, "Catchmail Zeppost", "zeppost.com" },
    { CHANNEL_MAILFORSPAM,    "MailForSpam",    "mailforspam.com" },
    { CHANNEL_MAILFORSPAM_TEMPMAIL_IO, "MailForSpam TempMail.io", "tempmail.io" },
    { CHANNEL_MAILFORSPAM_DISPOSABLE, "MailForSpam Disposable", "disposable.email" },
    { CHANNEL_TEMPMAILO,      "Tempmailo",      "tempmailo.com" },
    { CHANNEL_TEMPMAILC,      "TempMailC",      "tempmailc.com" },
    { CHANNEL_MAILNESIA,      "Mailnesia",      "mailnesia.com" },
    { CHANNEL_THROWAWAYMAIL,  "ThrowawayMail",  "throwawaymail.app" },
    { CHANNEL_INBOXKITTEN,    "InboxKitten",    "inboxkitten.com" },
    { CHANNEL_GETNADA,        "GetNada",        "getnada.net" },
    { CHANNEL_MAIL123,        "Mail123",        "mail123.fr" },
    { CHANNEL_ONE_SEC_MAIL,   "1SecMail",       "1sec-mail.com" },
    { CHANNEL_FAKEMAIL,       "FakeMail",       "fakemail.net" },
    { CHANNEL_OPENINBOX,      "OpenInbox",      "openinbox.io" },
    { CHANNEL_INBOXES,        "Inboxes",        "inboxes.com" },
    { CHANNEL_UNCORREOTEMPORAL, "UnCorreoTemporal", "uncorreotemporal.com" },
    { CHANNEL_AWAMAIL,        "AwaMail",        "awamail.com" },
    { CHANNEL_MAIL_TM,        "Mail.tm",        "mail.tm" },
    { CHANNEL_DROPMAIL,       "DropMail",       "dropmail.me" },
    { CHANNEL_GUERRILLAMAIL,  "Guerrilla Mail", "guerrillamail.com" },
    { CHANNEL_GUERRILLAMAIL_COM, "GuerrillaMail Root", "guerrillamail.com" },
    { CHANNEL_MAILDROP,       "Maildrop",       "maildrop.cx" },
    { CHANNEL_SMAIL_PW,       "Smail.pw",       "smail.pw" },
    { CHANNEL_VIP_215,        "VIP 215",        "vip.215.im" },
    { CHANNEL_FAKE_LEGAL,     "Fake Legal",     "fake.legal" },
    { CHANNEL_MOAKT,          "Moakt",          "moakt.com" },
    { CHANNEL_EMAIL10MIN,     "Email10Min",     "email10min.com" },
    { CHANNEL_MJJ_CM,         "MJJ Mail",       "mjj.cm" },
    { CHANNEL_LINSHI_CO,      "临时Co",         "linshi.co" },
    { CHANNEL_HARAKIRIMAIL,   "HarakiriMail",   "harakirimail.com" },
    { CHANNEL_TEMPMAIL_PLUS,  "TempMail Plus",  "tempmail.plus" },
    { CHANNEL_MAIL_GW,        "Mail.gw",        "mail.gw" },
    { CHANNEL_TEMPMAIL_LOL_V2, "TempMail LOL V2", "tempmail.lol" },
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
            case CHANNEL_MFFAC:          result = tm_provider_mffac_generate(); break;
            case CHANNEL_TA_EASY:        result = tm_provider_ta_easy_generate(); break;
            case CHANNEL_MOAKT:          result = tm_provider_moakt_generate(domain); break;
            case CHANNEL_10MINUTE_ONE:   result = tm_provider_tenminute_one_generate(domain); break;
            case CHANNEL_EMAIL10MIN:     result = tm_provider_email10min_generate(); break;
            case CHANNEL_MJJ_CM:         /* Socket.IO 渠道 C SDK 不支持 */ break;
            case CHANNEL_LINSHI_CO:      /* Socket.IO 渠道 C SDK 不支持 */ break;
            case CHANNEL_HARAKIRIMAIL:   result = tm_provider_harakirimail_generate(); break;
            case CHANNEL_TEMPMAIL_PLUS:  result = tm_provider_tempmail_plus_generate(); break;
            case CHANNEL_MAIL_GW:        result = tm_provider_mail_gw_generate(); break;
            case CHANNEL_TEMPMAIL_LOL_V2: result = tm_provider_tempmail_lol_v2_generate(); break;
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
            case CHANNEL_CATCHMAIL:      result = tm_provider_catchmail_generate(domain); break;
            case CHANNEL_CATCHMAIL_MAILISTRY: result = tm_with_channel(tm_provider_catchmail_generate("mailistry.com"), CHANNEL_CATCHMAIL_MAILISTRY); break;
            case CHANNEL_CATCHMAIL_ZEPPOST: result = tm_with_channel(tm_provider_catchmail_generate("zeppost.com"), CHANNEL_CATCHMAIL_ZEPPOST); break;
            case CHANNEL_MAILFORSPAM:    result = tm_provider_mailforspam_generate(domain); break;
            case CHANNEL_MAILFORSPAM_TEMPMAIL_IO: result = tm_with_channel(tm_provider_mailforspam_generate("tempmail.io"), CHANNEL_MAILFORSPAM_TEMPMAIL_IO); break;
            case CHANNEL_MAILFORSPAM_DISPOSABLE: result = tm_with_channel(tm_provider_mailforspam_generate("disposable.email"), CHANNEL_MAILFORSPAM_DISPOSABLE); break;
            case CHANNEL_TEMPMAILO:      result = tm_provider_tempmailo_generate(); break;
            case CHANNEL_TEMPMAILC:      result = tm_provider_tempmailc_generate(); break;
            case CHANNEL_MAILNESIA:      result = tm_provider_mailnesia_generate(); break;
            case CHANNEL_THROWAWAYMAIL:  result = tm_provider_throwawaymail_generate(); break;
            case CHANNEL_INBOXKITTEN:    result = tm_provider_inboxkitten_generate(); break;
            case CHANNEL_GETNADA:        result = tm_provider_getnada_generate(); break;
            case CHANNEL_MAIL123:        result = tm_provider_mail123_generate(); break;
            case CHANNEL_ONE_SEC_MAIL:   result = tm_provider_one_sec_mail_generate(); break;
            case CHANNEL_FAKEMAIL:       result = tm_provider_fakemail_generate(); break;
            case CHANNEL_OPENINBOX:      result = tm_provider_openinbox_generate(); break;
            case CHANNEL_INBOXES:        result = tm_provider_inboxes_generate(domain); break;
            case CHANNEL_UNCORREOTEMPORAL: result = tm_provider_uncorreotemporal_generate(); break;
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

    srand((unsigned int)time(NULL));

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
            case CHANNEL_MAIL_TM:        emails = tm_provider_mail_tm_get_emails(email_info->token, email_info->email, &count); break;
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
            case CHANNEL_TEMPMAIL_PLUS:
                emails = tm_provider_tempmail_plus_get_emails(email_info->email, &count);
                break;
            case CHANNEL_MAIL_GW:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_mail_gw_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_TEMPMAIL_LOL_V2:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_tempmail_lol_v2_get_emails(email_info->token, email_info->email, &count);
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
            case CHANNEL_TEMPMAILO:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_tempmailo_get_emails(email_info->token, email_info->email, &count);
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
            case CHANNEL_INBOXKITTEN:
                emails = tm_provider_inboxkitten_get_emails(email_info->email, &count);
                break;
            case CHANNEL_GETNADA:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_getnada_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_MAIL123:
                emails = tm_provider_mail123_get_emails(email_info->email, &count);
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
