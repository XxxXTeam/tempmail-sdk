/**
 * HarakiriMail -- https://harakirimail.com
 * 无需认证、无需 Cookie、无需 Token 的简单 REST API 渠道
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define HK_BASE "https://harakirimail.com"

static const char *hk_headers[] = {
    "Accept: application/json, text/plain, */*",
    "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    NULL
};

/* 随机生成 12 位字母数字字符串作为收件箱名 */
static void hk_random_name(char *buf, size_t cap) {
    static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    size_t len = 12;
    if (cap < len + 1) len = cap - 1;
    for (size_t i = 0; i < len; i++) {
        buf[i] = chars[rand() % (sizeof(chars) - 1)];
    }
    buf[len] = '\0';
}

tm_email_info_t* tm_provider_harakirimail_generate(void) {
    srand((unsigned)time(NULL));

    char name[16];
    hk_random_name(name, sizeof(name));

    char url[256];
    snprintf(url, sizeof(url), "%s/api/v1/inbox/%s", HK_BASE, name);

    /* 可选：调用收件箱接口验证地址可用 */
    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, hk_headers, NULL, 15);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }
    tm_http_response_free(resp);

    char email[128];
    snprintf(email, sizeof(email), "%s@harakirimail.com", name);

    tm_email_info_t *info = tm_email_info_new();
    info->channel = CHANNEL_HARAKIRIMAIL;
    info->email = tm_strdup(email);
    info->token = NULL;
    return info;
}

/* 获取单封邮件正文 */
static void hk_fetch_body(const char *id, char **html_out, char **text_out) {
    *html_out = NULL;
    *text_out = NULL;
    if (!id || !id[0]) return;

    char url[512];
    snprintf(url, sizeof(url), "%s/api/v1/email/%s", HK_BASE, id);

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, hk_headers, NULL, 15);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return;
    }

    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!root) return;

    cJSON *bh = cJSON_GetObjectItemCaseSensitive(root, "body_html");
    if (!cJSON_IsString(bh) || !bh->valuestring || !bh->valuestring[0]) {
        bh = cJSON_GetObjectItemCaseSensitive(root, "html");
    }
    if (cJSON_IsString(bh) && bh->valuestring) {
        *html_out = tm_strdup(bh->valuestring);
    }

    cJSON *bt = cJSON_GetObjectItemCaseSensitive(root, "body_text");
    if (!cJSON_IsString(bt) || !bt->valuestring || !bt->valuestring[0]) {
        bt = cJSON_GetObjectItemCaseSensitive(root, "text");
    }
    if (cJSON_IsString(bt) && bt->valuestring) {
        *text_out = tm_strdup(bt->valuestring);
    }

    cJSON_Delete(root);
}

tm_email_t* tm_provider_harakirimail_get_emails(const char *email, int *count) {
    *count = -1;
    if (!email || !email[0]) return NULL;

    /* 从邮箱地址提取收件箱名 */
    char name[256];
    const char *at = strchr(email, '@');
    if (at && at > email) {
        size_t len = (size_t)(at - email);
        if (len >= sizeof(name)) len = sizeof(name) - 1;
        memcpy(name, email, len);
        name[len] = '\0';
    } else {
        strncpy(name, email, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
    }

    char url[512];
    snprintf(url, sizeof(url), "%s/api/v1/inbox/%s", HK_BASE, name);

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, hk_headers, NULL, 15);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }

    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!root) return NULL;

    cJSON *arr = cJSON_GetObjectItemCaseSensitive(root, "emails");
    if (!arr || !cJSON_IsArray(arr)) {
        *count = 0;
        cJSON_Delete(root);
        return NULL;
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
        const char *id = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(raw, "_id"), "");

        /* 获取单封邮件详情以拿到正文 */
        char *html_body = NULL;
        char *text_body = NULL;
        hk_fetch_body(id, &html_body, &text_body);

        /* 将正文注入到 raw 对象中以便 normalize */
        if (html_body) cJSON_AddStringToObject(raw, "html", html_body);
        if (text_body) cJSON_AddStringToObject(raw, "text", text_body);
        /* 映射 _id → id，received → date */
        if (id[0]) cJSON_AddStringToObject(raw, "id", id);
        const char *recv = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(raw, "received"), "");
        if (recv[0]) cJSON_AddStringToObject(raw, "date", recv);

        emails[i] = tm_normalize_email(raw, email);

        free(html_body);
        free(text_body);
    }

    cJSON_Delete(root);
    return emails;
}
