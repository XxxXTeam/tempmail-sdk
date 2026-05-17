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
    CHANNEL_TMPMAILS,
    CHANNEL_TA_EASY,
    CHANNEL_10MINUTE_ONE,
    CHANNEL_LINSHIYOU,
    CHANNEL_MFFAC,
    CHANNEL_TEMPMAIL_LOL,
    CHANNEL_CHATGPT_ORG_UK,
    CHANNEL_AWAMAIL,
    CHANNEL_MAIL_TM,
    CHANNEL_DROPMAIL,
    CHANNEL_GUERRILLAMAIL,
    CHANNEL_MAILDROP,
    CHANNEL_SMAIL_PW,
    CHANNEL_BOOMLIFY,
    CHANNEL_MINMAIL,
    CHANNEL_VIP_215,
    CHANNEL_ANONBOX,
    CHANNEL_FAKE_LEGAL,
    CHANNEL_MOAKT,
    CHANNEL_ETEMPMAIL,
    CHANNEL_24MAIL_CHACUO,
    CHANNEL_EMAIL10MIN,
    CHANNEL_MJJ_CM,
    CHANNEL_MAIL_XIUVI,
    CHANNEL_LINSHI_CO,
    CHANNEL_HARAKIRIMAIL,
    CHANNEL_TEMPMAIL_PLUS,
    CHANNEL_MAIL_GW,
    CHANNEL_TEMPMAIL_LOL_V2,
    CHANNEL_SHARKLASERS,
    CHANNEL_GRR_LA,
    CHANNEL_GUERRILLAMAIL_INFO,
};

#define TM_CHANNEL_TRY_N ((int)(sizeof(g_channel_try_order) / sizeof(g_channel_try_order[0])))

static const tm_channel_info_t g_channel_infos[] = {
    { CHANNEL_TEMPMAIL,       "TempMail",       "tempmail.ing" },
    { CHANNEL_TEMPMAIL_CN,    "TempMail CN",    "tempmail.cn" },
    { CHANNEL_TMPMAILS,       "TmpMails",       "tmpmails.com" },
    { CHANNEL_TA_EASY,        "TA Easy",        "ta-easy.com" },
    { CHANNEL_10MINUTE_ONE,   "10 Minute Email", "10minutemail.one" },
    { CHANNEL_LINSHIYOU,      "临时邮",         "linshiyou.com" },
    { CHANNEL_MFFAC,          "MFFAC",          "mffac.com" },
    { CHANNEL_TEMPMAIL_LOL,   "TempMail LOL",   "tempmail.lol" },
    { CHANNEL_CHATGPT_ORG_UK, "ChatGPT Mail",   "mail.chatgpt.org.uk" },
    { CHANNEL_AWAMAIL,        "AwaMail",        "awamail.com" },
    { CHANNEL_MAIL_TM,        "Mail.tm",        "mail.tm" },
    { CHANNEL_DROPMAIL,       "DropMail",       "dropmail.me" },
    { CHANNEL_GUERRILLAMAIL,  "Guerrilla Mail", "guerrillamail.com" },
    { CHANNEL_MAILDROP,       "Maildrop",       "maildrop.cx" },
    { CHANNEL_SMAIL_PW,       "Smail.pw",       "smail.pw" },
    { CHANNEL_BOOMLIFY,       "Boomlify",       "boomlify.com" },
    { CHANNEL_MINMAIL,        "MinMail",        "minmail.app" },
    { CHANNEL_VIP_215,        "VIP 215",        "vip.215.im" },
    { CHANNEL_ANONBOX,        "Anonbox",        "anonbox.net" },
    { CHANNEL_FAKE_LEGAL,     "Fake Legal",     "fake.legal" },
    { CHANNEL_MOAKT,          "Moakt",          "moakt.com" },
    { CHANNEL_ETEMPMAIL,      "eTempMail",      "etempmail.com" },
    { CHANNEL_24MAIL_CHACUO,  "24Mail Chacuo",  "24mail.chacuo.net" },
    { CHANNEL_EMAIL10MIN,     "Email10Min",     "email10min.com" },
    { CHANNEL_MJJ_CM,         "MJJ Mail",       "mjj.cm" },
    { CHANNEL_MAIL_XIUVI,     "Xiuvi Mail",     "mail.xiuvi.cn" },
    { CHANNEL_LINSHI_CO,      "临时Co",         "linshi.co" },
    { CHANNEL_HARAKIRIMAIL,   "HarakiriMail",   "harakirimail.com" },
    { CHANNEL_TEMPMAIL_PLUS,  "TempMail Plus",  "tempmail.plus" },
    { CHANNEL_MAIL_GW,        "Mail.gw",        "mail.gw" },
    { CHANNEL_TEMPMAIL_LOL_V2, "TempMail LOL V2", "tempmail.lol" },
    { CHANNEL_SHARKLASERS,    "SharkLasers",    "sharklasers.com" },
    { CHANNEL_GRR_LA,         "Grr.la",         "grr.la" },
    { CHANNEL_GUERRILLAMAIL_INFO, "GuerrillaMail Info", "guerrillamail.info" },
};

