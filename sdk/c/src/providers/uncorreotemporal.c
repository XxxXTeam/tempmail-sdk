/**
 * UnCorreoTemporal -- https://uncorreotemporal.com
 */

#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UCT_BASE "https://uncorreotemporal.com/api/v1"

static const char *uct_headers_base[] = {
    "Accept: application/json",
    "Origin: https://uncorreotemporal.com",
    "Referer: https://uncorreotemporal.com/",
    "User-Agent: Mozilla/5.0",
    NULL
};

static char *uct_urlencode(const char *s) {
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

static cJSON *uct_json(tm_http_method_t method, const char *url, const char *token) {
    const char *headers[8];
    int n = 0;
    for (int i = 0; uct_headers_base[i] != NULL; i++) headers[n++] = uct_headers_base[i];
    if (method == TM_HTTP_POST) headers[n++] = "Content-Type: application/json";

    char *token_header = NULL;
    if (token && token[0]) {
        size_t cap = strlen(token) + 18;
        token_header = (char*)malloc(cap);
        if (!token_header) return NULL;
        snprintf(token_header, cap, "X-Session-Token: %s", token);
        headers[n++] = token_header;
    }
    headers[n] = NULL;

    tm_http_response_t *resp = tm_http_request_ipv4(method, url, headers, NULL, 15);
    free(token_header);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }
    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    return root;
}

tm_email_info_t* tm_provider_uncorreotemporal_generate(void) {
    cJSON *root = uct_json(TM_HTTP_POST, UCT_BASE "/mailboxes", NULL);
    if (!root) return NULL;

    const char *email = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "address"), "");
    const char *token = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "session_token"), "");
    if (!email[0] || !strchr(email, '@') || !token[0]) {
        cJSON_Delete(root);
        return NULL;
    }

    tm_email_info_t *info = tm_email_info_new();
    if (!info) {
        cJSON_Delete(root);
        return NULL;
    }
    info->channel = CHANNEL_UNCORREOTEMPORAL;
    info->email = tm_strdup(email);
    info->token = tm_strdup(token);
    cJSON_Delete(root);
    return info;
}

static cJSON *uct_fetch_detail(const char *email, const char *token, const char *message_id) {
    char *email_enc = uct_urlencode(email);
    char *id_enc = uct_urlencode(message_id);
    if (!email_enc || !id_enc) {
        free(email_enc);
        free(id_enc);
        return NULL;
    }
    size_t cap = strlen(UCT_BASE) + strlen(email_enc) + strlen(id_enc) + 24;
    char *url = (char*)malloc(cap);
    if (!url) {
        free(email_enc);
        free(id_enc);
        return NULL;
    }
    snprintf(url, cap, "%s/mailboxes/%s/messages/%s", UCT_BASE, email_enc, id_enc);
    free(email_enc);
    free(id_enc);
    cJSON *root = uct_json(TM_HTTP_GET, url, token);
    free(url);
    return root;
}

static cJSON *uct_message_raw(const cJSON *msg, const char *recipient) {
    cJSON *raw = cJSON_CreateObject();
    if (!raw) return NULL;

    cJSON_AddStringToObject(raw, "id", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON*)msg, "id"), ""));
    cJSON_AddStringToObject(raw, "from", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON*)msg, "from_address"), ""));
    cJSON_AddStringToObject(raw, "to", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON*)msg, "to_address"), recipient));
    cJSON_AddStringToObject(raw, "subject", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON*)msg, "subject"), ""));
    cJSON_AddStringToObject(raw, "text", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON*)msg, "body_text"), ""));
    cJSON_AddStringToObject(raw, "html", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON*)msg, "body_html"), ""));
    cJSON_AddStringToObject(raw, "date", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON*)msg, "received_at"), ""));
    const cJSON *read = cJSON_GetObjectItemCaseSensitive((cJSON*)msg, "is_read");
    if (cJSON_IsBool(read)) cJSON_AddBoolToObject(raw, "isRead", cJSON_IsTrue(read));
    const cJSON *attachments = cJSON_GetObjectItemCaseSensitive((cJSON*)msg, "attachments");
    if (cJSON_IsArray(attachments)) {
        cJSON_AddItemToObject(raw, "attachments", cJSON_Duplicate((cJSON*)attachments, 1));
    } else {
        cJSON_AddItemToObject(raw, "attachments", cJSON_CreateArray());
    }
    return raw;
}

tm_email_t* tm_provider_uncorreotemporal_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    if (!token || !token[0] || !email || !email[0]) return NULL;

    char *email_enc = uct_urlencode(email);
    if (!email_enc) return NULL;
    size_t cap = strlen(UCT_BASE) + strlen(email_enc) + 40;
    char *url = (char*)malloc(cap);
    if (!url) {
        free(email_enc);
        return NULL;
    }
    snprintf(url, cap, "%s/mailboxes/%s/messages?limit=50", UCT_BASE, email_enc);
    free(email_enc);
    cJSON *root = uct_json(TM_HTTP_GET, url, token);
    free(url);
    if (!root) return NULL;

    if (!cJSON_IsArray(root)) {
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
        cJSON *row = cJSON_GetArrayItem(root, i);
        const char *id = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(row, "id"), "");
        cJSON *detail = id[0] ? uct_fetch_detail(email, token, id) : NULL;
        cJSON *raw = uct_message_raw(detail ? detail : row, email);
        emails[i] = tm_normalize_email(raw ? raw : (detail ? detail : row), email);
        if (raw) cJSON_Delete(raw);
        if (detail) cJSON_Delete(detail);
    }
    cJSON_Delete(root);
    return emails;
}
