/**
 * Inboxes -- https://inboxes.com
 */

#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define INBOXES_BASE "https://inboxes.com/api/v2"
#define INBOXES_DEFAULT_DOMAIN "blondmail.com"

static const char *inboxes_headers[] = {
    "Accept: application/json",
    "Origin: https://inboxes.com",
    "Referer: https://inboxes.com/",
    "User-Agent: Mozilla/5.0",
    NULL
};

static char *inboxes_urlencode(const char *s) {
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

static cJSON *inboxes_json(const char *url) {
    tm_http_response_t *resp = tm_http_request_ipv4(TM_HTTP_GET, url, inboxes_headers, NULL, 15);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }
    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    return root;
}

static void inboxes_random_local(char *buf, size_t cap) {
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

static char *inboxes_pick_domain(cJSON *domains, const char *preferred) {
    const char *want = preferred && preferred[0] == '@' ? preferred + 1 : preferred;
    if (want && want[0]) {
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, domains) {
            const char *qdn = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(item, "qdn"), "");
            if (strcmp(qdn, want) == 0) return tm_strdup(qdn);
        }
    }
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, domains) {
        const char *qdn = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(item, "qdn"), "");
        if (strcmp(qdn, INBOXES_DEFAULT_DOMAIN) == 0) return tm_strdup(qdn);
    }
    item = cJSON_GetArrayItem(domains, 0);
    return tm_strdup(TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(item, "qdn"), INBOXES_DEFAULT_DOMAIN));
}

tm_email_info_t* tm_provider_inboxes_generate(const char *domain) {
    cJSON *root = inboxes_json(INBOXES_BASE "/domain");
    if (!root) return NULL;
    cJSON *domains = cJSON_GetObjectItemCaseSensitive(root, "domains");
    if (!cJSON_IsArray(domains) || cJSON_GetArraySize(domains) == 0) {
        cJSON_Delete(root);
        return NULL;
    }
    char *picked = inboxes_pick_domain(domains, domain);
    cJSON_Delete(root);
    if (!picked || !picked[0]) {
        free(picked);
        return NULL;
    }

    srand((unsigned int)time(NULL));
    char local[32];
    inboxes_random_local(local, sizeof(local));
    size_t cap = strlen(local) + strlen(picked) + 2;
    char *email = (char*)malloc(cap);
    if (!email) {
        free(picked);
        return NULL;
    }
    snprintf(email, cap, "%s@%s", local, picked);
    free(picked);

    tm_email_info_t *info = tm_email_info_new();
    if (!info) {
        free(email);
        return NULL;
    }
    info->channel = CHANNEL_INBOXES;
    info->email = email;
    return info;
}

static cJSON *inboxes_fetch_detail(const char *uid) {
    char *id_enc = inboxes_urlencode(uid);
    if (!id_enc) return NULL;
    size_t cap = strlen(INBOXES_BASE) + strlen(id_enc) + 12;
    char *url = (char*)malloc(cap);
    if (!url) {
        free(id_enc);
        return NULL;
    }
    snprintf(url, cap, "%s/message/%s", INBOXES_BASE, id_enc);
    free(id_enc);
    cJSON *root = inboxes_json(url);
    free(url);
    return root;
}

static cJSON *inboxes_message_raw(const cJSON *msg, const char *recipient) {
    cJSON *raw = cJSON_CreateObject();
    if (!raw) return NULL;

    const char *from = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON*)msg, "sf"),
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON*)msg, "f"), ""));
    cJSON_AddStringToObject(raw, "id", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON*)msg, "uid"), ""));
    cJSON_AddStringToObject(raw, "from", from);
    cJSON_AddStringToObject(raw, "to", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON*)msg, "ib"), recipient));
    cJSON_AddStringToObject(raw, "subject", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON*)msg, "s"), ""));
    cJSON_AddStringToObject(raw, "text", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON*)msg, "text"), ""));
    cJSON_AddStringToObject(raw, "html", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON*)msg, "html"), ""));
    cJSON_AddStringToObject(raw, "preview_text", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON*)msg, "ph"), ""));
    cJSON_AddStringToObject(raw, "date", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON*)msg, "cr"), ""));
    const cJSON *read = cJSON_GetObjectItemCaseSensitive((cJSON*)msg, "ru");
    if (cJSON_IsBool(read)) cJSON_AddBoolToObject(raw, "isRead", cJSON_IsTrue(read));
    const cJSON *attachments = cJSON_GetObjectItemCaseSensitive((cJSON*)msg, "at");
    if (cJSON_IsArray(attachments)) {
        cJSON_AddItemToObject(raw, "attachments", cJSON_Duplicate((cJSON*)attachments, 1));
    } else {
        cJSON_AddItemToObject(raw, "attachments", cJSON_CreateArray());
    }
    return raw;
}

tm_email_t* tm_provider_inboxes_get_emails(const char *email, int *count) {
    *count = -1;
    if (!email || !email[0]) return NULL;

    char *email_enc = inboxes_urlencode(email);
    if (!email_enc) return NULL;
    size_t cap = strlen(INBOXES_BASE) + strlen(email_enc) + 10;
    char *url = (char*)malloc(cap);
    if (!url) {
        free(email_enc);
        return NULL;
    }
    snprintf(url, cap, "%s/inbox/%s", INBOXES_BASE, email_enc);
    free(email_enc);
    cJSON *root = inboxes_json(url);
    free(url);
    if (!root) return NULL;

    cJSON *messages = cJSON_GetObjectItemCaseSensitive(root, "msgs");
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
        cJSON *row = cJSON_GetArrayItem(messages, i);
        const char *uid = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(row, "uid"), "");
        cJSON *detail = uid[0] ? inboxes_fetch_detail(uid) : NULL;
        cJSON *raw = inboxes_message_raw(detail ? detail : row, email);
        emails[i] = tm_normalize_email(raw ? raw : (detail ? detail : row), email);
        if (raw) cJSON_Delete(raw);
        if (detail) cJSON_Delete(detail);
    }
    cJSON_Delete(root);
    return emails;
}
