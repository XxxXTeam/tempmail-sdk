/**
 * Guerrillamail 镜像渠道实现
 * sharklasers.com / grr.la / guerrillamail.info 共用同一套 API，仅 baseURL 不同
 */
#include "tempmail_internal.h"

static const char* gm_mirror_headers[] = {
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36",
    NULL
};

tm_email_info_t* tm_provider_guerrillamail_mirror_generate(tm_channel_t channel, const char *base_url) {
    char url[512];
    snprintf(url, sizeof(url), "%s?f=get_email_address&lang=en", base_url);

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, gm_mirror_headers, NULL, 15);
    if (!resp || resp->status != 200) { tm_http_response_free(resp); return NULL; }

    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) return NULL;

    const char *addr = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(json, "email_addr"));
    const char *sid = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(json, "sid_token"));
    if (!addr || !sid) { cJSON_Delete(json); return NULL; }

    tm_email_info_t *info = tm_email_info_new();
    info->channel = channel;
    info->email = tm_strdup(addr);
    info->token = tm_strdup(sid);

    cJSON *ts = cJSON_GetObjectItemCaseSensitive(json, "email_timestamp");
    if (ts && cJSON_IsNumber(ts)) {
        info->expires_at = ((long long)ts->valuedouble + 3600) * 1000;
    }

    cJSON_Delete(json);
    return info;
}

tm_email_t* tm_provider_guerrillamail_mirror_get_emails(const char *base_url, const char *token, const char *email, int *count) {
    *count = -1;
    char url[512];
    snprintf(url, sizeof(url), "%s?f=check_email&seq=0&sid_token=%s", base_url, token);

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, gm_mirror_headers, NULL, 15);
    if (!resp || resp->status != 200) { tm_http_response_free(resp); return NULL; }

    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) return NULL;

    cJSON *list = cJSON_GetObjectItemCaseSensitive(json, "list");
    int n = cJSON_IsArray(list) ? cJSON_GetArraySize(list) : 0;
    *count = n;
    if (n == 0) { cJSON_Delete(json); return NULL; }

    tm_email_t *emails = tm_emails_new(n);
    for (int i = 0; i < n; i++) {
        cJSON *item = cJSON_GetArrayItem(list, i);
        cJSON *flat = cJSON_CreateObject();
        cJSON *mid = cJSON_GetObjectItemCaseSensitive(item, "mail_id");
        if (mid) cJSON_AddNumberToObject(flat, "id", mid->valuedouble);
        cJSON_AddStringToObject(flat, "from", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(item, "mail_from"), ""));
        cJSON_AddStringToObject(flat, "to", email);
        cJSON_AddStringToObject(flat, "subject", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(item, "mail_subject"), ""));
        const char *body_str = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(item, "mail_body"));
        if (!body_str) body_str = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(item, "mail_excerpt"));
        cJSON_AddStringToObject(flat, "text", body_str ? body_str : "");
        cJSON_AddStringToObject(flat, "date", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(item, "mail_date"), ""));
        cJSON *mr = cJSON_GetObjectItemCaseSensitive(item, "mail_read");
        if (mr) cJSON_AddBoolToObject(flat, "isRead", mr->valueint == 1);

        emails[i] = tm_normalize_email(flat, email);
        cJSON_Delete(flat);
    }
    cJSON_Delete(json);
    return emails;
}
