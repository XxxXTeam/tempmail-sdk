/**
 * ExpressInboxHub 渠道实现
 * 网站: https://expressinboxhub.com
 * 模式: Fake_trash_mail（CSRF token + session cookies）
 * 域名: mail42.shop
 */
#include "tempmail_internal.h"
#include <string.h>

#define EIH_BASE "https://expressinboxhub.com"

/**
 * 从 HTML 响应体中提取 CSRF token
 * 查找: <meta name="csrf-token" content="xxx">
 * 返回值需要调用者 free
 */
static char *eih_extract_csrf(const char *html) {
    if (!html) return NULL;

    /* 查找 csrf-token meta 标签 */
    const char *needle = "name=\"csrf-token\"";
    const char *pos = strstr(html, needle);
    if (!pos) return NULL;

    /* 向后找 content="..." */
    const char *content_attr = strstr(pos, "content=\"");
    if (!content_attr) return NULL;
    content_attr += strlen("content=\"");

    const char *end = strchr(content_attr, '"');
    if (!end || end <= content_attr) return NULL;

    size_t len = end - content_attr;
    char *token = (char *)malloc(len + 1);
    if (!token) return NULL;
    memcpy(token, content_attr, len);
    token[len] = '\0';
    return token;
}

/**
 * 从 Set-Cookie 头中提取完整 cookie 字符串
 * 格式化为 "key1=val1; key2=val2" 用于后续请求
 * 返回值需要调用者 free
 */
static char *eih_build_cookie_header(const char *raw_cookies) {
    if (!raw_cookies || !raw_cookies[0]) return NULL;
    return tm_strdup(raw_cookies);
}

tm_email_info_t *tm_provider_expressinboxhub_generate(void) {
    const char *init_headers[] = {
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
        "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8",
        NULL
    };

    /* 1. GET 首页，获取 CSRF token 和 session cookies */
    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, EIH_BASE, init_headers, NULL, 15);
    if (!resp || resp->status != 200) {
        TM_LOG_ERR("[expressinboxhub] 获取首页失败");
        tm_http_response_free(resp);
        return NULL;
    }

    char *csrf_token = eih_extract_csrf(resp->body);
    if (!csrf_token) {
        TM_LOG_ERR("[expressinboxhub] 从 HTML 中提取 CSRF token 失败");
        tm_http_response_free(resp);
        return NULL;
    }

    char *cookies = eih_build_cookie_header(resp->cookies);
    tm_http_response_free(resp);

    if (!cookies) {
        TM_LOG_ERR("[expressinboxhub] 未获取到 session cookies");
        free(csrf_token);
        return NULL;
    }

    /* 2. POST /messages 创建邮箱 */
    char cookie_header[4096];
    snprintf(cookie_header, sizeof(cookie_header), "Cookie: %s", cookies);

    char referer_header[] = "Referer: " EIH_BASE "/";
    char xsrf_header[1024];
    snprintf(xsrf_header, sizeof(xsrf_header), "X-CSRF-TOKEN: %s", csrf_token);

    const char *post_headers[] = {
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
        "Content-Type: application/json",
        "Accept: application/json",
        "X-Requested-With: XMLHttpRequest",
        referer_header,
        xsrf_header,
        cookie_header,
        NULL
    };

    /* 构造请求体 {"_token":"..."} */
    char body[1024];
    snprintf(body, sizeof(body), "{\"_token\":\"%s\"}", csrf_token);

    tm_http_response_t *r2 = tm_http_request(TM_HTTP_POST, EIH_BASE "/messages", post_headers, body, 15);
    if (!r2 || (r2->status < 200 || r2->status >= 300)) {
        TM_LOG_ERR("[expressinboxhub] POST /messages 失败, 状态码: %ld", r2 ? r2->status : 0);
        free(csrf_token);
        free(cookies);
        tm_http_response_free(r2);
        return NULL;
    }

    cJSON *json = cJSON_Parse(r2->body);
    tm_http_response_free(r2);
    if (!json) {
        TM_LOG_ERR("[expressinboxhub] 解析 JSON 响应失败");
        free(csrf_token);
        free(cookies);
        return NULL;
    }

    const char *mailbox = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(json, "mailbox"));
    if (!mailbox || !mailbox[0]) {
        TM_LOG_ERR("[expressinboxhub] 响应中未找到 mailbox 字段");
        cJSON_Delete(json);
        free(csrf_token);
        free(cookies);
        return NULL;
    }

    /* token 存储 JSON: {"csrfToken":"...", "cookies":"..."} */
    cJSON *token_json = cJSON_CreateObject();
    cJSON_AddStringToObject(token_json, "csrfToken", csrf_token);
    cJSON_AddStringToObject(token_json, "cookies", cookies);
    char *token_str = cJSON_PrintUnformatted(token_json);
    cJSON_Delete(token_json);

    tm_email_info_t *info = tm_email_info_new();
    info->channel = CHANNEL_EXPRESSINBOXHUB;
    info->email = tm_strdup(mailbox);
    info->token = token_str;

    cJSON_Delete(json);
    free(csrf_token);
    free(cookies);
    return info;
}

tm_email_t *tm_provider_expressinboxhub_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    if (!token || !email) return NULL;

    /* 解析 token 中的 session 信息 */
    cJSON *session = cJSON_Parse(token);
    if (!session) return NULL;

    const char *csrf_token = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(session, "csrfToken"));
    const char *cookies = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(session, "cookies"));
    if (!csrf_token || !cookies) {
        cJSON_Delete(session);
        return NULL;
    }

    /* 构建请求头 */
    char cookie_header[4096];
    snprintf(cookie_header, sizeof(cookie_header), "Cookie: %s", cookies);

    char xsrf_header[1024];
    snprintf(xsrf_header, sizeof(xsrf_header), "X-CSRF-TOKEN: %s", csrf_token);

    char referer_header[] = "Referer: " EIH_BASE "/";

    const char *headers[] = {
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
        "Content-Type: application/json",
        "Accept: application/json",
        "X-Requested-With: XMLHttpRequest",
        referer_header,
        xsrf_header,
        cookie_header,
        NULL
    };

    /* POST /messages 获取邮件列表 */
    char body[1024];
    snprintf(body, sizeof(body), "{\"_token\":\"%s\"}", csrf_token);

    tm_http_response_t *resp = tm_http_request(TM_HTTP_POST, EIH_BASE "/messages", headers, body, 15);
    cJSON_Delete(session);

    if (!resp || (resp->status < 200 || resp->status >= 300)) {
        tm_http_response_free(resp);
        return NULL;
    }

    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) return NULL;

    cJSON *messages = cJSON_GetObjectItemCaseSensitive(json, "messages");
    if (!cJSON_IsArray(messages)) {
        *count = 0;
        cJSON_Delete(json);
        return NULL;
    }

    int n = cJSON_GetArraySize(messages);
    *count = n;
    if (n == 0) {
        cJSON_Delete(json);
        return NULL;
    }

    tm_email_t *emails = tm_emails_new(n);
    if (!emails) {
        *count = -1;
        cJSON_Delete(json);
        return NULL;
    }

    for (int i = 0; i < n; i++) {
        cJSON *msg = cJSON_GetArrayItem(messages, i);
        emails[i] = tm_normalize_email(msg, email);
    }

    cJSON_Delete(json);
    return emails;
}
