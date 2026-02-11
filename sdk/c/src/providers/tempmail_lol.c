/**
 * tempmail.lol 渠道实现
 */
#include "tempmail_internal.h"

#define TL_BASE "https://api.tempmail.lol/v2"

static const char* tl_headers[] = {
    "Content-Type: application/json",
    "Origin: https://tempmail.lol",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36",
    "sec-ch-ua: \"Microsoft Edge\";v=\"143\", \"Chromium\";v=\"143\", \"Not A(Brand\";v=\"24\"",
    "sec-ch-ua-mobile: ?0",
    "sec-ch-ua-platform: \"Windows\"",
    "DNT: 1",
    NULL
};

tm_email_info_t* tm_provider_tempmail_lol_generate(const char *domain) {
    char body[256];
    if (domain) snprintf(body, sizeof(body), "{\"domain\":\"%s\",\"captcha\":null}", domain);
    else snprintf(body, sizeof(body), "{\"domain\":null,\"captcha\":null}");

    tm_http_response_t *resp = tm_http_request(TM_HTTP_POST, TL_BASE "/inbox/create", tl_headers, body, 15);
    if (!resp || resp->status != 200) { tm_http_response_free(resp); return NULL; }

    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) return NULL;

    const char *addr = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(json, "address"));
    const char *token = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(json, "token"));
    if (!addr || !token) { cJSON_Delete(json); return NULL; }

    tm_email_info_t *info = tm_email_info_new();
    info->channel = CHANNEL_TEMPMAIL_LOL;
    info->email = tm_strdup(addr);
    info->token = tm_strdup(token);
    cJSON_Delete(json);
    return info;
}

tm_email_t* tm_provider_tempmail_lol_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    char url[512];
    snprintf(url, sizeof(url), TL_BASE "/inbox?token=%s", token);
    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, tl_headers, NULL, 15);
    if (!resp || resp->status != 200) { tm_http_response_free(resp); return NULL; }

    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) return NULL;

    cJSON *arr = cJSON_GetObjectItemCaseSensitive(json, "emails");
    int n = cJSON_IsArray(arr) ? cJSON_GetArraySize(arr) : 0;
    *count = n;
    if (n == 0) { cJSON_Delete(json); return NULL; }

    tm_email_t *emails = tm_emails_new(n);
    for (int i = 0; i < n; i++) emails[i] = tm_normalize_email(cJSON_GetArrayItem(arr, i), email);
    cJSON_Delete(json);
    return emails;
}
