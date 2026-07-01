/**
 * Lroid — https://lroid.com
 * 通过 HTML 解析获取邮箱地址，使用 session cookies 维持会话
 * 域名: yevme.com
 * 简化实现：使用 kontrol API
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LROID_BASE "https://lroid.com"

/* 请求头 */
static const char *lroid_headers[] = {
    "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
    "Accept-Language: en-US,en;q=0.9",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/148.0.0.0 Safari/537.36",
    NULL
};

/**
 * 从 HTML 中提取 <input id="eposta_adres" value="xxx@yevme.com"> 的 value
 * 使用 strstr 进行简单字符串匹配
 * 返回新分配的字符串，调用者需 free
 */
static char* lroid_extract_email_from_html(const char *html) {
    if (!html) return NULL;

    /* 查找 id="eposta_adres" */
    const char *id_pos = strstr(html, "eposta_adres");
    if (!id_pos) return NULL;

    /* 从 id 位置向前回溯找到 <input 标签的起始，或向后找 value */
    /* 先向后查找 value= */
    const char *value_pos = strstr(id_pos, "value=\"");
    if (!value_pos) {
        /* 也可能 value 在 id 之前，从 id 位置向前查找所属 <input 标签 */
        const char *search = html;
        const char *input_start = NULL;
        while (search < id_pos) {
            const char *found = strstr(search, "<input");
            if (!found || found > id_pos) break;
            input_start = found;
            search = found + 6;
        }
        if (input_start) {
            value_pos = strstr(input_start, "value=\"");
        }
        if (!value_pos) return NULL;
    }

    /* 提取 value 内容 */
    value_pos += 7; /* 跳过 value=" */
    const char *end_quote = strchr(value_pos, '"');
    if (!end_quote) return NULL;

    size_t len = (size_t)(end_quote - value_pos);
    if (len == 0 || len > 256) return NULL;

    char *email = (char*)malloc(len + 1);
    if (!email) return NULL;
    memcpy(email, value_pos, len);
    email[len] = '\0';

    /* 基本验证：必须包含 @ */
    if (!strchr(email, '@')) {
        free(email);
        return NULL;
    }

    return email;
}

/**
 * 构建带 cookie 的请求头
 * 返回静态数组指针，非线程安全
 */
static const char** lroid_headers_with_cookie(const char *cookie) {
    static char cookie_hdr[4096];
    static const char *hdrs[8];

    int i = 0;
    hdrs[i++] = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8";
    hdrs[i++] = "Accept-Language: en-US,en;q=0.9";
    hdrs[i++] = "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/148.0.0.0 Safari/537.36";
    if (cookie && cookie[0]) {
        snprintf(cookie_hdr, sizeof(cookie_hdr), "Cookie: %s", cookie);
        hdrs[i++] = cookie_hdr;
    }
    hdrs[i++] = NULL;
    return (const char**)hdrs;
}

tm_email_info_t* tm_provider_lroid_generate(void) {
    /* 首次访问主页，获取自动分配的邮箱地址和 session cookie */
    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, LROID_BASE, lroid_headers, NULL, 15);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        TM_LOG_ERR("[lroid] 访问主页失败, status=%ld", resp ? resp->status : 0);
        tm_http_response_free(resp);
        return NULL;
    }

    /* 从 HTML 中提取邮箱地址 */
    char *email = lroid_extract_email_from_html(resp->body);
    if (!email) {
        TM_LOG_ERR("[lroid] 无法从 HTML 中提取邮箱地址");
        tm_http_response_free(resp);
        return NULL;
    }

    /* 保存 session cookie 作为 token */
    char *cookie = tm_strdup(resp->cookies ? resp->cookies : "");
    tm_http_response_free(resp);

    tm_email_info_t *info = tm_email_info_new();
    info->channel = CHANNEL_LROID;
    info->email = email;
    info->token = cookie;
    return info;
}

tm_email_t* tm_provider_lroid_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    if (!email || !email[0]) return NULL;

    /* 使用 session cookie 访问 kontrol API 获取邮件 */
    const char **hdrs = lroid_headers_with_cookie(token);

    char url[512];
    snprintf(url, sizeof(url), "%s/en/api-kontrol/", LROID_BASE);

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, hdrs, NULL, 15);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        TM_LOG_ERR("[lroid] kontrol API 请求失败, status=%ld", resp ? resp->status : 0);
        tm_http_response_free(resp);
        return NULL;
    }

    /* 尝试解析为 JSON */
    cJSON *json = cJSON_Parse(resp->body);
    if (!json) {
        /* 若非 JSON，检查是否为空响应（无邮件） */
        if (resp->body && (resp->body[0] == '\0' || strcmp(resp->body, "[]") == 0
            || strcmp(resp->body, "null") == 0 || strcmp(resp->body, "0") == 0)) {
            *count = 0;
            tm_http_response_free(resp);
            return NULL;
        }
        TM_LOG_ERR("[lroid] kontrol API 响应解析失败");
        tm_http_response_free(resp);
        return NULL;
    }
    tm_http_response_free(resp);

    /* 处理 JSON 数组格式 */
    cJSON *arr = json;
    if (!cJSON_IsArray(arr)) {
        /* 可能包裹在某个字段中 */
        arr = cJSON_GetObjectItemCaseSensitive(json, "data");
        if (!arr) arr = cJSON_GetObjectItemCaseSensitive(json, "mails");
        if (!arr) arr = cJSON_GetObjectItemCaseSensitive(json, "emails");
        if (!arr) arr = cJSON_GetObjectItemCaseSensitive(json, "messages");
        if (!arr || !cJSON_IsArray(arr)) {
            /* 若为空对象，返回 0 封邮件 */
            cJSON_Delete(json);
            *count = 0;
            return NULL;
        }
    }

    int n = cJSON_GetArraySize(arr);
    *count = n;
    if (n == 0) {
        cJSON_Delete(json);
        return NULL;
    }

    tm_email_t *emails = tm_emails_new(n);
    if (!emails) {
        cJSON_Delete(json);
        *count = -1;
        return NULL;
    }

    for (int i = 0; i < n; i++) {
        cJSON *item = cJSON_GetArrayItem(arr, i);
        emails[i] = tm_normalize_email(item, email);
    }

    cJSON_Delete(json);
    return emails;
}
