/**
 * mail.cx 渠道实现
 * API 文档: https://api.mail.cx/
 * 无需注册，任意邮箱名即可接收邮件
 */
#include "tempmail_internal.h"
#include <time.h>

#define MCX_BASE "https://api.mail.cx/api/v1"

static const char* mcx_domains[] = {"qabq.com", "nqmo.com"};
static const int mcx_domain_count = 2;

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
    char auth[512];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", token);
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
        emails[i] = tm_normalize_email(msg, email);
    }
    cJSON_Delete(json);
    return emails;
}
