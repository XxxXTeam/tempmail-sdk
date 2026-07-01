/**
 * Emailnator 渠道实现
 * 网站: https://www.emailnator.com
 * 需要 XSRF-TOKEN Session 认证
 */
#include "tempmail_internal.h"
#include <time.h>
#include <ctype.h>

#define EN_BASE "https://www.emailnator.com"

static int en_hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static char *en_url_decode(const char *s) {
    if (!s) return tm_strdup("");
    size_t len = strlen(s);
    char *out = (char*)malloc(len + 1);
    if (!out) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '%' && i + 2 < len) {
            int hi = en_hex_val(s[i + 1]);
            int lo = en_hex_val(s[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out[j++] = (char)((hi << 4) | lo);
                i += 2;
                continue;
            }
        }
        out[j++] = s[i] == '+' ? ' ' : s[i];
    }
    out[j] = '\0';
    return out;
}

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
    char *encoded = (char*)malloc(xsrf_len + 1);
    if (!encoded) return -1;
    memcpy(encoded, xsrf_start, xsrf_len);
    encoded[xsrf_len] = '\0';
    *xsrf_out = en_url_decode(encoded);
    free(encoded);
    if (!*xsrf_out) return -1;

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

    resp = tm_http_request(TM_HTTP_POST, EN_BASE "/generate-email", gen_headers, "{\"email\":[\"plusGmail\",\"dotGmail\",\"googleMail\"]}", 15);
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

static char *en_fetch_detail(const char *xsrf, const char *cookies, const char *email, const char *message_id) {
    if (!message_id || !message_id[0]) return tm_strdup("");
    char xsrf_header[1024], cookie_header[2048], body[1024];
    snprintf(xsrf_header, sizeof(xsrf_header), "X-XSRF-TOKEN: %s", xsrf);
    snprintf(cookie_header, sizeof(cookie_header), "Cookie: %s", cookies);
    snprintf(body, sizeof(body), "{\"email\":\"%s\",\"messageID\":\"%s\"}", email, message_id);

    const char* headers[] = {
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
        "Content-Type: application/json",
        "Accept: application/json, text/plain, */*",
        "Origin: https://www.emailnator.com",
        "Referer: https://www.emailnator.com/",
        "X-Requested-With: XMLHttpRequest",
        xsrf_header,
        cookie_header,
        NULL
    };
    tm_http_response_t *resp = tm_http_request(TM_HTTP_POST, EN_BASE "/message-list", headers, body, 15);
    if (!resp || resp->status != 200) {
        tm_http_response_free(resp);
        return tm_strdup("");
    }
    cJSON *json = cJSON_Parse(resp->body);
    char *html = NULL;
    if (cJSON_IsString(json)) {
        html = tm_strdup(json->valuestring);
    } else if (resp->body) {
        html = tm_strdup(resp->body);
    } else {
        html = tm_strdup("");
    }
    cJSON_Delete(json);
    tm_http_response_free(resp);
    return html;
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
        char *html = en_fetch_detail(xsrf, cookies, email, mid);

        cJSON *flat = cJSON_CreateObject();
        cJSON_AddStringToObject(flat, "id", mid);
        cJSON_AddStringToObject(flat, "from", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(msg, "from"), ""));
        cJSON_AddStringToObject(flat, "to", email);
        cJSON_AddStringToObject(flat, "subject", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(msg, "subject"), ""));
        cJSON_AddStringToObject(flat, "html", html ? html : "");
        cJSON_AddStringToObject(flat, "date", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(msg, "time"), ""));
        cJSON_AddBoolToObject(flat, "isRead", 0);
        emails[idx] = tm_normalize_email(flat, email);
        cJSON_Delete(flat);
        free(html);
        idx++;
    }

    cJSON_Delete(json);
    return emails;
}
