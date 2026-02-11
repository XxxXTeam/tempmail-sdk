/**
 * mail.chatgpt.org.uk 渠道实现
 */
#include "tempmail_internal.h"

#define CG_BASE "https://mail.chatgpt.org.uk/api"

static const char* cg_headers[] = {
    "Content-Type: application/json",
    "Referer: https://mail.chatgpt.org.uk/",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36",
    "sec-ch-ua: \"Microsoft Edge\";v=\"143\", \"Chromium\";v=\"143\", \"Not A(Brand\";v=\"24\"",
    "sec-ch-ua-mobile: ?0",
    "sec-ch-ua-platform: \"Windows\"",
    "DNT: 1",
    NULL
};

tm_email_info_t* tm_provider_chatgpt_org_uk_generate(void) {
    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, CG_BASE "/generate-email", cg_headers, NULL, 15);
    if (!resp || resp->status != 200) { tm_http_response_free(resp); return NULL; }

    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) return NULL;

    if (!cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(json, "success"))) { cJSON_Delete(json); return NULL; }

    cJSON *data = cJSON_GetObjectItemCaseSensitive(json, "data");
    tm_email_info_t *info = tm_email_info_new();
    info->channel = CHANNEL_CHATGPT_ORG_UK;
    info->email = tm_strdup(cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(data, "email")));
    cJSON_Delete(json);
    return info;
}

tm_email_t* tm_provider_chatgpt_org_uk_get_emails(const char *email, int *count) {
    *count = -1;
    char url[512];
    snprintf(url, sizeof(url), CG_BASE "/emails?email=%s", email);
    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, cg_headers, NULL, 15);
    if (!resp || resp->status != 200) { tm_http_response_free(resp); return NULL; }

    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) return NULL;

    if (!cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(json, "success"))) { cJSON_Delete(json); return NULL; }

    cJSON *data = cJSON_GetObjectItemCaseSensitive(json, "data");
    cJSON *arr = cJSON_GetObjectItemCaseSensitive(data, "emails");
    int n = cJSON_IsArray(arr) ? cJSON_GetArraySize(arr) : 0;
    *count = n;
    if (n == 0) { cJSON_Delete(json); return NULL; }

    tm_email_t *emails = tm_emails_new(n);
    for (int i = 0; i < n; i++) emails[i] = tm_normalize_email(cJSON_GetArrayItem(arr, i), email);
    cJSON_Delete(json);
    return emails;
}
