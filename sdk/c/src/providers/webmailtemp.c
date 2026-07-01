/**
 * WebMailTemp -- https://webmailtemp.com
 */

#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define WMT_BASE "https://webmailtemp.com"

static const char *wmt_create_headers[] = {
    "Accept: application/json",
    "User-Agent: Mozilla/5.0",
    NULL
};

static char *wmt_urlencode(const char *s) {
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

static char *wmt_cookie_header_line(const char *cookie) {
    size_t need = strlen(cookie ? cookie : "") + 9;
    char *line = (char *)malloc(need);
    if (!line) return NULL;
    snprintf(line, need, "Cookie: %s", cookie ? cookie : "");
    return line;
}

static char *wmt_encode_token(const char *username, const char *cookie) {
    cJSON *root = cJSON_CreateObject();
    if (!root) return NULL;
    cJSON_AddStringToObject(root, "u", username ? username : "");
    cJSON_AddStringToObject(root, "c", cookie ? cookie : "");
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return out;
}

static int wmt_decode_token(const char *token, char **username, char **cookie) {
    *username = NULL;
    *cookie = NULL;
    cJSON *root = cJSON_Parse(token ? token : "");
    if (!root) return 0;
    const char *u = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "u"), "");
    const char *c = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "c"), "");
    if (!u[0] || !c[0]) {
        cJSON_Delete(root);
        return 0;
    }
    *username = tm_strdup(u);
    *cookie = tm_strdup(c);
    cJSON_Delete(root);
    return *username && *cookie;
}

static cJSON *wmt_message_raw(const cJSON *msg, const char *recipient) {
    cJSON *raw = cJSON_CreateObject();
    if (!raw) return NULL;
    cJSON_AddStringToObject(raw, "id", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "id"), ""));
    cJSON_AddStringToObject(raw, "from", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "from"), ""));
    cJSON_AddStringToObject(raw, "to", recipient ? recipient : "");
    cJSON_AddStringToObject(raw, "subject", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "subject"), ""));
    cJSON_AddStringToObject(raw, "text", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "body"), ""));
    cJSON_AddStringToObject(raw, "html", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "html"), ""));
    cJSON *received = cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "received_at");
    if (!received) received = cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "timestamp");
    cJSON_AddStringToObject(raw, "date", TM_JSON_STR(received, ""));
    const cJSON *attachments = cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "attachments");
    if (attachments) cJSON_AddItemToObject(raw, "attachments", cJSON_Duplicate((cJSON *)attachments, 1));
    return raw;
}

tm_email_info_t* tm_provider_webmailtemp_generate(void) {
    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, WMT_BASE "/api/create", wmt_create_headers, NULL, 15);
    if (!resp || resp->status < 200 || resp->status >= 300 || !resp->cookies || !resp->cookies[0]) {
        tm_http_response_free(resp);
        return NULL;
    }
    char *cookie = tm_strdup(resp->cookies);
    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!root || !cookie || !cookie[0]) {
        cJSON_Delete(root);
        free(cookie);
        return NULL;
    }

    const cJSON *success = cJSON_GetObjectItemCaseSensitive(root, "success");
    const char *email = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "email"), "");
    const char *username = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "username"), "");
    if (!cJSON_IsTrue(success) || !email[0] || !strchr(email, '@') || !username[0]) {
        cJSON_Delete(root);
        free(cookie);
        return NULL;
    }

    char *token = wmt_encode_token(username, cookie);
    tm_email_info_t *info = tm_email_info_new();
    if (!info || !token) {
        if (info) tm_free_email_info(info);
        free(token);
        cJSON_Delete(root);
        free(cookie);
        return NULL;
    }
    info->channel = CHANNEL_WEBMAILTEMP;
    info->email = tm_strdup(email);
    info->token = token;
    const cJSON *ttl = cJSON_GetObjectItemCaseSensitive(root, "ttl");
    if (cJSON_IsNumber(ttl) && ttl->valuedouble > 0) {
        info->expires_at = ((long long)time(NULL) + (long long)ttl->valuedouble) * 1000LL;
    }
    cJSON_Delete(root);
    free(cookie);
    return info;
}

tm_email_t* tm_provider_webmailtemp_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    if (!token || !email || !email[0]) return NULL;

    char *username = NULL;
    char *cookie = NULL;
    if (!wmt_decode_token(token, &username, &cookie)) {
        free(username);
        free(cookie);
        return NULL;
    }
    char *user_enc = wmt_urlencode(username);
    char *cookie_line = wmt_cookie_header_line(cookie);
    free(username);
    free(cookie);
    if (!user_enc || !cookie_line) {
        free(user_enc);
        free(cookie_line);
        return NULL;
    }

    size_t cap = strlen(WMT_BASE) + strlen(user_enc) + 14;
    char *url = (char *)malloc(cap);
    if (!url) {
        free(user_enc);
        free(cookie_line);
        return NULL;
    }
    snprintf(url, cap, "%s/api/check/%s", WMT_BASE, user_enc);
    free(user_enc);

    const char *headers[] = {
        "Accept: application/json",
        "User-Agent: Mozilla/5.0",
        cookie_line,
        NULL
    };
    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, headers, NULL, 15);
    free(url);
    free(cookie_line);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }
    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!root) return NULL;

    cJSON *messages = cJSON_GetObjectItemCaseSensitive(root, "emails");
    if (!cJSON_IsArray(messages)) {
        *count = 0;
        cJSON_Delete(root);
        return NULL;
    }
    int n = cJSON_GetArraySize(messages);
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
        cJSON *item = cJSON_GetArrayItem(messages, i);
        cJSON *raw = wmt_message_raw(item, email);
        emails[i] = tm_normalize_email(raw ? raw : item, email);
        if (raw) cJSON_Delete(raw);
    }
    cJSON_Delete(root);
    return emails;
}
