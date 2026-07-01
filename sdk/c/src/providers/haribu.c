/**
 * Haribu -- https://haribu.net
 * Tempail 类模式：从首页 HTML 提取 <input id="eposta_adres" value="..."> 获取邮箱地址
 * 通过 api-kontrol 接口轮询邮件，依赖 session cookies
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HARIBU_BASE "https://haribu.net"

static const char *haribu_headers[] = {
    "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
    "Accept-Language: en-US,en;q=0.9",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    NULL
};

static const char *haribu_api_headers[] = {
    "Accept: application/json, text/javascript, */*; q=0.01",
    "Accept-Language: en-US,en;q=0.9",
    "X-Requested-With: XMLHttpRequest",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    NULL
};

/**
 * 从 HTML 中提取 <input id="eposta_adres" value="xxx@yyy.com"> 的 value 值
 * 使用 strstr 进行简单字符串搜索
 */
static char* haribu_extract_email(const char *html) {
    if (!html) return NULL;

    /* 查找包含 eposta_adres 的 input 元素 */
    const char *pos = strstr(html, "eposta_adres");
    if (!pos) return NULL;

    /* 在该元素附近查找 value=" */
    const char *search_end = pos + 512;  /* 限制搜索范围 */
    const char *p = pos;
    const char *val_start = NULL;

    while (p < search_end && *p) {
        if (strncmp(p, "value=\"", 7) == 0) {
            val_start = p + 7;
            break;
        }
        if (strncmp(p, "value='", 7) == 0) {
            val_start = p + 7;
            break;
        }
        p++;
    }

    if (!val_start) {
        /* 也尝试在 eposta_adres 之前查找 value（属性顺序不固定） */
        p = pos - 256;
        if (p < html) p = html;
        const char *last_val = NULL;
        while (p < pos) {
            if (strncmp(p, "value=\"", 7) == 0) {
                last_val = p + 7;
            } else if (strncmp(p, "value='", 7) == 0) {
                last_val = p + 7;
            }
            p++;
        }
        val_start = last_val;
    }

    if (!val_start) return NULL;

    /* 找到结束引号 */
    const char *val_end = strchr(val_start, '"');
    if (!val_end) val_end = strchr(val_start, '\'');
    if (!val_end || val_end == val_start) return NULL;

    /* 验证提取的值包含 @ 符号（确认是邮箱地址） */
    size_t len = (size_t)(val_end - val_start);
    char *email = (char*)malloc(len + 1);
    if (!email) return NULL;
    memcpy(email, val_start, len);
    email[len] = '\0';

    if (!strchr(email, '@')) {
        free(email);
        return NULL;
    }

    return email;
}

tm_email_info_t* tm_provider_haribu_generate(void) {
    /* 访问首页获取 session cookie 和邮箱地址 */
    tm_http_response_t *resp = tm_http_request(
        TM_HTTP_GET, HARIBU_BASE "/en/", haribu_headers, NULL, 15);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        TM_LOG_ERR("[haribu] 首页请求失败, status=%ld", resp ? resp->status : 0);
        tm_http_response_free(resp);
        return NULL;
    }

    /* 从 HTML 中提取邮箱地址 */
    char *email = haribu_extract_email(resp->body);
    if (!email) {
        TM_LOG_ERR("[haribu] 无法从 HTML 中提取邮箱地址");
        tm_http_response_free(resp);
        return NULL;
    }

    /* 保存 session cookies 作为 token */
    char *cookies = resp->cookies ? tm_strdup(resp->cookies) : NULL;
    tm_http_response_free(resp);

    if (!cookies || !cookies[0]) {
        TM_LOG_ERR("[haribu] 未获取到 session cookies");
        free(email);
        free(cookies);
        return NULL;
    }

    tm_email_info_t *info = tm_email_info_new();
    info->channel = CHANNEL_HARIBU;
    info->email = email;
    info->token = cookies;
    return info;
}

