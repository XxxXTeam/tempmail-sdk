/**
 * mail.gw 渠道实现
 * API 与 mail.tm 完全一致，仅 baseURL 不同
 */
#include "tempmail_internal.h"
#include <time.h>

#define MGW_BASE "https://api.mail.gw"

static const char* mgw_headers[] = {
    "Content-Type: application/json",
    "Accept: application/json",
    NULL
};

static void mgw_random_string(char *buf, int len) {
    const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    for (int i = 0; i < len; i++) {
        buf[i] = chars[rand() % (sizeof(chars) - 1)];
    }
    buf[len] = '\0';
}

tm_email_info_t* tm_provider_mail_gw_generate(void) {
    srand((unsigned)time(NULL));

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, MGW_BASE "/domains", mgw_headers, NULL, 15);
    if (!resp || resp->status != 200) { tm_http_response_free(resp); return NULL; }

    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) return NULL;

    cJSON *members = json;
    if (!cJSON_IsArray(json)) {
        members = cJSON_GetObjectItemCaseSensitive(json, "hydra:member");
    }
    if (!cJSON_IsArray(members) || cJSON_GetArraySize(members) == 0) { cJSON_Delete(json); return NULL; }

    char *domain = NULL;
    int sz = cJSON_GetArraySize(members);
    for (int i = 0; i < sz; i++) {
        cJSON *d = cJSON_GetArrayItem(members, i);
        if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(d, "isActive"))) {
            domain = tm_strdup(cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(d, "domain")));
            break;
        }
    }
    cJSON_Delete(json);
    if (!domain) return NULL;

    char username[13], password[17];
    mgw_random_string(username, 12);
    mgw_random_string(password, 16);
    char address[256];
    snprintf(address, sizeof(address), "%s@%s", username, domain);
    free(domain);

    char body[512];
    snprintf(body, sizeof(body), "{\"address\":\"%s\",\"password\":\"%s\"}", address, password);
    const char* create_headers[] = {
        "Content-Type: application/ld+json",
        "Accept: application/json",
        NULL
    };
    resp = tm_http_request(TM_HTTP_POST, MGW_BASE "/accounts", create_headers, body, 15);
    if (!resp || resp->status < 200 || resp->status >= 300) { tm_http_response_free(resp); return NULL; }
    tm_http_response_free(resp);

    resp = tm_http_request(TM_HTTP_POST, MGW_BASE "/token", mgw_headers, body, 15);
    if (!resp || resp->status != 200) { tm_http_response_free(resp); return NULL; }

    json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) return NULL;

    const char *token = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(json, "token"));
    if (!token) { cJSON_Delete(json); return NULL; }

    tm_email_info_t *info = tm_email_info_new();
    info->channel = CHANNEL_MAIL_GW;
    info->email = tm_strdup(address);
    info->token = tm_strdup(token);
    cJSON_Delete(json);
    return info;
}

tm_email_t* tm_provider_mail_gw_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    char auth[512];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", token);
    const char* headers[] = { auth, "Accept: application/json", NULL };

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, MGW_BASE "/messages", headers, NULL, 15);
    if (!resp || resp->status != 200) { tm_http_response_free(resp); return NULL; }

    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) return NULL;

    cJSON *messages = json;
    if (!cJSON_IsArray(json)) {
        messages = cJSON_GetObjectItemCaseSensitive(json, "hydra:member");
    }
    int n = cJSON_IsArray(messages) ? cJSON_GetArraySize(messages) : 0;
    *count = n;
    if (n == 0) { cJSON_Delete(json); return NULL; }

    tm_email_t *emails = tm_emails_new(n);
    for (int i = 0; i < n; i++) {
        cJSON *msg = cJSON_GetArrayItem(messages, i);
        const char *mid = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(msg, "id"));
        if (mid) {
            char detail_url[512];
            snprintf(detail_url, sizeof(detail_url), MGW_BASE "/messages/%s", mid);
            tm_http_response_t *dr = tm_http_request(TM_HTTP_GET, detail_url, headers, NULL, 15);
            if (dr && dr->status == 200) {
                cJSON *detail = cJSON_Parse(dr->body);
                if (detail) {
                    emails[i] = tm_normalize_email(detail, email);
                    cJSON_Delete(detail);
                    tm_http_response_free(dr);
                    continue;
                }
            }
            tm_http_response_free(dr);
        }
        emails[i] = tm_normalize_email(msg, email);
    }
    cJSON_Delete(json);
    return emails;
}
