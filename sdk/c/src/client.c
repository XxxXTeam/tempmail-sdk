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

static const tm_channel_info_t g_channel_infos[] = {
    { CHANNEL_TEMPMAIL,       "TempMail",       "tempmail.ing" },
    { CHANNEL_LINSHI_EMAIL,   "临时邮箱",       "linshi-email.com" },
    { CHANNEL_TEMPMAIL_LOL,   "TempMail LOL",   "tempmail.lol" },
    { CHANNEL_CHATGPT_ORG_UK, "ChatGPT Mail",   "mail.chatgpt.org.uk" },
    { CHANNEL_TEMPMAIL_LA,    "TempMail LA",     "tempmail.la" },
    { CHANNEL_TEMP_MAIL_IO,   "Temp Mail IO",    "temp-mail.io" },
    { CHANNEL_AWAMAIL,        "AwaMail",         "awamail.com" },
    { CHANNEL_MAIL_TM,        "Mail.tm",         "mail.tm" },
    { CHANNEL_DROPMAIL,       "DropMail",        "dropmail.me" },
    { CHANNEL_GUERRILLAMAIL,  "Guerrilla Mail",  "guerrillamail.com" },
    { CHANNEL_MAILDROP,       "Maildrop",        "maildrop.cc" },
};

const tm_channel_info_t* tm_list_channels(int *count) {
    if (count) *count = CHANNEL_COUNT;
    return g_channel_infos;
}

tm_email_info_t* tm_generate_email(const tm_generate_options_t *options) {
    tm_channel_t channel = options ? options->channel : CHANNEL_RANDOM;

    if (channel == CHANNEL_RANDOM) {
        srand((unsigned int)time(NULL));
        channel = (tm_channel_t)(rand() % CHANNEL_COUNT);
    }

    TM_LOG_INF("创建临时邮箱, 渠道: %s", tm_channel_name(channel));

    tm_email_info_t *result = NULL;
    int duration = (options && options->duration > 0) ? options->duration : 30;
    const char *domain = options ? options->domain : NULL;

    /* 简单重试（不使用函数指针重试器，因为参数不同） */
    tm_retry_config_t cfg = (options && options->retry) ? *options->retry : tm_default_retry_config();

    for (int attempt = 0; attempt <= cfg.max_retries; attempt++) {
        switch (channel) {
            case CHANNEL_TEMPMAIL:       result = tm_provider_tempmail_generate(duration); break;
            case CHANNEL_LINSHI_EMAIL:   result = tm_provider_linshi_email_generate(); break;
            case CHANNEL_TEMPMAIL_LOL:   result = tm_provider_tempmail_lol_generate(domain); break;
            case CHANNEL_CHATGPT_ORG_UK: result = tm_provider_chatgpt_org_uk_generate(); break;
            case CHANNEL_TEMPMAIL_LA:    result = tm_provider_tempmail_la_generate(); break;
            case CHANNEL_TEMP_MAIL_IO:   result = tm_provider_temp_mail_io_generate(); break;
            case CHANNEL_AWAMAIL:        result = tm_provider_awamail_generate(); break;
            case CHANNEL_MAIL_TM:        result = tm_provider_mail_tm_generate(); break;
            case CHANNEL_DROPMAIL:       result = tm_provider_dropmail_generate(); break;
            case CHANNEL_GUERRILLAMAIL:  result = tm_provider_guerrillamail_generate(); break;
            case CHANNEL_MAILDROP:       result = tm_provider_maildrop_generate(); break;
            default: return NULL;
        }

        if (result) {
            if (attempt > 0) TM_LOG_INF("第 %d 次尝试成功", attempt + 1);
            TM_LOG_INF("邮箱创建成功: %s", result->email);
            return result;
        }

        if (attempt < cfg.max_retries) {
            int delay = cfg.initial_delay_ms * (1 << attempt);
            if (delay > cfg.max_delay_ms) delay = cfg.max_delay_ms;
            TM_LOG_WARN("请求失败，%dms 后第 %d 次重试...", delay, attempt + 2);
            tm_sleep_ms(delay);
        }
    }

    TM_LOG_ERR("重试 %d 次后仍失败", cfg.max_retries);
    return NULL;
}

tm_get_emails_result_t* tm_get_emails(const tm_get_emails_options_t *options) {
    if (!options) return NULL;

    tm_get_emails_result_t *result = (tm_get_emails_result_t*)calloc(1, sizeof(tm_get_emails_result_t));
    if (!result) return NULL;

    result->channel = options->channel;
    result->email = tm_strdup(options->email);

    TM_LOG_DBG("获取邮件, 渠道: %s, 邮箱: %s", tm_channel_name(options->channel), options->email);

    tm_retry_config_t cfg = (options->retry) ? *options->retry : tm_default_retry_config();
    int count = 0;
    tm_email_t *emails = NULL;

    for (int attempt = 0; attempt <= cfg.max_retries; attempt++) {
        switch (options->channel) {
            case CHANNEL_TEMPMAIL:       emails = tm_provider_tempmail_get_emails(options->email, &count); break;
            case CHANNEL_LINSHI_EMAIL:   emails = tm_provider_linshi_email_get_emails(options->email, &count); break;
            case CHANNEL_TEMPMAIL_LOL:   emails = tm_provider_tempmail_lol_get_emails(options->token, options->email, &count); break;
            case CHANNEL_CHATGPT_ORG_UK: emails = tm_provider_chatgpt_org_uk_get_emails(options->email, &count); break;
            case CHANNEL_TEMPMAIL_LA:    emails = tm_provider_tempmail_la_get_emails(options->email, &count); break;
            case CHANNEL_TEMP_MAIL_IO:   emails = tm_provider_temp_mail_io_get_emails(options->email, &count); break;
            case CHANNEL_AWAMAIL:        emails = tm_provider_awamail_get_emails(options->token, options->email, &count); break;
            case CHANNEL_MAIL_TM:        emails = tm_provider_mail_tm_get_emails(options->token, options->email, &count); break;
            case CHANNEL_DROPMAIL:       emails = tm_provider_dropmail_get_emails(options->token, options->email, &count); break;
            case CHANNEL_GUERRILLAMAIL:  emails = tm_provider_guerrillamail_get_emails(options->token, options->email, &count); break;
            case CHANNEL_MAILDROP:       emails = tm_provider_maildrop_get_emails(options->token, options->email, &count); break;
            default: break;
        }

        /* count >= 0 表示请求成功（即使邮件为空） */
        if (count >= 0 && (emails || count == 0)) {
            if (attempt > 0) TM_LOG_INF("第 %d 次尝试成功", attempt + 1);
            result->emails = emails;
            result->email_count = count;
            result->success = true;
            if (count > 0) {
                TM_LOG_INF("获取到 %d 封邮件, 渠道: %s", count, tm_channel_name(options->channel));
            } else {
                TM_LOG_DBG("暂无邮件, 渠道: %s", tm_channel_name(options->channel));
            }
            return result;
        }

        if (attempt < cfg.max_retries) {
            int delay = cfg.initial_delay_ms * (1 << attempt);
            if (delay > cfg.max_delay_ms) delay = cfg.max_delay_ms;
            TM_LOG_WARN("请求失败，%dms 后第 %d 次重试...", delay, attempt + 2);
            tm_sleep_ms(delay);
        }
    }

    TM_LOG_ERR("获取邮件失败, 渠道: %s", tm_channel_name(options->channel));
    result->success = false;
    result->error = tm_strdup("重试耗尽后仍失败");
    return result;
}
