/**
 * awamail.com 渠道实现
 */
#include "tempmail_internal.h"

#define AWA_BASE "https://awamail.com/welcome"

static const char* awa_headers[] = {
    "Content-Length: 0",
    "accept-language: zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
    "cache-control: no-cache",
    "dnt: 1",
    "origin: https://awamail.com",
    "pragma: no-cache",
    "referer: https://awamail.com/?lang=zh",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36 Edg/144.0.0.0",
    "sec-ch-ua: \"Not(A:Brand\";v=\"8\", \"Chromium\";v=\"144\", \"Microsoft Edge\";v=\"144\"",
    "sec-ch-ua-mobile: ?0",
    "sec-ch-ua-platform: \"Windows\"",
    "sec-fetch-dest: empty",
    "sec-fetch-mode: cors",
    "sec-fetch-site: same-origin",
    NULL
};

static const char* awa_get_headers_base[] = {
    "x-requested-with: XMLHttpRequest",
    "dnt: 1",
    "origin: https://awamail.com",
    "referer: https://awamail.com/?lang=zh",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36 Edg/144.0.0.0",
    "sec-fetch-dest: empty",
    "sec-fetch-mode: cors",
    "sec-fetch-site: same-origin",
    NULL
};

tm_email_info_t* tm_provider_awamail_generate(void) {
    tm_http_response_t *resp = tm_http_request(TM_HTTP_POST, AWA_BASE "/change_mailbox", awa_headers, "", 15);
    if (!resp || resp->status != 200) { tm_http_response_free(resp); return NULL; }

    /* 提取 session cookie */
    if (!resp->cookies || !strstr(resp->cookies, "awamail_session=")) {
        tm_http_response_free(resp);
        return NULL;
    }

    /* 只保留 awamail_session=xxx 部分 */
    char *session = NULL;
    char *start = strstr(resp->cookies, "awamail_session=");
    if (start) {
        char *end = strchr(start, ';');
        size_t len = end ? (size_t)(end - start) : strlen(start);
        session = (char*)malloc(len + 1);
        memcpy(session, start, len);
        session[len] = '\0';
    }

    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) { free(session); return NULL; }

    if (!cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(json, "success"))) {
        cJSON_Delete(json); free(session); return NULL;
    }

    cJSON *data = cJSON_GetObjectItemCaseSensitive(json, "data");
    tm_email_info_t *info = tm_email_info_new();
    info->channel = CHANNEL_AWAMAIL;
    info->email = tm_strdup(cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(data, "email_address")));
    info->token = session;
    cJSON_Delete(json);
    return info;
}

tm_email_t* tm_provider_awamail_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    /* 构建带 Cookie 的 headers */
    char cookie_hdr[512];
    snprintf(cookie_hdr, sizeof(cookie_hdr), "Cookie: %s", token);
    const char* headers[] = {
        cookie_hdr,
        "x-requested-with: XMLHttpRequest",
        "dnt: 1",
        "origin: https://awamail.com",
        "referer: https://awamail.com/?lang=zh",
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36 Edg/144.0.0.0",
        "sec-fetch-dest: empty",
        "sec-fetch-mode: cors",
        "sec-fetch-site: same-origin",
        NULL
    };

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, AWA_BASE "/get_emails", headers, NULL, 15);
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
