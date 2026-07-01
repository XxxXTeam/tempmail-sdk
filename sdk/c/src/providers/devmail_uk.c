/**
 * DevMail UK -- https://devmail.uk
 */

#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEVMAIL_UK_BASE "https://www.devmail.uk/api"

static const char *devmail_uk_headers[] = {
    "Accept: application/json",
    NULL
};

static char *devmail_uk_urlencode(const char *s) {
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

static cJSON *devmail_uk_get_json(const char *url) {
    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, devmail_uk_headers, NULL, 15);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }
    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    return root;
}

static char *devmail_uk_mailbox(const char *email) {
    if (!email) return tm_strdup("");
    const char *at = strchr(email, '@');
    if (at && at > email) {
        size_t len = (size_t)(at - email);
        char *out = (char*)malloc(len + 1);
        if (!out) return NULL;
        memcpy(out, email, len);
        out[len] = '\0';
        return out;
    }
    return tm_strdup(email);
}

tm_email_info_t* tm_provider_devmail_uk_generate(void) {
    cJSON *root = devmail_uk_get_json(DEVMAIL_UK_BASE "/new");
    if (!root) return NULL;

    const char *email = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "email"), "");
    const char *mailbox = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "mailbox"), "");
    char *derived = NULL;
    if ((!email || !email[0]) && mailbox[0]) {
        size_t cap = strlen(mailbox) + sizeof("@devmail.uk");
        derived = (char*)malloc(cap);
        if (derived) snprintf(derived, cap, "%s@devmail.uk", mailbox);
        email = derived ? derived : "";
    }
    if (!email[0] || !strchr(email, '@')) {
        free(derived);
        cJSON_Delete(root);
        return NULL;
    }

    tm_email_info_t *info = tm_email_info_new();
    if (!info) {
        free(derived);
        cJSON_Delete(root);
        return NULL;
    }
    info->channel = CHANNEL_DEVMAIL_UK;
    info->email = tm_strdup(email);
    free(derived);
    cJSON_Delete(root);
    return info;
}

tm_email_t* tm_provider_devmail_uk_get_emails(const char *email, int *count) {
    *count = -1;
    if (!email || !email[0]) return NULL;

    char *mailbox = devmail_uk_mailbox(email);
    if (!mailbox || !mailbox[0]) {
        free(mailbox);
        return NULL;
    }
    char *enc = devmail_uk_urlencode(mailbox);
    free(mailbox);
    if (!enc) return NULL;

    size_t cap = strlen(DEVMAIL_UK_BASE) + strlen(enc) + 32;
    char *url = (char*)malloc(cap);
    if (!url) {
        free(enc);
        return NULL;
    }
    snprintf(url, cap, "%s/inbox/%s?detail=true", DEVMAIL_UK_BASE, enc);
    free(enc);

    cJSON *root = devmail_uk_get_json(url);
    free(url);
    if (!root) return NULL;

    cJSON *arr = cJSON_GetObjectItemCaseSensitive(root, "emails");
    if (!arr || !cJSON_IsArray(arr)) {
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

    int idx = 0;
    cJSON *item;
    cJSON_ArrayForEach(item, arr) {
        emails[idx++] = tm_normalize_email(item, email);
    }

    cJSON_Delete(root);
    return emails;
}
