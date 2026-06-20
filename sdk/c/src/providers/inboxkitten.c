/**
 * InboxKitten -- https://inboxkitten.com
 */

#include "tempmail_internal.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INBOXKITTEN_BASE "https://inboxkitten.com/api/v1/mail"
#define INBOXKITTEN_DOMAIN "inboxkitten.com"

static const char *inboxkitten_json_headers[] = {
    "Accept: application/json",
    NULL
};

static const char *inboxkitten_html_headers[] = {
    "Accept: text/html,*/*",
    NULL
};

static char *inboxkitten_urlencode(const char *s) {
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

static cJSON *inboxkitten_get_json(const char *url) {
    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, inboxkitten_json_headers, NULL, 15);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }
    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    return root;
}

static char *inboxkitten_get_html(const char *url) {
    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, inboxkitten_html_headers, NULL, 15);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }
    char *body = tm_strdup(resp->body);
    tm_http_response_free(resp);
    return body;
}

static void inboxkitten_random_local(char *buf, size_t cap) {
    static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    if (cap < 4) return;
    size_t len = cap - 1;
    if (len > 19) len = 19;
    buf[0] = 's';
    buf[1] = 'd';
    buf[2] = 'k';
    for (size_t i = 3; i < len; i++) {
        buf[i] = chars[rand() % (sizeof(chars) - 1)];
    }
    buf[len] = '\0';
}

static char *inboxkitten_local_part(const char *email) {
    if (!email) return tm_strdup("");
    const char *at = strchr(email, '@');
    size_t len = at ? (size_t)(at - email) : strlen(email);
    char *out = (char*)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, email, len);
    out[len] = '\0';
    return out;
}

static char *inboxkitten_html_to_text(const char *html) {
    if (!html) return tm_strdup("");
    size_t len = strlen(html);
    char *out = (char*)malloc(len + 1);
    if (!out) return NULL;
    int in_tag = 0;
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        char c = html[i];
        if (c == '<') {
            in_tag = 1;
            out[j++] = ' ';
            continue;
        }
        if (c == '>') {
            in_tag = 0;
            out[j++] = ' ';
            continue;
        }
        if (!in_tag) out[j++] = c;
    }
    out[j] = '\0';

    char *w = out;
    char *r = out;
    int last_space = 1;
    while (*r) {
        unsigned char c = (unsigned char)*r++;
        if (isspace(c)) {
            if (!last_space) *w++ = ' ';
            last_space = 1;
        } else {
            *w++ = (char)c;
            last_space = 0;
        }
    }
    if (w > out && w[-1] == ' ') w--;
    *w = '\0';
    return out;
}

tm_email_info_t* tm_provider_inboxkitten_generate(void) {
    char local[32];
    inboxkitten_random_local(local, sizeof(local));
    size_t cap = strlen(local) + strlen(INBOXKITTEN_DOMAIN) + 2;
    char *email = (char*)malloc(cap);
    if (!email) return NULL;
    snprintf(email, cap, "%s@%s", local, INBOXKITTEN_DOMAIN);

    tm_email_info_t *info = tm_email_info_new();
    if (!info) {
        free(email);
        return NULL;
    }
    info->channel = CHANNEL_INBOXKITTEN;
    info->email = email;
    return info;
}

static char *inboxkitten_detail_url(const char *path, const char *region, const char *key) {
    char *region_enc = inboxkitten_urlencode(region);
    char *key_enc = inboxkitten_urlencode(key);
    if (!region_enc || !key_enc) {
        free(region_enc);
        free(key_enc);
        return NULL;
    }
    size_t cap = strlen(INBOXKITTEN_BASE) + strlen(path) + strlen(region_enc) + strlen(key_enc) + 32;
    char *url = (char*)malloc(cap);
    if (!url) {
        free(region_enc);
        free(key_enc);
        return NULL;
    }
    snprintf(url, cap, "%s/%s?region=%s&key=%s", INBOXKITTEN_BASE, path, region_enc, key_enc);
    free(region_enc);
    free(key_enc);
    return url;
}

