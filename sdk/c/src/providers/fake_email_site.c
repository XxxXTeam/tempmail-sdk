/**
 * FakeEmailSite -- https://fake-email.site
 */

#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FAKE_EMAIL_SITE_BASE "https://api.fake-email.site"

static const char *fake_email_site_headers[] = {
    "Accept: application/json",
    "Content-Type: application/json",
    NULL
};

/**
 * 创建临时邮箱
 * POST /api/temporary-address body: {}
 * 返回 {"temp_email_addr":"xxx@fake-email.site"}
 */
tm_email_info_t* tm_provider_fake_email_site_generate(void) {
    tm_http_response_t *resp = tm_http_request(
        TM_HTTP_POST,
        FAKE_EMAIL_SITE_BASE "/api/temporary-address",
        fake_email_site_headers,
        "{}",
        15
    );
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }

    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!root) return NULL;

    const char *email = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "temp_email_addr"), "");
    if (!email[0] || !strchr(email, '@')) {
        cJSON_Delete(root);
        return NULL;
    }

    tm_email_info_t *info = tm_email_info_new();
    if (!info) {
        cJSON_Delete(root);
        return NULL;
    }
    info->channel = CHANNEL_FAKE_EMAIL_SITE;
    info->email = tm_strdup(email);
    info->token = tm_strdup(email);
    cJSON_Delete(root);
    return info;
}

/**
 * 对字符串进行 URL 编码
 */
static char *fake_email_site_urlencode(const char *s) {
    if (!s) return tm_strdup("");
    size_t len = strlen(s);
    char *out = (char*)malloc(len * 3 + 1);
    if (!out) return NULL;
    char *p = out;
    static const char hex[] = "0123456789ABCDEF";
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~' || c == '@') {
            *p++ = (char)c;
        } else {
            *p++ = '%';
            *p++ = hex[c >> 4];
            *p++ = hex[c & 15];
        }
    }
    *p = '\0';
    return out;
}

/**
 * 获取邮件列表
 * GET /api/inbox/poll?address=xxx
 * 返回 {"temp_email_addr":"xxx","new_mail_count":0,"next_since":null,"messages":[]}
 * 消息字段: id, from_addr, to_addr, subject, body_text, received_at
 * 只有 body_text 没有原生 html，tm_normalize_email 会自动补 html
 * 处理 404 返回 NULL 且 count=0
 */
tm_email_t* tm_provider_fake_email_site_get_emails(const char *email, int *count) {
    *count = -1;
    if (!email || !email[0]) return NULL;

    char *enc = fake_email_site_urlencode(email);
    if (!enc) return NULL;
    size_t cap = strlen(FAKE_EMAIL_SITE_BASE) + strlen(enc) + 64;
    char *url = (char*)malloc(cap);
    if (!url) {
        free(enc);
        return NULL;
    }
    snprintf(url, cap, "%s/api/inbox/poll?address=%s", FAKE_EMAIL_SITE_BASE, enc);
    free(enc);

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, fake_email_site_headers, NULL, 15);
    free(url);
    if (!resp) return NULL;

    /* 处理 404 返回空 */
    if (resp->status == 404) {
        tm_http_response_free(resp);
        *count = 0;
        return NULL;
    }

    if (resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }

    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!root) return NULL;

    cJSON *arr = cJSON_GetObjectItemCaseSensitive(root, "messages");
    int n = (arr && cJSON_IsArray(arr)) ? cJSON_GetArraySize(arr) : 0;
    *count = n;
    if (n == 0) {
        cJSON_Delete(root);
        return NULL;
    }

    tm_email_t *emails = tm_emails_new(n);
    if (!emails) {
        *count = -1;
        cJSON_Delete(root);
        return NULL;
    }

    for (int i = 0; i < n; i++) {
        cJSON *raw = cJSON_GetArrayItem(arr, i);
        emails[i] = tm_normalize_email(raw, email);
    }
    cJSON_Delete(root);
    return emails;
}
