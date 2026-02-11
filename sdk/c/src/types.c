/**
 * 类型相关的工具函数
 */

#include "tempmail_internal.h"

/* 渠道名称字符串映射 */
static const char* channel_names[] = {
    "tempmail",
    "linshi-email",
    "tempmail-lol",
    "chatgpt-org-uk",
    "tempmail-la",
    "temp-mail-io",
    "awamail",
    "mail-tm",
    "dropmail",
    "guerrillamail",
    "maildrop",
};

const char* tm_channel_name(tm_channel_t channel) {
    if (channel >= 0 && channel < CHANNEL_COUNT) {
        return channel_names[channel];
    }
    return "unknown";
}

char* tm_strdup(const char *s) {
    if (!s) {
        char *empty = (char*)malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }
    size_t len = strlen(s);
    char *dup = (char*)malloc(len + 1);
    if (dup) {
        memcpy(dup, s, len + 1);
    }
    return dup;
}

char* tm_json_get_str(const cJSON *obj, const char **keys, int key_count) {
    for (int i = 0; i < key_count; i++) {
        const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, keys[i]);
        if (cJSON_IsString(item) && item->valuestring) {
            return tm_strdup(item->valuestring);
        }
        if (cJSON_IsNumber(item)) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%g", item->valuedouble);
            return tm_strdup(buf);
        }
    }
    return tm_strdup("");
}

tm_email_info_t* tm_email_info_new(void) {
    tm_email_info_t *info = (tm_email_info_t*)calloc(1, sizeof(tm_email_info_t));
    return info;
}

tm_email_t* tm_emails_new(int count) {
    if (count <= 0) return NULL;
    tm_email_t *emails = (tm_email_t*)calloc(count, sizeof(tm_email_t));
    return emails;
}

void tm_free_email_info(tm_email_info_t *info) {
    if (!info) return;
    free(info->email);
    free(info->token);
    free(info->created_at);
    free(info);
}

void tm_free_email(tm_email_t *email) {
    if (!email) return;
    free(email->id);
    free(email->from_addr);
    free(email->to);
    free(email->subject);
    free(email->text);
    free(email->html);
    free(email->date);
    if (email->attachments) {
        for (int i = 0; i < email->attachment_count; i++) {
            free(email->attachments[i].filename);
            free(email->attachments[i].content_type);
            free(email->attachments[i].url);
        }
        free(email->attachments);
    }
}

void tm_free_get_emails_result(tm_get_emails_result_t *result) {
    if (!result) return;
    free(result->email);
    free(result->error);
    if (result->emails) {
        for (int i = 0; i < result->email_count; i++) {
            tm_free_email(&result->emails[i]);
        }
        free(result->emails);
    }
    free(result);
}

tm_retry_config_t tm_default_retry_config(void) {
    tm_retry_config_t cfg;
    cfg.max_retries = 2;
    cfg.initial_delay_ms = 1000;
    cfg.max_delay_ms = 5000;
    cfg.timeout_secs = 15;
    return cfg;
}