const tm_channel_info_t* tm_list_channels(int *count) {
    if (count) *count = TM_CHANNEL_TRY_N;
    return g_channel_infos;
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
            case CHANNEL_BOOMLIFY:       result = tm_provider_boomlify_generate(); break;
            case CHANNEL_MINMAIL:        result = tm_provider_minmail_generate(); break;
            case CHANNEL_VIP_215:        result = tm_provider_vip215_generate(); break;
            case CHANNEL_ANONBOX:        result = tm_provider_anonbox_generate(); break;
            case CHANNEL_FAKE_LEGAL:     result = tm_provider_fake_legal_generate(domain); break;
            case CHANNEL_MFFAC:          result = tm_provider_mffac_generate(); break;
            case CHANNEL_TA_EASY:        result = tm_provider_ta_easy_generate(); break;
            case CHANNEL_TMPMAILS:       result = tm_provider_tmpmails_generate(domain); break;
            case CHANNEL_MOAKT:          result = tm_provider_moakt_generate(domain); break;
            case CHANNEL_ETEMPMAIL:      result = tm_provider_etempmail_generate(); break;
            case CHANNEL_10MINUTE_ONE:   result = tm_provider_tenminute_one_generate(domain); break;
            case CHANNEL_24MAIL_CHACUO:  result = tm_provider_24mail_chacuo_generate(); break;
            case CHANNEL_EMAIL10MIN:     result = tm_provider_email10min_generate(); break;
            case CHANNEL_MJJ_CM:         /* Socket.IO 渠道 C SDK 不支持 */ break;
            case CHANNEL_MAIL_XIUVI:     /* Socket.IO 渠道 C SDK 不支持 */ break;
            case CHANNEL_LINSHI_CO:      /* Socket.IO 渠道 C SDK 不支持 */ break;
            case CHANNEL_HARAKIRIMAIL:   result = tm_provider_harakirimail_generate(); break;
            case CHANNEL_TEMPMAIL_PLUS:  result = tm_provider_tempmail_plus_generate(); break;
            case CHANNEL_MAIL_GW:        result = tm_provider_mail_gw_generate(); break;
            case CHANNEL_TEMPMAIL_LOL_V2: result = tm_provider_tempmail_lol_v2_generate(); break;
            case CHANNEL_SHARKLASERS:    result = tm_provider_guerrillamail_mirror_generate(CHANNEL_SHARKLASERS, "https://www.sharklasers.com/ajax.php"); break;
            case CHANNEL_GRR_LA:         result = tm_provider_guerrillamail_mirror_generate(CHANNEL_GRR_LA, "https://www.grr.la/ajax.php"); break;
            case CHANNEL_GUERRILLAMAIL_INFO: result = tm_provider_guerrillamail_mirror_generate(CHANNEL_GUERRILLAMAIL_INFO, "https://www.guerrillamail.info/ajax.php"); break;
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
            case CHANNEL_BOOMLIFY:
                emails = tm_provider_boomlify_get_emails(email_info->email, &count);
                break;
            case CHANNEL_MINMAIL:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_minmail_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_VIP_215:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_vip215_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_ANONBOX:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_anonbox_get_emails(email_info->token, email_info->email, &count);
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
            case CHANNEL_TMPMAILS:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_tmpmails_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_MOAKT:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_moakt_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_ETEMPMAIL:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_etempmail_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_10MINUTE_ONE:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_tenminute_one_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_24MAIL_CHACUO:
                emails = tm_provider_24mail_chacuo_get_emails(email_info->email, &count);
                break;
            case CHANNEL_EMAIL10MIN:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_email10min_get_emails(email_info->token, email_info->email, &count);
                break;
            case CHANNEL_MJJ_CM:         /* Socket.IO 渠道 C SDK 不支持 */ count = -1; break;
            case CHANNEL_MAIL_XIUVI:     /* Socket.IO 渠道 C SDK 不支持 */ count = -1; break;
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
            case CHANNEL_GRR_LA:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_guerrillamail_mirror_get_emails("https://www.grr.la/ajax.php", email_info->token, email_info->email, &count);
                break;
            case CHANNEL_GUERRILLAMAIL_INFO:
                if (!email_info->token) { count = -1; break; }
                emails = tm_provider_guerrillamail_mirror_get_emails("https://www.guerrillamail.info/ajax.php", email_info->token, email_info->email, &count);
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
