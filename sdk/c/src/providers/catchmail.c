/**
 * Catchmail -- https://catchmail.io
 */

#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CATCHMAIL_BASE "https://api.catchmail.io/api/v1"
#define CATCHMAIL_DEFAULT_DOMAIN "catchmail.io"

static const char *catchmail_domains[] = {
    "catchmail.io",
    "mailistry.com",
    "zeppost.com",
};

static const char *catchmail_headers[] = {
    "Accept: application/json",
    "Referer: https://catchmail.io/",
    "Origin: https://catchmail.io",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    NULL
};

static void catchmail_random_local(char *buf, size_t cap) {
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

static const char *catchmail_pick_domain(const char *preferred) {
    if (preferred && preferred[0]) {
        const char *p = preferred[0] == '@' ? preferred + 1 : preferred;
        size_t n = sizeof(catchmail_domains) / sizeof(catchmail_domains[0]);
        for (size_t i = 0; i < n; i++) {
            if (strcmp(p, catchmail_domains[i]) == 0) {
                return catchmail_domains[i];
            }
        }
    }
    return CATCHMAIL_DEFAULT_DOMAIN;
}

static char *catchmail_urlencode(const char *s) {
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

static char *catchmail_clean_address_json(const cJSON *item) {
    if (cJSON_IsArray(item) && cJSON_GetArraySize((cJSON*)item) > 0) {
        return catchmail_clean_address_json(cJSON_GetArrayItem((cJSON*)item, 0));
    }
    const char *raw = cJSON_GetStringValue(item);
    if (!raw) return tm_strdup("");
    const char *lt = strchr(raw, '<');
    const char *gt = lt ? strchr(lt + 1, '>') : NULL;
    if (lt && gt && gt > lt + 1) {
        size_t len = (size_t)(gt - lt - 1);
        char *out = (char*)malloc(len + 1);
        if (!out) return tm_strdup("");
        memcpy(out, lt + 1, len);
        out[len] = '\0';
        return out;
    }
    return tm_strdup(raw);
}

tm_email_info_t* tm_provider_catchmail_generate(const char *domain) {
    srand((unsigned)time(NULL));
    char local[32];
    catchmail_random_local(local, sizeof(local));

    char email[160];
    snprintf(email, sizeof(email), "%s@%s", local, catchmail_pick_domain(domain));
    char *enc = catchmail_urlencode(email);
    if (!enc) return NULL;

    char url[512];
    snprintf(url, sizeof(url), "%s/mailbox?address=%s", CATCHMAIL_BASE, enc);
    free(enc);

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, catchmail_headers, NULL, 15);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }
    tm_http_response_free(resp);

    tm_email_info_t *info = tm_email_info_new();
    if (!info) return NULL;
    info->channel = CHANNEL_CATCHMAIL;
    info->email = tm_strdup(email);
    return info;
}

static cJSON *catchmail_fetch_message(const char *message_id, const char *email) {
    char *id_enc = catchmail_urlencode(message_id);
    char *mail_enc = catchmail_urlencode(email);
    if (!id_enc || !mail_enc) {
        free(id_enc);
        free(mail_enc);
        return NULL;
    }
    char url[768];
    snprintf(url, sizeof(url), "%s/message/%s?mailbox=%s", CATCHMAIL_BASE, id_enc, mail_enc);
    free(id_enc);
    free(mail_enc);

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, catchmail_headers, NULL, 15);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }
    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    return root;
}

static cJSON *catchmail_flatten_detail(const cJSON *detail, const char *recipient) {
    cJSON *flat = cJSON_CreateObject();
    if (!flat) return NULL;

    cJSON *id = cJSON_GetObjectItemCaseSensitive((cJSON*)detail, "id");
    cJSON *from = cJSON_GetObjectItemCaseSensitive((cJSON*)detail, "from");
    cJSON *to = cJSON_GetObjectItemCaseSensitive((cJSON*)detail, "to");
    cJSON *subject = cJSON_GetObjectItemCaseSensitive((cJSON*)detail, "subject");
    cJSON *date = cJSON_GetObjectItemCaseSensitive((cJSON*)detail, "date");
    cJSON *body = cJSON_GetObjectItemCaseSensitive((cJSON*)detail, "body");
    cJSON *text = body ? cJSON_GetObjectItemCaseSensitive(body, "text") : NULL;
    cJSON *html = body ? cJSON_GetObjectItemCaseSensitive(body, "html") : NULL;
    cJSON *attachments = cJSON_GetObjectItemCaseSensitive((cJSON*)detail, "attachments");

    char *from_clean = catchmail_clean_address_json(from);
    char *to_clean = catchmail_clean_address_json(to);

    cJSON_AddStringToObject(flat, "id", TM_JSON_STR(id, ""));
    cJSON_AddStringToObject(flat, "from", from_clean ? from_clean : "");
    cJSON_AddStringToObject(flat, "to", (to_clean && to_clean[0]) ? to_clean : recipient);
    cJSON_AddStringToObject(flat, "subject", TM_JSON_STR(subject, ""));
    cJSON_AddStringToObject(flat, "text", TM_JSON_STR(text, ""));
    cJSON_AddStringToObject(flat, "html", TM_JSON_STR(html, ""));
    cJSON_AddStringToObject(flat, "date", TM_JSON_STR(date, ""));
    if (attachments) {
        cJSON_AddItemToObject(flat, "attachments", cJSON_Duplicate(attachments, 1));
    }

    free(from_clean);
    free(to_clean);
    return flat;
}

tm_email_t* tm_provider_catchmail_get_emails(const char *email, int *count) {
    *count = -1;
    if (!email || !email[0]) return NULL;

    char *enc = catchmail_urlencode(email);
    if (!enc) return NULL;
    char url[512];
    snprintf(url, sizeof(url), "%s/mailbox?address=%s", CATCHMAIL_BASE, enc);
    free(enc);

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, catchmail_headers, NULL, 15);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }

    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!root) return NULL;

    cJSON *arr = cJSON_GetObjectItemCaseSensitive(root, "messages");
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

    for (int i = 0; i < n; i++) {
        cJSON *item = cJSON_GetArrayItem(arr, i);
        const char *id = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(item, "id"), "");
        cJSON *detail = id[0] ? catchmail_fetch_message(id, email) : NULL;
        if (detail) {
            cJSON *flat = catchmail_flatten_detail(detail, email);
            emails[i] = tm_normalize_email(flat ? flat : detail, email);
            if (flat) cJSON_Delete(flat);
            cJSON_Delete(detail);
            continue;
        }

        cJSON *flat = cJSON_CreateObject();
        char *from_clean = catchmail_clean_address_json(cJSON_GetObjectItemCaseSensitive(item, "from"));
        cJSON_AddStringToObject(flat, "id", id);
        cJSON_AddStringToObject(flat, "from", from_clean ? from_clean : "");
        cJSON_AddStringToObject(flat, "to", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(item, "mailbox"), email));
        cJSON_AddStringToObject(flat, "subject", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(item, "subject"), ""));
        cJSON_AddStringToObject(flat, "date", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(item, "date"), ""));
        emails[i] = tm_normalize_email(flat, email);
        free(from_clean);
        cJSON_Delete(flat);
    }

    cJSON_Delete(root);
    return emails;
}