static cJSON *inboxkitten_message_raw(const cJSON *item, const char *recipient) {
    cJSON *raw = cJSON_CreateObject();
    if (!raw) return NULL;

    const cJSON *storage = cJSON_GetObjectItemCaseSensitive((cJSON*)item, "storage");
    const char *region = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON*)storage, "region"), "");
    const char *key = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON*)storage, "key"), "");
    const cJSON *message = cJSON_GetObjectItemCaseSensitive((cJSON*)item, "message");
    const cJSON *headers = cJSON_GetObjectItemCaseSensitive((cJSON*)message, "headers");
    const cJSON *envelope = cJSON_GetObjectItemCaseSensitive((cJSON*)item, "envelope");

    const char *from = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON*)headers, "from"), "");
    if (!from[0]) from = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON*)envelope, "sender"), "");
    const char *to = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON*)item, "recipient"), "");
    if (!to[0]) to = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON*)envelope, "targets"), recipient);
    const char *subject = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON*)headers, "subject"), "");

    cJSON_AddStringToObject(raw, "id", key);
    cJSON_AddStringToObject(raw, "messageId", key);
    cJSON_AddStringToObject(raw, "from", from);
    cJSON_AddStringToObject(raw, "to", to);
    cJSON_AddStringToObject(raw, "subject", subject);
    const cJSON *timestamp = cJSON_GetObjectItemCaseSensitive((cJSON*)item, "timestamp");
    if (timestamp) cJSON_AddItemToObject(raw, "timestamp", cJSON_Duplicate((cJSON*)timestamp, 1));

    if (region[0] && key[0]) {
        char *meta_url = inboxkitten_detail_url("getKey", region, key);
        char *html_url = inboxkitten_detail_url("getHtml", region, key);
        cJSON *meta = meta_url ? inboxkitten_get_json(meta_url) : NULL;
        char *html = html_url ? inboxkitten_get_html(html_url) : NULL;
        free(meta_url);
        free(html_url);

        if (meta) {
            const char *name = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(meta, "name"), "");
            const char *recipients = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(meta, "recipients"), "");
            const char *meta_subject = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(meta, "subject"), "");
            if (name[0]) cJSON_ReplaceItemInObjectCaseSensitive(raw, "from", cJSON_CreateString(name));
            if (recipients[0]) cJSON_ReplaceItemInObjectCaseSensitive(raw, "to", cJSON_CreateString(recipients));
            if (meta_subject[0]) cJSON_ReplaceItemInObjectCaseSensitive(raw, "subject", cJSON_CreateString(meta_subject));
            cJSON_Delete(meta);
        }
        if (html) {
            char *text = inboxkitten_html_to_text(html);
            cJSON_AddStringToObject(raw, "html", html);
            cJSON_AddStringToObject(raw, "text", text ? text : "");
            free(text);
            free(html);
        }
    }
    return raw;
}

tm_email_t* tm_provider_inboxkitten_get_emails(const char *email, int *count) {
    *count = -1;
    char *local = inboxkitten_local_part(email);
    if (!local || !local[0]) {
        free(local);
        return NULL;
    }
    char *enc = inboxkitten_urlencode(local);
    free(local);
    if (!enc) return NULL;
    size_t cap = strlen(INBOXKITTEN_BASE) + strlen(enc) + 32;
    char *url = (char*)malloc(cap);
    if (!url) {
        free(enc);
        return NULL;
    }
    snprintf(url, cap, "%s/list?recipient=%s", INBOXKITTEN_BASE, enc);
    free(enc);

    cJSON *arr = inboxkitten_get_json(url);
    free(url);
    if (!arr || !cJSON_IsArray(arr)) {
        cJSON_Delete(arr);
        return NULL;
    }

    int n = cJSON_GetArraySize(arr);
    *count = n;
    if (n == 0) {
        cJSON_Delete(arr);
        return NULL;
    }

    tm_email_t *emails = tm_emails_new(n);
    if (!emails) {
        *count = -1;
        cJSON_Delete(arr);
        return NULL;
    }

    for (int i = 0; i < n; i++) {
        cJSON *item = cJSON_GetArrayItem(arr, i);
        cJSON *raw = inboxkitten_message_raw(item, email);
        emails[i] = tm_normalize_email(raw ? raw : item, email);
        if (raw) cJSON_Delete(raw);
    }

    cJSON_Delete(arr);
    return emails;
}
