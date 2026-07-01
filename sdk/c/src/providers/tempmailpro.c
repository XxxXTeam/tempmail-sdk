/**
 * TempMail Pro -- https://tempmailpro.us
 */

#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEMPMAILPRO_BASE "https://tempmailpro.us/api/v1"

static const char *tempmailpro_headers[] = {
    "Accept: application/json",
    "User-Agent: Mozilla/5.0",
    NULL
};

static const char *tempmailpro_post_headers[] = {
    "Accept: application/json",
    "Content-Type: application/json",
    "User-Agent: Mozilla/5.0",
    NULL
};

static char *tempmailpro_urlencode(const char *s) {
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

static cJSON *tempmailpro_json(tm_http_method_t method, const char *url, const char *body) {
    const char **headers = method == TM_HTTP_POST ? tempmailpro_post_headers : tempmailpro_headers;
    tm_http_response_t *resp = tm_http_request(method, url, headers, body, 15);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }
    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    return root;
}

tm_email_info_t *tm_provider_tempmailpro_generate(void) {
    cJSON *root = tempmailpro_json(TM_HTTP_POST, TEMPMAILPRO_BASE "/mailbox/create", "{}");
    if (!root) return NULL;
    cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
    const char *address = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(data, "address"), "");
    const char *token = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(data, "token"), "");
    if (!address[0] || !strchr(address, '@') || !token[0]) {
        cJSON_Delete(root);
        return NULL;
    }

    tm_email_info_t *info = tm_email_info_new();
    if (!info) {
        cJSON_Delete(root);
        return NULL;
    }
    info->channel = CHANNEL_TEMPMAILPRO;
    info->email = tm_strdup(address);
    info->token = tm_strdup(token);
    cJSON *expires_at = cJSON_GetObjectItemCaseSensitive(data, "expires_at");
    if (cJSON_IsNumber(expires_at)) info->expires_at = (long long)expires_at->valuedouble;
    cJSON *created_at = cJSON_GetObjectItemCaseSensitive(data, "created_at");
    if (cJSON_IsString(created_at) && created_at->valuestring) {
        info->created_at = tm_strdup(created_at->valuestring);
    } else if (cJSON_IsNumber(created_at)) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.0f", created_at->valuedouble);
        info->created_at = tm_strdup(buf);
    }
    cJSON_Delete(root);
    return info;
}

static cJSON *tempmailpro_fetch_detail(const char *token, const char *message_id) {
    char *token_enc = tempmailpro_urlencode(token);
    char *id_enc = tempmailpro_urlencode(message_id);
    if (!token_enc || !id_enc) {
        free(token_enc);
        free(id_enc);
        return NULL;
    }
    size_t cap = strlen(TEMPMAILPRO_BASE) + strlen(token_enc) + strlen(id_enc) + 32;
    char *url = (char *)malloc(cap);
    if (!url) {
        free(token_enc);
        free(id_enc);
        return NULL;
    }
    snprintf(url, cap, "%s/mailbox/%s/emails/%s", TEMPMAILPRO_BASE, token_enc, id_enc);
    free(token_enc);
    free(id_enc);

    cJSON *root = tempmailpro_json(TM_HTTP_GET, url, NULL);
    free(url);
    if (!root) return NULL;
    cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
    cJSON *dup = cJSON_IsObject(data) ? cJSON_Duplicate(data, 1) : NULL;
    cJSON_Delete(root);
    return dup;
}

static cJSON *tempmailpro_message_raw(cJSON *msg, const char *recipient) {
    cJSON *raw = cJSON_CreateObject();
    if (!raw) return NULL;
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(msg, "id");
    if (!id) id = cJSON_GetObjectItemCaseSensitive(msg, "message_id");
    const cJSON *from = cJSON_GetObjectItemCaseSensitive(msg, "from_address");
    if (!from) from = cJSON_GetObjectItemCaseSensitive(msg, "from_name");
    cJSON_AddStringToObject(raw, "id", TM_JSON_STR(id, ""));
    cJSON_AddStringToObject(raw, "from", TM_JSON_STR(from, ""));
    cJSON_AddStringToObject(raw, "to", recipient);
    cJSON_AddStringToObject(raw, "subject", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(msg, "subject"), ""));
    cJSON_AddStringToObject(raw, "text", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(msg, "body_text"), ""));
    cJSON_AddStringToObject(raw, "html", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(msg, "body_html"), ""));
    cJSON *received_at = cJSON_GetObjectItemCaseSensitive(msg, "received_at");
    if (cJSON_IsNumber(received_at)) cJSON_AddNumberToObject(raw, "date", received_at->valuedouble);
    else cJSON_AddStringToObject(raw, "date", TM_JSON_STR(received_at, ""));
    cJSON *attachments = cJSON_GetObjectItemCaseSensitive(msg, "attachments");
    if (attachments) cJSON_AddItemToObject(raw, "attachments", cJSON_Duplicate(attachments, 1));
    return raw;
}

tm_email_t *tm_provider_tempmailpro_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    if (!token || !token[0] || !email || !email[0]) return NULL;
    char *token_enc = tempmailpro_urlencode(token);
    if (!token_enc) return NULL;
    size_t cap = strlen(TEMPMAILPRO_BASE) + strlen(token_enc) + 24;
    char *url = (char *)malloc(cap);
    if (!url) {
        free(token_enc);
        return NULL;
    }
    snprintf(url, cap, "%s/mailbox/%s/emails", TEMPMAILPRO_BASE, token_enc);
    free(token_enc);

    cJSON *root = tempmailpro_json(TM_HTTP_GET, url, NULL);
    free(url);
    if (!root) return NULL;
    cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (!cJSON_IsArray(data)) {
        *count = 0;
        cJSON_Delete(root);
        return NULL;
    }
    int n = cJSON_GetArraySize(data);
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
        cJSON *item = cJSON_GetArrayItem(data, i);
        const char *message_id = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(item, "id"), "");
        cJSON *detail = message_id[0] ? tempmailpro_fetch_detail(token, message_id) : NULL;
        cJSON *raw = tempmailpro_message_raw(detail ? detail : item, email);
        emails[i] = tm_normalize_email(raw ? raw : (detail ? detail : item), email);
        if (raw) cJSON_Delete(raw);
        if (detail) cJSON_Delete(detail);
    }
    cJSON_Delete(root);
    return emails;
}
