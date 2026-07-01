/**
 * CleanTempMail -- https://cleantempmail.com
 */

#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CTP_BASE "https://cleantempmail.com/api"

static char *ctpm_api_key(void) {
    const char *v = getenv("CLEANTEMPMAIL_API_KEY");
    if (v && v[0]) return tm_strdup(v);
    return tm_strdup("ct-test");
}

static cJSON *ctpm_get_json(const char *url) {
    char *api_key = ctpm_api_key();
    if (!api_key || !api_key[0]) {
        free(api_key);
        return NULL;
    }
    char api_line[256];
    snprintf(api_line, sizeof(api_line), "X-API-Key: %s", api_key);
    const char *headers[] = {
        "Accept: application/json",
        "User-Agent: Mozilla/5.0",
        api_line,
        NULL
    };
    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, headers, NULL, 15);
    free(api_key);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }
    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    return root;
}

tm_email_info_t* tm_provider_cleantempmail_generate(void) {
    cJSON *root = ctpm_get_json(CTP_BASE "/generate-email");
    if (!root) return NULL;

    cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (!data || !cJSON_IsObject(data)) {
        cJSON_Delete(root);
        return NULL;
    }

    const char *email = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(data, "email"), "");
    const char *mailbox = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(data, "mailbox"), "");
    const char *picked = email[0] ? email : mailbox;
    if (!picked[0] || !strchr(picked, '@')) {
        cJSON_Delete(root);
        return NULL;
    }

    tm_email_info_t *info = tm_email_info_new();
    if (!info) {
        cJSON_Delete(root);
        return NULL;
    }
    info->channel = CHANNEL_CLEANTEMPMAIL;
    info->email = tm_strdup(picked);
    cJSON_Delete(root);
    return info;
}

static char *ctpm_urlencode(const char *s) {
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

tm_email_t* tm_provider_cleantempmail_get_emails(const char *email, int *count) {
    *count = -1;
    if (!email || !email[0]) return NULL;

    char *enc = ctpm_urlencode(email);
    if (!enc) return NULL;

    size_t cap = strlen(CTP_BASE) + strlen(enc) + 32;
    char *url = (char*)malloc(cap);
    if (!url) {
        free(enc);
        return NULL;
    }
    snprintf(url, cap, "%s/emails?email=%s", CTP_BASE, enc);
    free(enc);

    cJSON *root = ctpm_get_json(url);
    free(url);
    if (!root) return NULL;

    cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (!data || !cJSON_IsObject(data)) {
        cJSON_Delete(root);
        *count = 0;
        return NULL;
    }

    cJSON *arr = cJSON_GetObjectItemCaseSensitive(data, "emails");
    if (!arr || !cJSON_IsArray(arr)) {
        cJSON_Delete(root);
        *count = 0;
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
