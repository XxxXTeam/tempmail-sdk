/**
 * TempFastMail -- https://tempfastmail.com
 */

#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TFM_BASE "https://tempfastmail.com"

static const char *tfm_headers[] = {
    "Accept: application/json",
    "User-Agent: Mozilla/5.0",
    NULL
};

static char *tfm_urlencode(const char *s) {
    if (!s) return tm_strdup("");
    size_t len = strlen(s);
    char *out = (char *)malloc(len * 3 + 1);
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

static cJSON *tfm_json(tm_http_method_t method, const char *url) {
    tm_http_response_t *resp = tm_http_request(method, url, tfm_headers, NULL, 15);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }
    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    return root;
}

static cJSON *tfm_message_raw(const cJSON *msg, const char *recipient) {
    cJSON *raw = cJSON_CreateObject();
    if (!raw) return NULL;
    cJSON_AddStringToObject(raw, "id", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "uuid"), ""));
    cJSON_AddStringToObject(raw, "from", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "from"), ""));
    const char *to = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "real_to"), "");
    cJSON_AddStringToObject(raw, "to", to[0] ? to : (recipient ? recipient : ""));
    cJSON_AddStringToObject(raw, "subject", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "subject"), ""));
    cJSON_AddStringToObject(raw, "html", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "html"), ""));
    cJSON_AddStringToObject(raw, "date", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "received_at"), ""));
    const cJSON *attachments = cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "attachments");
    if (attachments) cJSON_AddItemToObject(raw, "attachments", cJSON_Duplicate((cJSON *)attachments, 1));
    return raw;
}

tm_email_info_t* tm_provider_tempfastmail_generate(void) {
    cJSON *root = tfm_json(TM_HTTP_POST, TFM_BASE "/api/email-box");
    if (!root) return NULL;
    const char *email = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "email"), "");
    const char *uuid = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "uuid"), "");
    if (!email[0] || !strchr(email, '@') || !uuid[0]) {
        cJSON_Delete(root);
        return NULL;
    }
    tm_email_info_t *info = tm_email_info_new();
    if (!info) {
        cJSON_Delete(root);
        return NULL;
    }
    info->channel = CHANNEL_TEMPFASTMAIL;
    info->email = tm_strdup(email);
    info->token = tm_strdup(uuid);
    cJSON_Delete(root);
    return info;
}

static cJSON *tfm_fetch_detail(const char *box_uuid, const char *message_uuid) {
    char *box_enc = tfm_urlencode(box_uuid);
    char *msg_enc = tfm_urlencode(message_uuid);
    if (!box_enc || !msg_enc) {
        free(box_enc);
        free(msg_enc);
        return NULL;
    }
    size_t cap = strlen(TFM_BASE) + strlen(box_enc) + strlen(msg_enc) + 28;
    char *url = (char *)malloc(cap);
    if (!url) {
        free(box_enc);
        free(msg_enc);
        return NULL;
    }
    snprintf(url, cap, "%s/api/email-box/%s/email/%s", TFM_BASE, box_enc, msg_enc);
    free(box_enc);
    free(msg_enc);
    cJSON *root = tfm_json(TM_HTTP_GET, url);
    free(url);
    return root;
}

tm_email_t* tm_provider_tempfastmail_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    if (!token || !token[0] || !email || !email[0]) return NULL;

    char *box_enc = tfm_urlencode(token);
    if (!box_enc) return NULL;
    size_t cap = strlen(TFM_BASE) + strlen(box_enc) + 24;
    char *url = (char *)malloc(cap);
    if (!url) {
        free(box_enc);
        return NULL;
    }
    snprintf(url, cap, "%s/api/email-box/%s/emails", TFM_BASE, box_enc);
    free(box_enc);
    cJSON *rows = tfm_json(TM_HTTP_GET, url);
    free(url);
    if (!rows) return NULL;
    if (!cJSON_IsArray(rows)) {
        *count = 0;
        cJSON_Delete(rows);
        return NULL;
    }

    int n = cJSON_GetArraySize(rows);
    *count = n;
    if (n == 0) {
        cJSON_Delete(rows);
        return NULL;
    }
    tm_email_t *emails = tm_emails_new(n);
    if (!emails) {
        *count = -1;
        cJSON_Delete(rows);
        return NULL;
    }
    for (int i = 0; i < n; i++) {
        cJSON *item = cJSON_GetArrayItem(rows, i);
        const char *message_uuid = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(item, "uuid"), "");
        cJSON *detail = message_uuid[0] ? tfm_fetch_detail(token, message_uuid) : NULL;
        cJSON *raw = tfm_message_raw(detail ? detail : item, email);
        emails[i] = tm_normalize_email(raw ? raw : (detail ? detail : item), email);
        if (raw) cJSON_Delete(raw);
        if (detail) cJSON_Delete(detail);
    }
    cJSON_Delete(rows);
    return emails;
}
