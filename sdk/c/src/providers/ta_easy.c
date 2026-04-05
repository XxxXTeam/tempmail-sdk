/**
 * ta-easy.com 临时邮箱
 */
#include "tempmail_internal.h"

#define TA_EASY_API "https://api-endpoint.ta-easy.com"
#define TA_EASY_ORIGIN "https://www.ta-easy.com"

static const char *ta_easy_generate_headers[] = {
    "Accept: application/json",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    "origin: " TA_EASY_ORIGIN,
    "referer: " TA_EASY_ORIGIN "/",
    NULL
};

static const char *ta_easy_json_headers[] = {
    "Accept: application/json",
    "Content-Type: application/json",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    "origin: " TA_EASY_ORIGIN,
    "referer: " TA_EASY_ORIGIN "/",
    NULL
};

tm_email_info_t* tm_provider_ta_easy_generate(void) {
    tm_http_response_t *resp = tm_http_request(
        TM_HTTP_POST,
        TA_EASY_API "/temp-email/address/new",
        ta_easy_generate_headers,
        "",
        15
    );
    if (!resp || resp->status != 200) {
        tm_http_response_free(resp);
        return NULL;
    }

    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) return NULL;

    cJSON *st = cJSON_GetObjectItemCaseSensitive(json, "status");
    if (!cJSON_IsString(st) || strcmp(st->valuestring, "success") != 0) {
        cJSON_Delete(json);
        return NULL;
    }

    const char *addr = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(json, "address"));
    const char *tk = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(json, "token"));
    if (!addr || !tk) {
        cJSON_Delete(json);
        return NULL;
    }

    tm_email_info_t *info = tm_email_info_new();
    info->channel = CHANNEL_TA_EASY;
    info->email = tm_strdup(addr);
    info->token = tm_strdup(tk);

    cJSON *exp = cJSON_GetObjectItemCaseSensitive(json, "expiresAt");
    if (cJSON_IsNumber(exp)) {
        info->expires_at = (long long) exp->valuedouble;
    }

    cJSON_Delete(json);
    return info;
}

tm_email_t* tm_provider_ta_easy_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    if (!token || !email) return NULL;

    cJSON *root = cJSON_CreateObject();
    if (!root) return NULL;
    cJSON_AddStringToObject(root, "token", token);
    cJSON_AddStringToObject(root, "email", email);
    char *body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!body) return NULL;

    tm_http_response_t *resp = tm_http_request(
        TM_HTTP_POST,
        TA_EASY_API "/temp-email/inbox/list",
        ta_easy_json_headers,
        body,
        15
    );
    free(body);

    if (!resp || resp->status != 200) {
        tm_http_response_free(resp);
        return NULL;
    }

    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) return NULL;

    cJSON *st = cJSON_GetObjectItemCaseSensitive(json, "status");
    if (!cJSON_IsString(st) || strcmp(st->valuestring, "success") != 0) {
        cJSON_Delete(json);
        return NULL;
    }

    cJSON *data = cJSON_GetObjectItemCaseSensitive(json, "data");
    if (!cJSON_IsArray(data)) {
        cJSON_Delete(json);
        *count = 0;
        return NULL;
    }

    int n = cJSON_GetArraySize(data);
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
        emails[i] = tm_normalize_email(cJSON_GetArrayItem(data, i), email);
    }
    cJSON_Delete(json);
    return emails;
}
