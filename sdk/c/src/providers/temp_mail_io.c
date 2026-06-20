/**
 * temp-mail.io -- https://temp-mail.io
 * REST API: https://api.internal.temp-mail.io/api/v3
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TMIO_BASE "https://api.internal.temp-mail.io/api/v3"
#define TMIO_PAGE "https://temp-mail.io/en"

static char g_tmio_cors_header[128] = "";

static void tmio_try_cache_cors_header(const char *body) {
    const char *p = body ? strstr(body, "mobileTestingHeader") : NULL;
    if (!p) return;

    p = strchr(p, ':');
    if (!p) return;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (*p != '"') return;

    const char *start = p + 1;
    const char *end = strchr(start, '"');
    if (!end || end <= start) return;

    size_t len = (size_t)(end - start);
    if (len >= sizeof(g_tmio_cors_header)) {
        len = sizeof(g_tmio_cors_header) - 1;
    }
    memcpy(g_tmio_cors_header, start, len);
    g_tmio_cors_header[len] = '\0';
}

static const char *tmio_cors_header(void) {
    if (g_tmio_cors_header[0]) {
        return g_tmio_cors_header;
    }

    const char *headers[] = {
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36",
        NULL
    };
    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, TMIO_PAGE, headers, NULL, 15);
    if (resp && resp->body) {
        tmio_try_cache_cors_header(resp->body);
    }
    tm_http_response_free(resp);

    if (!g_tmio_cors_header[0]) {
        strcpy(g_tmio_cors_header, "1");
    }
    return g_tmio_cors_header;
}

static void tmio_headers(const char **headers, char *cors_line, size_t cors_line_cap) {
    snprintf(cors_line, cors_line_cap, "X-CORS-Header: %s", tmio_cors_header());
    headers[0] = "Content-Type: application/json";
    headers[1] = "Application-Name: web";
    headers[2] = "Application-Version: 4.0.0";
    headers[3] = cors_line;
    headers[4] = "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36 Edg/144.0.0.0";
    headers[5] = "Origin: https://temp-mail.io";
    headers[6] = "Referer: https://temp-mail.io/";
    headers[7] = NULL;
}

tm_email_info_t* tm_provider_temp_mail_io_generate(void) {
    char cors_line[192];
    const char *headers[8];
    tmio_headers(headers, cors_line, sizeof(cors_line));

    const char *body = "{\"min_name_length\":10,\"max_name_length\":10}";
    tm_http_response_t *resp = tm_http_request(TM_HTTP_POST, TMIO_BASE "/email/new", headers, body, 15);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }

    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!root) return NULL;

    const char *email = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "email"), "");
    const char *token = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "token"), "");
    if (!email[0] || !token[0]) {
        cJSON_Delete(root);
        return NULL;
    }

    tm_email_info_t *info = tm_email_info_new();
    if (!info) {
        cJSON_Delete(root);
        return NULL;
    }
    info->channel = CHANNEL_TEMP_MAIL_IO;
    info->email = tm_strdup(email);
    info->token = tm_strdup(token);

    cJSON_Delete(root);
    return info;
}

tm_email_t* tm_provider_temp_mail_io_get_emails(const char *email, int *count) {
    *count = -1;
    if (!email || !email[0]) return NULL;

    size_t url_len = strlen(TMIO_BASE) + strlen("/email//messages") + strlen(email) + 1;
    char *url = (char*)malloc(url_len);
    if (!url) return NULL;
    snprintf(url, url_len, "%s/email/%s/messages", TMIO_BASE, email);

    char cors_line[192];
    const char *headers[8];
    tmio_headers(headers, cors_line, sizeof(cors_line));

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, headers, NULL, 15);
    free(url);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }

    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!root) return NULL;

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
        *count = -1;
        cJSON_Delete(root);
        return NULL;
    }

    for (int i = 0; i < n; i++) {
        cJSON *raw = cJSON_GetArrayItem(root, i);
        emails[i] = tm_normalize_email(raw, email);
    }

    cJSON_Delete(root);
    return emails;
}
