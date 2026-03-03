/**
 * mail.cx 渠道实现
 * API 文档: https://api.mail.cx/
 * 无需注册，任意邮箱名即可接收邮件
 */
#include "tempmail_internal.h"
#include <time.h>

#define MCX_BASE "https://api.mail.cx/api/v1"

static const char* mcx_domains[] = {"qabq.com", "nqmo.com", "end.tw", "uuf.me", "6n9.net"};
static const int mcx_domain_count = 5;

/*
 * 从 "name <email>" 格式中提取邮箱地址
 * 如 "openel <openel@foxmail.com>" → "openel@foxmail.com"
 */
static char* mcx_extract_email(const char *s) {
    if (!s) return tm_strdup("");
    const char *start = strchr(s, '<');
    const char *end = strchr(s, '>');
    if (start && end && end > start) {
        size_t len = end - start - 1;
        char *result = (char*)malloc(len + 1);
        memcpy(result, start + 1, len);
        result[len] = '\0';
        return result;
    }
    return tm_strdup(s);
}

static void mcx_random_username(char *buf, int len) {
    const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    for (int i = 0; i < len; i++) {
        buf[i] = chars[rand() % (sizeof(chars) - 1)];
    }
    buf[len] = '\0';
}

static char* mcx_get_token(void) {
    const char* headers[] = {
        "Content-Type: application/json",
        "Accept: application/json",
        NULL
    };

    tm_http_response_t *resp = tm_http_request(TM_HTTP_POST, MCX_BASE "/auth/authorize_token", headers, "{}", 15);
    if (!resp || resp->status != 200) { tm_http_response_free(resp); return NULL; }

    /* 去除可能的引号 */
    char *token = tm_strdup(resp->body);
    tm_http_response_free(resp);

    if (token && token[0] == '"') {
        size_t len = strlen(token);
        if (len > 2 && token[len-1] == '"') {
            memmove(token, token + 1, len - 2);
            token[len - 2] = '\0';
        }
    }

    return token;
}

tm_email_info_t* tm_provider_mail_cx_generate(void) {
    srand((unsigned)time(NULL));

    char *token = mcx_get_token();
    if (!token) return NULL;

    const char *domain = mcx_domains[rand() % mcx_domain_count];
    char username[9];
    mcx_random_username(username, 8);

    char address[256];
    snprintf(address, sizeof(address), "%s@%s", username, domain);

    tm_email_info_t *info = tm_email_info_new();
    info->channel = CHANNEL_MAIL_CX;
    info->email = tm_strdup(address);
    info->token = token;
    return info;
}

tm_email_t* tm_provider_mail_cx_get_emails(const char *token, const char *email, int *count) {
    *count = -1;

    /* mail.cx token 有效期很短（~5分钟），每次请求前刷新 */
    char *fresh_token = mcx_get_token();
    if (!fresh_token) return NULL;

    char auth[512];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", fresh_token);
    free(fresh_token);
    const char* headers[] = { auth, "Accept: application/json", NULL };

    char url[512];
    snprintf(url, sizeof(url), MCX_BASE "/mailbox/%s", email);

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, headers, NULL, 15);
    if (!resp || resp->status != 200) { tm_http_response_free(resp); return NULL; }

    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json || !cJSON_IsArray(json)) { cJSON_Delete(json); *count = 0; return NULL; }

    int n = cJSON_GetArraySize(json);
    *count = n;
    if (n == 0) { cJSON_Delete(json); return NULL; }

    tm_email_t *emails = tm_emails_new(n);
    for (int i = 0; i < n; i++) {
        cJSON *msg = cJSON_GetArrayItem(json, i);

        /* 扁平化 from/to 格式 */
        cJSON *from_item = cJSON_GetObjectItemCaseSensitive(msg, "from");
        char *from_email = mcx_extract_email(cJSON_GetStringValue(from_item));

        cJSON *to_arr = cJSON_GetObjectItemCaseSensitive(msg, "to");
        char *to_email = NULL;
        if (cJSON_IsArray(to_arr) && cJSON_GetArraySize(to_arr) > 0) {
            to_email = mcx_extract_email(cJSON_GetStringValue(cJSON_GetArrayItem(to_arr, 0)));
        } else {
            to_email = tm_strdup(email);
        }

        /* 构建扁平化的 JSON 对象 */
        cJSON *flat = cJSON_CreateObject();
        cJSON_AddStringToObject(flat, "id", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(msg, "id"), ""));
        cJSON_AddStringToObject(flat, "from", from_email);
        cJSON_AddStringToObject(flat, "to", to_email);
        cJSON_AddStringToObject(flat, "subject", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(msg, "subject"), ""));
        cJSON_AddStringToObject(flat, "text", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(msg, "text"), ""));
        cJSON_AddStringToObject(flat, "html", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(msg, "html"), ""));
        cJSON_AddStringToObject(flat, "date", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(msg, "date"), ""));
        cJSON *seen = cJSON_GetObjectItemCaseSensitive(msg, "seen");
        cJSON_AddBoolToObject(flat, "seen", cJSON_IsTrue(seen));

        emails[i] = tm_normalize_email(flat, email);
        cJSON_Delete(flat);
        free(from_email);
        free(to_email);
    }
    cJSON_Delete(json);
    return emails;
}
