/**
 * temp-mail.io 渠道实现
 */
#include "tempmail_internal.h"

#define TMIO_BASE "https://api.internal.temp-mail.io/api/v3"

static const char* tmio_headers[] = {
    "Content-Type: application/json",
    "Application-Name: web",
    "Application-Version: 4.0.0",
    "X-CORS-Header: 1",
    "origin: https://temp-mail.io",
    "referer: https://temp-mail.io/",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36 Edg/144.0.0.0",
    NULL
};

tm_email_info_t* tm_provider_temp_mail_io_generate(void) {
    tm_http_response_t *resp = tm_http_request(
        TM_HTTP_POST, TMIO_BASE "/email/new", tmio_headers,
        "{\"min_name_length\":10,\"max_name_length\":10}", 15
    );
    if (!resp || resp->status != 200) { tm_http_response_free(resp); return NULL; }

    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) return NULL;

    const char *em = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(json, "email"));
    const char *tk = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(json, "token"));
    if (!em || !tk) { cJSON_Delete(json); return NULL; }

    tm_email_info_t *info = tm_email_info_new();
    info->channel = CHANNEL_TEMP_MAIL_IO;
    info->email = tm_strdup(em);
    info->token = tm_strdup(tk);
    cJSON_Delete(json);
    return info;
}

tm_email_t* tm_provider_temp_mail_io_get_emails(const char *email, int *count) {
    *count = -1;
    char url[512];
    snprintf(url, sizeof(url), TMIO_BASE "/email/%s/messages", email);
    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, tmio_headers, NULL, 15);
    if (!resp || resp->status != 200) { tm_http_response_free(resp); return NULL; }

    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) return NULL;

    if (!cJSON_IsArray(json)) { cJSON_Delete(json); *count = 0; return NULL; }

    int n = cJSON_GetArraySize(json);
    *count = n;
    if (n == 0) { cJSON_Delete(json); return NULL; }

    tm_email_t *emails = tm_emails_new(n);
    for (int i = 0; i < n; i++) emails[i] = tm_normalize_email(cJSON_GetArrayItem(json, i), email);
    cJSON_Delete(json);
    return emails;
}
