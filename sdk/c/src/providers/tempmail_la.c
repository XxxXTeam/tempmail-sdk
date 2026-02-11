/**
 * tempmail.la 渠道实现（支持分页）
 */
#include "tempmail_internal.h"

#define TLA_BASE "https://tempmail.la/api"

static const char* tla_headers[] = {
    "Content-Type: application/json",
    "accept-language: zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
    "cache-control: no-cache",
    "dnt: 1",
    "locale: zh-CN",
    "origin: https://tempmail.la",
    "platform: PC",
    "pragma: no-cache",
    "product: TEMP_MAIL",
    "referer: https://tempmail.la/zh-CN/tempmail",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36 Edg/144.0.0.0",
    "sec-ch-ua: \"Not(A:Brand\";v=\"8\", \"Chromium\";v=\"144\", \"Microsoft Edge\";v=\"144\"",
    "sec-ch-ua-mobile: ?0",
    "sec-ch-ua-platform: \"Windows\"",
    "sec-fetch-dest: empty",
    "sec-fetch-mode: cors",
    "sec-fetch-site: same-origin",
    NULL
};

tm_email_info_t* tm_provider_tempmail_la_generate(void) {
    tm_http_response_t *resp = tm_http_request(TM_HTTP_POST, TLA_BASE "/mail/create", tla_headers, "{\"turnstile\":\"\"}", 15);
    if (!resp || resp->status != 200) { tm_http_response_free(resp); return NULL; }

    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) return NULL;

    cJSON *code = cJSON_GetObjectItemCaseSensitive(json, "code");
    if (!code || code->valueint != 0) { cJSON_Delete(json); return NULL; }

    cJSON *data = cJSON_GetObjectItemCaseSensitive(json, "data");
    tm_email_info_t *info = tm_email_info_new();
    info->channel = CHANNEL_TEMPMAIL_LA;
    info->email = tm_strdup(cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(data, "address")));
    cJSON *exp = cJSON_GetObjectItemCaseSensitive(data, "endAt");
    info->expires_at = exp && cJSON_IsNumber(exp) ? (long long)exp->valuedouble : 0;
    cJSON_Delete(json);
    return info;
}

tm_email_t* tm_provider_tempmail_la_get_emails(const char *email, int *count) {
    *count = -1;

    /* 收集所有分页邮件 */
    cJSON *all = cJSON_CreateArray();
    char *cursor = NULL;

    while (1) {
        char body[512];
        if (cursor) snprintf(body, sizeof(body), "{\"address\":\"%s\",\"cursor\":\"%s\"}", email, cursor);
        else snprintf(body, sizeof(body), "{\"address\":\"%s\",\"cursor\":null}", email);

        tm_http_response_t *resp = tm_http_request(TM_HTTP_POST, TLA_BASE "/mail/box", tla_headers, body, 15);
        if (!resp || resp->status != 200) { tm_http_response_free(resp); break; }

        cJSON *json = cJSON_Parse(resp->body);
        tm_http_response_free(resp);
        if (!json) break;

        cJSON *code = cJSON_GetObjectItemCaseSensitive(json, "code");
        if (!code || code->valueint != 0) { cJSON_Delete(json); break; }

        cJSON *data = cJSON_GetObjectItemCaseSensitive(json, "data");
        cJSON *rows = cJSON_GetObjectItemCaseSensitive(data, "rows");
        if (cJSON_IsArray(rows)) {
            int n = cJSON_GetArraySize(rows);
            for (int i = 0; i < n; i++) {
                cJSON_AddItemToArray(all, cJSON_Duplicate(cJSON_GetArrayItem(rows, i), 1));
            }
        }

        int has_more = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(data, "hasMore"));
        const char *next = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(data, "cursor"));
        free(cursor);
        cursor = NULL;
        if (has_more && next) cursor = tm_strdup(next);
        cJSON_Delete(json);
        if (!has_more || !cursor) break;
    }

    free(cursor);
    int n = cJSON_GetArraySize(all);
    *count = n;
    if (n == 0) { cJSON_Delete(all); return NULL; }

    tm_email_t *emails = tm_emails_new(n);
    for (int i = 0; i < n; i++) emails[i] = tm_normalize_email(cJSON_GetArrayItem(all, i), email);
    cJSON_Delete(all);
    return emails;
}
