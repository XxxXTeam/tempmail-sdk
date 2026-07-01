/**
 * shitty.email -- https://shitty.email
 */

#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SHITTY_EMAIL_BASE "https://shitty.email/api"

static const char *shitty_email_headers[] = {
    "Accept: application/json",
    "User-Agent: Mozilla/5.0",
    NULL
};

static char **shitty_email_token_headers(const char *token) {
    char **headers = (char**)calloc(4, sizeof(char*));
    if (!headers) return NULL;
    headers[0] = tm_strdup("Accept: application/json");
    headers[1] = tm_strdup("User-Agent: Mozilla/5.0");
    size_t cap = strlen(token) + 18;
    headers[2] = (char*)malloc(cap);
    if (!headers[0] || !headers[1] || !headers[2]) {
        free(headers[0]);
        free(headers[1]);
        free(headers[2]);
        free(headers);
        return NULL;
    }
    snprintf(headers[2], cap, "X-Session-Token: %s", token);
    headers[3] = NULL;
    return headers;
}

static void shitty_email_free_headers(char **headers) {
    if (!headers) return;
    for (int i = 0; headers[i]; i++) free(headers[i]);
    free(headers);
}

static char *shitty_email_urlencode(const char *s) {
    if (!s) return tm_strdup("");
    size_t len = strlen(s);
    char *out = (char*)malloc(len * 3 + 1);
    if (!out) return NULL;
    char *p = out;
    static const char hex[] = "0123456789ABCDEF";
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
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

static cJSON *shitty_email_json(tm_http_method_t method, const char *url, const char *token) {
    const char **headers = shitty_email_headers;
    char **owned_headers = NULL;
    if (token && token[0]) {
        owned_headers = shitty_email_token_headers(token);
        if (!owned_headers) return NULL;
        headers = (const char**)owned_headers;
    }
    tm_http_response_t *resp = tm_http_request(method, url, headers, NULL, 15);
    shitty_email_free_headers(owned_headers);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }
    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    return root;
}

tm_email_info_t* tm_provider_shitty_email_generate(void) {
    cJSON *root = shitty_email_json(TM_HTTP_POST, SHITTY_EMAIL_BASE "/inbox", NULL);
    if (!root) return NULL;

    const char *email = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "email"), "");
    const char *token = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "token"), "");
    if (!email[0] || !strchr(email, '@') || !token[0]) {
        cJSON_Delete(root);
        return NULL;
    }

    tm_email_info_t *info = tm_email_info_new();
    if (!info) {
        cJSON_Delete(root);
        return NULL;
    }
    info->channel = CHANNEL_SHITTY_EMAIL;
    info->email = tm_strdup(email);
    info->token = tm_strdup(token);
    cJSON_Delete(root);
    return info;
}

static cJSON *shitty_email_fetch_message(const char *token, const char *message_id) {
    char *id_enc = shitty_email_urlencode(message_id);
    if (!id_enc) return NULL;
    size_t cap = strlen(SHITTY_EMAIL_BASE) + strlen(id_enc) + 16;
    char *url = (char*)malloc(cap);
    if (!url) {
        free(id_enc);
        return NULL;
    }
    snprintf(url, cap, "%s/email/%s", SHITTY_EMAIL_BASE, id_enc);
    free(id_enc);

    cJSON *root = shitty_email_json(TM_HTTP_GET, url, token);
    free(url);
    return root;
}

static cJSON *shitty_email_message_raw(const cJSON *msg, const char *recipient) {
    cJSON *raw = cJSON_CreateObject();
    if (!raw) return NULL;

    const cJSON *id = cJSON_GetObjectItemCaseSensitive((cJSON*)msg, "id");
    const cJSON *from = cJSON_GetObjectItemCaseSensitive((cJSON*)msg, "from");
    const cJSON *to = cJSON_GetObjectItemCaseSensitive((cJSON*)msg, "to");
    const cJSON *subject = cJSON_GetObjectItemCaseSensitive((cJSON*)msg, "subject");
    const cJSON *text = cJSON_GetObjectItemCaseSensitive((cJSON*)msg, "text");
    const cJSON *html = cJSON_GetObjectItemCaseSensitive((cJSON*)msg, "html");
    const cJSON *date = cJSON_GetObjectItemCaseSensitive((cJSON*)msg, "date");

    cJSON_AddStringToObject(raw, "id", TM_JSON_STR(id, ""));
    cJSON_AddStringToObject(raw, "from", TM_JSON_STR(from, ""));
    cJSON_AddStringToObject(raw, "to", TM_JSON_STR(to, recipient));
    cJSON_AddStringToObject(raw, "subject", TM_JSON_STR(subject, ""));
    cJSON_AddStringToObject(raw, "text", TM_JSON_STR(text, ""));
    cJSON_AddStringToObject(raw, "html", TM_JSON_STR(html, ""));
    cJSON_AddStringToObject(raw, "date", TM_JSON_STR(date, ""));
    return raw;
}

tm_email_t* tm_provider_shitty_email_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    if (!token || !token[0] || !email || !email[0]) return NULL;

    cJSON *root = shitty_email_json(TM_HTTP_GET, SHITTY_EMAIL_BASE "/inbox", token);
    if (!root) return NULL;
    cJSON *arr = cJSON_GetObjectItemCaseSensitive(root, "emails");
    if (!cJSON_IsArray(arr)) {
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
        *count = -1;
        cJSON_Delete(root);
        return NULL;
    }

    for (int i = 0; i < n; i++) {
        cJSON *item = cJSON_GetArrayItem(arr, i);
        const char *message_id = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(item, "id"), "");
        cJSON *detail = message_id[0] ? shitty_email_fetch_message(token, message_id) : NULL;
        cJSON *raw = shitty_email_message_raw(detail ? detail : item, email);
        emails[i] = tm_normalize_email(raw ? raw : (detail ? detail : item), email);
        if (raw) cJSON_Delete(raw);
        if (detail) cJSON_Delete(detail);
    }

    cJSON_Delete(root);
    return emails;
}