tm_email_t* tm_provider_haribu_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    if (!token || !token[0] || !email || !email[0]) return NULL;

    /* 构建带 Cookie 头的请求头数组 */
    char cookie_header[2048];
    snprintf(cookie_header, sizeof(cookie_header), "Cookie: %s", token);

    const char *headers[] = {
        "Accept: application/json, text/javascript, */*; q=0.01",
        "Accept-Language: en-US,en;q=0.9",
        "X-Requested-With: XMLHttpRequest",
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
            "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
        cookie_header,
        NULL
    };

    /* 调用 kontrol API 检查新邮件 */
    tm_http_response_t *resp = tm_http_request(
        TM_HTTP_GET, HARIBU_BASE "/en/api-kontrol/", headers, NULL, 15);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        TM_LOG_ERR("[haribu] kontrol API 请求失败, status=%ld", resp ? resp->status : 0);
        tm_http_response_free(resp);
        return NULL;
    }

    /* 尝试将 kontrol 响应解析为 JSON 数组 */
    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);

    if (!root) {
        /* kontrol 返回非 JSON（可能是空页面或 "yok" 表示无邮件） */
        *count = 0;
        return NULL;
    }

    /* 处理 JSON 数组格式的邮件列表 */
    cJSON *arr = root;
    if (!cJSON_IsArray(arr)) {
        /* 尝试从对象中提取邮件数组 */
        arr = cJSON_GetObjectItemCaseSensitive(root, "emails");
        if (!arr) arr = cJSON_GetObjectItemCaseSensitive(root, "data");
        if (!arr) arr = cJSON_GetObjectItemCaseSensitive(root, "mails");
        if (!arr || !cJSON_IsArray(arr)) {
            *count = 0;
            cJSON_Delete(root);
            return NULL;
        }
    }

    int n = cJSON_GetArraySize(arr);
    *count = n;
    if (n == 0) {
        cJSON_Delete(root);
        return NULL;
    }

    tm_email_t *emails = tm_emails_new(n);
    if (!emails) {
        cJSON_Delete(root);
        *count = -1;
        return NULL;
    }

    for (int i = 0; i < n; i++) {
        cJSON *raw = cJSON_GetArrayItem(arr, i);

        /* 将 haribu 特有字段映射为标准字段 */
        const char *mail_from = TM_JSON_STR(
            cJSON_GetObjectItemCaseSensitive(raw, "mail_from"), "");
        if (!mail_from[0]) {
            mail_from = TM_JSON_STR(
                cJSON_GetObjectItemCaseSensitive(raw, "from"), "");
        }
        if (mail_from[0] && !cJSON_GetObjectItemCaseSensitive(raw, "from")) {
            cJSON_AddStringToObject(raw, "from", mail_from);
        }

        const char *mail_subject = TM_JSON_STR(
            cJSON_GetObjectItemCaseSensitive(raw, "mail_subject"), "");
        if (!mail_subject[0]) {
            mail_subject = TM_JSON_STR(
                cJSON_GetObjectItemCaseSensitive(raw, "subject"), "");
        }
        if (mail_subject[0] && !cJSON_GetObjectItemCaseSensitive(raw, "subject")) {
            cJSON_AddStringToObject(raw, "subject", mail_subject);
        }

        const char *mail_text = TM_JSON_STR(
            cJSON_GetObjectItemCaseSensitive(raw, "mail_text"), "");
        if (mail_text[0] && !cJSON_GetObjectItemCaseSensitive(raw, "text")) {
            cJSON_AddStringToObject(raw, "text", mail_text);
        }

        const char *mail_html = TM_JSON_STR(
            cJSON_GetObjectItemCaseSensitive(raw, "mail_html"), "");
        if (!mail_html[0]) {
            mail_html = TM_JSON_STR(
                cJSON_GetObjectItemCaseSensitive(raw, "html"), "");
        }
        if (mail_html[0] && !cJSON_GetObjectItemCaseSensitive(raw, "html")) {
            cJSON_AddStringToObject(raw, "html", mail_html);
        }

        emails[i] = tm_normalize_email(raw, email);
    }

    cJSON_Delete(root);
    return emails;
}
