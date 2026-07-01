/**
 * TempMail Plus -- https://tempmail.plus
 * 无需认证、无需 Cookie、无需 Token 的简单 REST API 渠道
 * 域名: mailto.plus
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TMP_BASE "https://tempmail.plus/api/mails"
#define TMP_DEFAULT_DOMAIN "mailto.plus"

static const char *tmp_headers[] = {
    "Accept: application/json",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
    "Referer: https://tempmail.plus/",
    "Origin: https://tempmail.plus",
    NULL
};

/* 随机生成 12 位字母数字字符串作为本地部分 */
static void tmp_random_local(char *buf, size_t cap) {
    static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    size_t len = 12;
    if (cap < len + 1) len = cap - 1;
    for (size_t i = 0; i < len; i++) {
        buf[i] = chars[rand() % (sizeof(chars) - 1)];
    }
    buf[len] = '\0';
}

static void tmp_seed_random_once(void) {
    static int seeded = 0;
    if (!seeded) {
        srand((unsigned int)time(NULL) ^ (unsigned int)clock());
        seeded = 1;
    }
}

tm_email_info_t* tm_provider_tempmail_plus_generate(const char *domain, tm_channel_t channel) {
    tmp_seed_random_once();
    const char *selected_domain = (domain && domain[0]) ? domain : TMP_DEFAULT_DOMAIN;

    char local[16];
    tmp_random_local(local, sizeof(local));

    char email[128];
    snprintf(email, sizeof(email), "%s@%s", local, selected_domain);

    char url[512];
    snprintf(url, sizeof(url), "%s/?email=%s&epin=", TMP_BASE, email);

    /* 调用列表接口验证地址可用 */
    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, tmp_headers, NULL, 15);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }
    tm_http_response_free(resp);

    tm_email_info_t *info = tm_email_info_new();
    info->channel = channel;
    info->email = tm_strdup(email);
    info->token = NULL;
    return info;
}

/* 获取单封邮件正文 */
static void tmp_fetch_body(int mail_id, const char *email, char **html_out, char **text_out) {
    *html_out = NULL;
    *text_out = NULL;
    if (mail_id == 0) return;

    char url[512];
    snprintf(url, sizeof(url), "%s/%d?email=%s&epin=", TMP_BASE, mail_id, email);

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, tmp_headers, NULL, 15);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return;
    }

    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!root) return;

    cJSON *h = cJSON_GetObjectItemCaseSensitive(root, "html");
    if (cJSON_IsString(h) && h->valuestring) {
        *html_out = tm_strdup(h->valuestring);
    }

    cJSON *t = cJSON_GetObjectItemCaseSensitive(root, "text");
    if (cJSON_IsString(t) && t->valuestring) {
        *text_out = tm_strdup(t->valuestring);
    }

    cJSON_Delete(root);
}

tm_email_t* tm_provider_tempmail_plus_get_emails(const char *email, int *count) {
    *count = -1;
    if (!email || !email[0]) return NULL;

    char url[512];
    snprintf(url, sizeof(url), "%s/?email=%s&epin=", TMP_BASE, email);

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, tmp_headers, NULL, 15);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }

    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!root) return NULL;

    cJSON *arr = cJSON_GetObjectItemCaseSensitive(root, "mail_list");
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
        cJSON *mid = cJSON_GetObjectItemCaseSensitive(raw, "mail_id");
        int mail_id = cJSON_IsNumber(mid) ? (int)mid->valuedouble : 0;

        /* 获取单封邮件详情以拿到正文 */
        char *html_body = NULL;
        char *text_body = NULL;
        tmp_fetch_body(mail_id, email, &html_body, &text_body);

        /* 将正文和标准化字段注入到 raw 对象中以便 normalize */
        if (html_body) cJSON_AddStringToObject(raw, "html", html_body);
        if (text_body) cJSON_AddStringToObject(raw, "text", text_body);

        /* 映射 mail_id → id，from_mail → from，time → date */
        char id_str[32];
        snprintf(id_str, sizeof(id_str), "%d", mail_id);
        cJSON_AddStringToObject(raw, "id", id_str);

        const char *from_mail = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(raw, "from_mail"), "");
        if (from_mail[0]) cJSON_AddStringToObject(raw, "from", from_mail);

        const char *time_str = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(raw, "time"), "");
        if (time_str[0]) cJSON_AddStringToObject(raw, "date", time_str);

        /* is_new → isRead 取反 */
        cJSON *is_new = cJSON_GetObjectItemCaseSensitive(raw, "is_new");
        int is_read = (is_new && cJSON_IsTrue(is_new)) ? 0 : 1;
        cJSON_AddBoolToObject(raw, "isRead", is_read);

        emails[i] = tm_normalize_email(raw, email);

        free(html_body);
        free(text_body);
    }

    cJSON_Delete(root);
    return emails;
}
