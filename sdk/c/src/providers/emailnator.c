/**
 * Emailnator 渠道实现
 * 网站: https://www.emailnator.com
 * 需要 XSRF-TOKEN Session 认证
 */
#include "tempmail_internal.h"
#include <time.h>

#define EN_BASE "https://www.emailnator.com"

/**
 * 从 Set-Cookie 字符串中提取 XSRF-TOKEN 和完整 cookie
 * cookies_out 和 xsrf_out 需要调用者 free
 */
static int en_parse_cookies(const char *raw_cookies, char **xsrf_out, char **cookies_out) {
    *xsrf_out = NULL;
    *cookies_out = NULL;
    if (!raw_cookies) return -1;

    /* 简单提取 XSRF-TOKEN 值 */
    const char *xsrf_start = strstr(raw_cookies, "XSRF-TOKEN=");
    if (!xsrf_start) return -1;

    xsrf_start += strlen("XSRF-TOKEN=");
    const char *xsrf_end = strchr(xsrf_start, ';');
    if (!xsrf_end) xsrf_end = xsrf_start + strlen(xsrf_start);

    size_t xsrf_len = xsrf_end - xsrf_start;
    *xsrf_out = (char*)malloc(xsrf_len + 1);
    memcpy(*xsrf_out, xsrf_start, xsrf_len);
    (*xsrf_out)[xsrf_len] = '\0';

    *cookies_out = tm_strdup(raw_cookies);
    return 0;
}

tm_email_info_t* tm_provider_emailnator_generate(void) {
    const char* init_headers[] = {
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
        NULL
    };

    /* 1. 初始化 session，获取 cookie */
    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, EN_BASE, init_headers, NULL, 15);
    if (!resp || resp->status != 200) { tm_http_response_free(resp); return NULL; }

    char *xsrf_token = NULL;
    char *cookies = NULL;
    if (en_parse_cookies(resp->cookies, &xsrf_token, &cookies) != 0) {
        tm_http_response_free(resp);
        return NULL;
    }
    tm_http_response_free(resp);

    /* 2. 创建邮箱 */
    char xsrf_header[1024], cookie_header[2048];
    snprintf(xsrf_header, sizeof(xsrf_header), "X-XSRF-TOKEN: %s", xsrf_token);
    snprintf(cookie_header, sizeof(cookie_header), "Cookie: %s", cookies);

    const char* gen_headers[] = {
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
        "Content-Type: application/json",
        "Accept: application/json",
        "X-Requested-With: XMLHttpRequest",
        xsrf_header,
        cookie_header,
        NULL
    };

    resp = tm_http_request(TM_HTTP_POST, EN_BASE "/generate-email", gen_headers, "{\"email\":[\"domain\"]}", 15);
    if (!resp || resp->status != 200) {
        free(xsrf_token); free(cookies);
        tm_http_response_free(resp);
        return NULL;
    }

    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) { free(xsrf_token); free(cookies); return NULL; }

    cJSON *email_arr = cJSON_GetObjectItemCaseSensitive(json, "email");
    if (!cJSON_IsArray(email_arr) || cJSON_GetArraySize(email_arr) == 0) {
        cJSON_Delete(json); free(xsrf_token); free(cookies);
        return NULL;
    }

    const char *email = cJSON_GetStringValue(cJSON_GetArrayItem(email_arr, 0));
    if (!email) { cJSON_Delete(json); free(xsrf_token); free(cookies); return NULL; }

    /* token 存储 JSON: {"xsrfToken":"...","cookies":"..."} */
    cJSON *token_json = cJSON_CreateObject();
    cJSON_AddStringToObject(token_json, "xsrfToken", xsrf_token);
    cJSON_AddStringToObject(token_json, "cookies", cookies);
    char *token_str = cJSON_PrintUnformatted(token_json);
    cJSON_Delete(token_json);

    tm_email_info_t *info = tm_email_info_new();
    info->channel = CHANNEL_EMAILNATOR;
    info->email = tm_strdup(email);
    info->token = token_str;

    cJSON_Delete(json);
    free(xsrf_token);
    free(cookies);
    return info;
}

tm_email_t* tm_provider_emailnator_get_emails(const char *token, const char *email, int *count) {
    *count = -1;

    cJSON *session = cJSON_Parse(token);
    if (!session) return NULL;

    const char *xsrf = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(session, "xsrfToken"));
    const char *cookies = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(session, "cookies"));
    if (!xsrf || !cookies) { cJSON_Delete(session); return NULL; }

    char xsrf_header[1024], cookie_header[2048];
    snprintf(xsrf_header, sizeof(xsrf_header), "X-XSRF-TOKEN: %s", xsrf);
    snprintf(cookie_header, sizeof(cookie_header), "Cookie: %s", cookies);

    const char* headers[] = {
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
        "Content-Type: application/json",
        "Accept: application/json",
        "X-Requested-With: XMLHttpRequest",
        xsrf_header,
        cookie_header,
        NULL
    };

    char body[512];
    snprintf(body, sizeof(body), "{\"email\":\"%s\"}", email);

    tm_http_response_t *resp = tm_http_request(TM_HTTP_POST, EN_BASE "/message-list", headers, body, 15);
    cJSON_Delete(session);
    if (!resp || resp->status != 200) { tm_http_response_free(resp); return NULL; }

    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) return NULL;

    cJSON *msg_data = cJSON_GetObjectItemCaseSensitive(json, "messageData");
    if (!cJSON_IsArray(msg_data)) { cJSON_Delete(json); *count = 0; return NULL; }

    /* 过滤广告消息并计数 */
    int total = cJSON_GetArraySize(msg_data);
    int real_count = 0;
    for (int i = 0; i < total; i++) {
        cJSON *msg = cJSON_GetArrayItem(msg_data, i);
        const char *mid = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(msg, "messageID"));
        if (mid && strncmp(mid, "ADS", 3) != 0) real_count++;
    }

    *count = real_count;
    if (real_count == 0) { cJSON_Delete(json); return NULL; }

    tm_email_t *emails = tm_emails_new(real_count);
    int idx = 0;
    for (int i = 0; i < total && idx < real_count; i++) {
        cJSON *msg = cJSON_GetArrayItem(msg_data, i);
        const char *mid = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(msg, "messageID"));
        if (!mid || strncmp(mid, "ADS", 3) == 0) continue;

        emails[idx].id = tm_strdup(mid);
        emails[idx].from_addr = tm_strdup(TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(msg, "from"), ""));
        emails[idx].to = tm_strdup(email);
        emails[idx].subject = tm_strdup(TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(msg, "subject"), ""));
        emails[idx].text = tm_strdup("");
        emails[idx].html = tm_strdup("");
        emails[idx].date = tm_strdup(TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(msg, "time"), ""));
        emails[idx].is_read = false;
        emails[idx].attachments = NULL;
        emails[idx].attachment_count = 0;
        idx++;
    }

    cJSON_Delete(json);
    return emails;
}
