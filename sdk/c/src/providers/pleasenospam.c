/**
 * PleeaseNoSpam -- https://pleasenospam.email
 * 无需认证的简单 REST API 渠道
 * 支持域名: pleasenospam.email, spamlessmail.org
 * 创建邮箱: 本地生成随机地址，无需调用 API
 * 获取邮件: GET /{email}.json 返回 JSON 数组
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PNS_BASE "https://pleasenospam.email"

static const char *pns_headers[] = {
    "Accept: application/json, text/plain, */*",
    "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    NULL
};

/* 随机生成 10 位小写字母数字字符串作为邮箱本地部分 */
static void pns_random_local(char *buf, size_t cap) {
    static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    size_t len = 10;
    if (cap < len + 1) len = cap - 1;
    for (size_t i = 0; i < len; i++) {
        buf[i] = chars[rand() % (sizeof(chars) - 1)];
    }
    buf[len] = '\0';
}

/**
 * 创建 pleasenospam 邮箱
 * 无需 API 调用，直接生成 {randomLocal}@pleasenospam.email
 */
tm_email_info_t* tm_provider_pleasenospam_generate(void) {
    srand((unsigned)time(NULL));

    char local[16];
    pns_random_local(local, sizeof(local));

    char email[128];
    snprintf(email, sizeof(email), "%s@pleasenospam.email", local);

    tm_email_info_t *info = tm_email_info_new();
    if (!info) return NULL;

    info->channel = CHANNEL_PLEASENOSPAM;
    info->email = tm_strdup(email);
    info->token = NULL;
    return info;
}

/**
 * 获取 pleasenospam 邮件列表
 * API: GET https://pleasenospam.email/{email}.json
 * 返回 JSON 数组: [{"id":"xxx","from":["sender@xxx.com"],"subject":"...","receivedDate":1234567890,"text":"...","html":"..."}]
 * 注意: from 字段是数组，取 from[0] 作为发件人地址
 */
tm_email_t* tm_provider_pleasenospam_get_emails(const char *email, int *count) {
    *count = -1;
    if (!email || !email[0]) return NULL;

    char url[512];
    snprintf(url, sizeof(url), "%s/%s.json", PNS_BASE, email);

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, pns_headers, NULL, 15);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }

    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!root) return NULL;

    /* API 直接返回 JSON 数组 */
    if (!cJSON_IsArray(root)) {
        *count = 0;
        cJSON_Delete(root);
        return NULL;
    }

    int n = cJSON_GetArraySize(root);
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
        cJSON *raw = cJSON_GetArrayItem(root, i);

        /* from 字段是数组，提取 from[0] 注入为 from_addr 供 normalize 使用 */
        cJSON *from_arr = cJSON_GetObjectItemCaseSensitive(raw, "from");
        if (cJSON_IsArray(from_arr) && cJSON_GetArraySize(from_arr) > 0) {
            cJSON *first = cJSON_GetArrayItem(from_arr, 0);
            if (cJSON_IsString(first) && first->valuestring) {
                cJSON_AddStringToObject(raw, "from_addr", first->valuestring);
            }
        }

        /* receivedDate (秒级时间戳) 映射为 date 供 normalize 使用 */
        cJSON *rd = cJSON_GetObjectItemCaseSensitive(raw, "receivedDate");
        if (cJSON_IsNumber(rd) && rd->valuedouble > 0) {
            cJSON_AddNumberToObject(raw, "date", rd->valuedouble);
        }

        emails[i] = tm_normalize_email(raw, email);
    }

    cJSON_Delete(root);
    return emails;
}
