/**
 * linshi-email.com 渠道实现
 */
#include "tempmail_internal.h"
#include <time.h>

#define LS_BASE "https://www.linshi-email.com/api/v1"

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
    char api_key[96];
    char url[320];
    tm_http_response_t *resp;
    cJSON *json, *status, *data, *email_item;
    const char *keys[] = { "email", "mail", "address" };
    char *email_val;
    tm_email_info_t *info;

    if (tm_linshi_random_api_path_key(api_key, sizeof(api_key)) != 0)
        return NULL;

    snprintf(url, sizeof(url), LS_BASE "/email/%s", api_key);
    resp = tm_http_request(TM_HTTP_POST, url, ls_headers, "{}", 15);
    if (!resp || resp->status != 200) { tm_http_response_free(resp); return NULL; }

    json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) return NULL;

    status = cJSON_GetObjectItemCaseSensitive(json, "status");
    if (!status || strcmp(status->valuestring, "ok") != 0) { cJSON_Delete(json); return NULL; }

    data = cJSON_GetObjectItemCaseSensitive(json, "data");
    if (!data || !cJSON_IsObject(data)) { cJSON_Delete(json); return NULL; }

    email_val = tm_json_get_str(data, keys, 3);
    if (!email_val || !strchr(email_val, '@')) {
        free(email_val);
        cJSON_Delete(json);
        return NULL;
    }

    info = tm_email_info_new();
    info->channel = CHANNEL_LINSHI_EMAIL;
    info->email = email_val;
    info->token = tm_strdup(api_key);
    email_item = cJSON_GetObjectItemCaseSensitive(data, "expired");
    info->expires_at = email_item && cJSON_IsNumber(email_item) ? (long long)email_item->valuedouble : 0;
    cJSON_Delete(json);
    return info;
}

tm_email_t* tm_provider_linshi_email_get_emails(const char *api_path_key, const char *email, int *count) {
    *count = -1;
    if (!api_path_key || !*api_path_key || !email) return NULL;

    char url[768];
    snprintf(url, sizeof(url), LS_BASE "/refreshmessage/%s/%s?t=%lld",
             api_path_key, email, (long long)time(NULL) * 1000);
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
