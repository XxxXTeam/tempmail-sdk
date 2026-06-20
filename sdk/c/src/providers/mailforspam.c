/**
 * MailForSpam -- https://mailforspam.com
 */

#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAILFORSPAM_BASE "https://api.mailforspam.com/api"
#define MAILFORSPAM_DEFAULT_DOMAIN "mailforspam.com"

static const char *mailforspam_domains[] = {
    "mailforspam.com",
    "tempmail.io",
    "disposable.email",
};

static const char *mailforspam_headers[] = {
    "Accept: application/json",
    "Referer: https://mailforspam.com/",
    "Origin: https://mailforspam.com",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    NULL
};

static void mailforspam_random_local(char *buf, size_t cap) {
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

static const char *mailforspam_pick_domain(const char *preferred) {
    if (preferred && preferred[0]) {
        const char *p = preferred[0] == '@' ? preferred + 1 : preferred;
        size_t n = sizeof(mailforspam_domains) / sizeof(mailforspam_domains[0]);
        for (size_t i = 0; i < n; i++) {
            if (strcmp(p, mailforspam_domains[i]) == 0) {
                return mailforspam_domains[i];
            }
        }
    }
    return MAILFORSPAM_DEFAULT_DOMAIN;
}

static char *mailforspam_urlencode(const char *s) {
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

static char *mailforspam_emails_url(const char *email, int page_size) {
    char *enc = mailforspam_urlencode(email);
    if (!enc) return NULL;
    size_t cap = strlen(MAILFORSPAM_BASE) + strlen(enc) + 128;
    char *url = (char*)malloc(cap);
    if (!url) {
        free(enc);
        return NULL;
    }
    snprintf(url, cap, "%s/mailboxes/%s/emails?page=1&page_size=%d&sort_by=date&sort_order=desc",
             MAILFORSPAM_BASE, enc, page_size);
    free(enc);
    return url;
}

static cJSON *mailforspam_get_json(const char *url) {
    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, mailforspam_headers, NULL, 15);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }
    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    return root;
}

tm_email_info_t* tm_provider_mailforspam_generate(const char *domain) {
    srand((unsigned)time(NULL));
    char local[32];
    mailforspam_random_local(local, sizeof(local));

    char email[160];
    snprintf(email, sizeof(email), "%s@%s", local, mailforspam_pick_domain(domain));

    char *url = mailforspam_emails_url(email, 1);
    if (!url) return NULL;
    cJSON *root = mailforspam_get_json(url);
    free(url);
    if (!root) return NULL;
    cJSON_Delete(root);

    tm_email_info_t *info = tm_email_info_new();
    if (!info) return NULL;
    info->channel = CHANNEL_MAILFORSPAM;
    info->email = tm_strdup(email);
    return info;
}

static cJSON *mailforspam_fetch_message(const char *message_id, const char *email) {
    char *mail_enc = mailforspam_urlencode(email);
    char *id_enc = mailforspam_urlencode(message_id);
    if (!mail_enc || !id_enc) {
        free(mail_enc);
        free(id_enc);
        return NULL;
    }
    size_t cap = strlen(MAILFORSPAM_BASE) + strlen(mail_enc) + strlen(id_enc) + 64;
    char *url = (char*)malloc(cap);
    if (!url) {
        free(mail_enc);
        free(id_enc);
        return NULL;
    }
    snprintf(url, cap, "%s/mailboxes/%s/emails/%s", MAILFORSPAM_BASE, mail_enc, id_enc);
    free(mail_enc);
    free(id_enc);

    cJSON *root = mailforspam_get_json(url);
    free(url);
    return root;
}

static cJSON *mailforspam_flatten(const cJSON *raw, const char *recipient) {
    cJSON *flat = cJSON_CreateObject();
    if (!flat) return NULL;

    cJSON *id = cJSON_GetObjectItemCaseSensitive((cJSON*)raw, "id");
    cJSON *sender = cJSON_GetObjectItemCaseSensitive((cJSON*)raw, "sender");
    cJSON *receiver = cJSON_GetObjectItemCaseSensitive((cJSON*)raw, "receiver");
    cJSON *subject = cJSON_GetObjectItemCaseSensitive((cJSON*)raw, "subject");
    cJSON *body_text = cJSON_GetObjectItemCaseSensitive((cJSON*)raw, "body_text");
    cJSON *body_html = cJSON_GetObjectItemCaseSensitive((cJSON*)raw, "body_html");
    cJSON *date = cJSON_GetObjectItemCaseSensitive((cJSON*)raw, "date");
    cJSON *read_at = cJSON_GetObjectItemCaseSensitive((cJSON*)raw, "readAt");
    cJSON *attachments = cJSON_GetObjectItemCaseSensitive((cJSON*)raw, "attachments");

    cJSON_AddStringToObject(flat, "id", TM_JSON_STR(id, ""));
    cJSON_AddStringToObject(flat, "from", TM_JSON_STR(sender, ""));
    cJSON_AddStringToObject(flat, "to", TM_JSON_STR(receiver, recipient));
    cJSON_AddStringToObject(flat, "subject", TM_JSON_STR(subject, ""));
    cJSON_AddStringToObject(flat, "text", TM_JSON_STR(body_text, ""));
    cJSON_AddStringToObject(flat, "html", TM_JSON_STR(body_html, ""));
    cJSON_AddStringToObject(flat, "date", TM_JSON_STR(date, ""));
    cJSON_AddBoolToObject(flat, "isRead", read_at && !cJSON_IsNull(read_at));
    if (attachments) {
        cJSON_AddItemToObject(flat, "attachments", cJSON_Duplicate(attachments, 1));
    }
    return flat;
}

tm_email_t* tm_provider_mailforspam_get_emails(const char *email, int *count) {
    *count = -1;
    if (!email || !email[0]) return NULL;

    char *url = mailforspam_emails_url(email, 100);
    if (!url) return NULL;
    cJSON *root = mailforspam_get_json(url);
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

    for (int i = 0; i < n; i++) {
        cJSON *item = cJSON_GetArrayItem(arr, i);
        const char *id = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(item, "id"), "");
        cJSON *detail = id[0] ? mailforspam_fetch_message(id, email) : NULL;
        cJSON *source = detail ? detail : item;
        cJSON *flat = mailforspam_flatten(source, email);
        emails[i] = tm_normalize_email(flat ? flat : source, email);
        if (flat) cJSON_Delete(flat);
        if (detail) cJSON_Delete(detail);
    }

    cJSON_Delete(root);
    return emails;
}
