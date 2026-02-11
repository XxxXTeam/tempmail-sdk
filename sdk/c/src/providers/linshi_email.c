/**
 * linshi-email.com 渠道实现
 */
#include "tempmail_internal.h"
#include <time.h>

#define LS_BASE "https://www.linshi-email.com/api/v1"
#define LS_KEY  "552562b8524879814776e52bc8de5c9f"

static const char* ls_headers[] = {
    "Content-Type: application/json",
    "Origin: https://www.linshi-email.com",
    "Referer: https://www.linshi-email.com/",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36",
    "sec-ch-ua: \"Microsoft Edge\";v=\"143\", \"Chromium\";v=\"143\", \"Not A(Brand\";v=\"24\"",
    "sec-ch-ua-mobile: ?0",
    "sec-ch-ua-platform: \"Windows\"",
    "DNT: 1",
    NULL
};

tm_email_info_t* tm_provider_linshi_email_generate(void) {
    char url[256];
    snprintf(url, sizeof(url), LS_BASE "/email/%s", LS_KEY);
    tm_http_response_t *resp = tm_http_request(TM_HTTP_POST, url, ls_headers, "{}", 15);
    if (!resp || resp->status != 200) { tm_http_response_free(resp); return NULL; }

    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) return NULL;

    cJSON *status = cJSON_GetObjectItemCaseSensitive(json, "status");
    if (!status || strcmp(status->valuestring, "ok") != 0) { cJSON_Delete(json); return NULL; }

    cJSON *data = cJSON_GetObjectItemCaseSensitive(json, "data");
    tm_email_info_t *info = tm_email_info_new();
    info->channel = CHANNEL_LINSHI_EMAIL;
    info->email = tm_strdup(cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(data, "email")));
    cJSON *exp = cJSON_GetObjectItemCaseSensitive(data, "expired");
    info->expires_at = exp && cJSON_IsNumber(exp) ? (long long)exp->valuedouble : 0;
    cJSON_Delete(json);
    return info;
}

tm_email_t* tm_provider_linshi_email_get_emails(const char *email, int *count) {
    *count = -1;
    char url[512];
    snprintf(url, sizeof(url), LS_BASE "/refreshmessage/%s/%s?t=%lld", LS_KEY, email, (long long)time(NULL)*1000);
    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, ls_headers, NULL, 15);
    if (!resp || resp->status != 200) { tm_http_response_free(resp); return NULL; }

    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) return NULL;

    cJSON *status = cJSON_GetObjectItemCaseSensitive(json, "status");
    if (!status || strcmp(status->valuestring, "ok") != 0) { cJSON_Delete(json); return NULL; }

    cJSON *arr = cJSON_GetObjectItemCaseSensitive(json, "list");
    int n = cJSON_IsArray(arr) ? cJSON_GetArraySize(arr) : 0;
    *count = n;
    if (n == 0) { cJSON_Delete(json); return NULL; }

    tm_email_t *emails = tm_emails_new(n);
    for (int i = 0; i < n; i++) emails[i] = tm_normalize_email(cJSON_GetArrayItem(arr, i), email);
    cJSON_Delete(json);
    return emails;
}
