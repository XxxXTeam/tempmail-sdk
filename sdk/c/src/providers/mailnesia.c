/**
 * Mailnesia -- https://mailnesia.com
 */

#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAILNESIA_BASE "https://mailnesia.com"
#define MAILNESIA_DOMAIN "mailnesia.com"

static const char *mailnesia_headers[] = {
    "Accept: text/html,*/*",
    NULL
};

static char *mailnesia_strndup(const char *s, size_t len) {
    char *out = (char*)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}

static char *mailnesia_urlencode(const char *s) {
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

static char *mailnesia_mailbox_url(const char *local) {
    char *enc = mailnesia_urlencode(local);
    if (!enc) return NULL;
    size_t cap = strlen(MAILNESIA_BASE) + strlen(enc) + 16;
    char *url = (char*)malloc(cap);
    if (!url) {
        free(enc);
        return NULL;
    }
    snprintf(url, cap, "%s/mailbox/%s", MAILNESIA_BASE, enc);
    free(enc);
    return url;
}

static char *mailnesia_detail_url(const char *local, const char *id) {
    char *base = mailnesia_mailbox_url(local);
    char *id_enc = mailnesia_urlencode(id);
    if (!base || !id_enc) {
        free(base);
        free(id_enc);
        return NULL;
    }
    size_t cap = strlen(base) + strlen(id_enc) + 2;
    char *url = (char*)malloc(cap);
    if (!url) {
        free(base);
        free(id_enc);
        return NULL;
    }
    snprintf(url, cap, "%s/%s", base, id_enc);
    free(base);
    free(id_enc);
    return url;
}

static char *mailnesia_get_html(const char *url) {
    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, mailnesia_headers, NULL, 15);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }
    char *body = tm_strdup(resp->body);
    tm_http_response_free(resp);
    return body;
}

static void mailnesia_random_local(char *buf, size_t cap) {
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

static char *mailnesia_local_part(const char *email) {
    if (!email) return tm_strdup("");
    const char *at = strchr(email, '@');
    size_t len = at ? (size_t)(at - email) : strlen(email);
    return mailnesia_strndup(email, len);
}

static char *mailnesia_clean_text(const char *start, const char *end, int strip_tags) {
    if (!start || !end || end < start) return tm_strdup("");
    size_t len = (size_t)(end - start);
    char *tmp = (char*)malloc(len + 1);
    if (!tmp) return NULL;
    size_t j = 0;
    int in_tag = 0;
    for (const char *p = start; p < end; p++) {
        if (strip_tags && *p == '<') {
            in_tag = 1;
            tmp[j++] = ' ';
            continue;
        }
        if (strip_tags && in_tag) {
            if (*p == '>') in_tag = 0;
            continue;
        }
        if (*p == '&') {
            if ((size_t)(end - p) >= 6 && strncmp(p, "&nbsp;", 6) == 0) { tmp[j++] = ' '; p += 5; continue; }
            if ((size_t)(end - p) >= 5 && strncmp(p, "&amp;", 5) == 0) { tmp[j++] = '&'; p += 4; continue; }
            if ((size_t)(end - p) >= 4 && strncmp(p, "&lt;", 4) == 0) { tmp[j++] = '<'; p += 3; continue; }
            if ((size_t)(end - p) >= 4 && strncmp(p, "&gt;", 4) == 0) { tmp[j++] = '>'; p += 3; continue; }
            if ((size_t)(end - p) >= 6 && strncmp(p, "&quot;", 6) == 0) { tmp[j++] = '"'; p += 5; continue; }
            if ((size_t)(end - p) >= 5 && strncmp(p, "&#39;", 5) == 0) { tmp[j++] = '\''; p += 4; continue; }
        }
        tmp[j++] = *p;
    }
    tmp[j] = '\0';

    char *out = (char*)malloc(j + 1);
    if (!out) {
        free(tmp);
        return NULL;
    }
    size_t k = 0;
    int prev_space = 1;
    for (size_t i = 0; i < j; i++) {
        char c = tmp[i];
        int is_space = (c == ' ' || c == '\n' || c == '\r' || c == '\t');
        if (is_space) {
            if (!prev_space) out[k++] = ' ';
            prev_space = 1;
        } else {
            out[k++] = c;
            prev_space = 0;
        }
    }
    if (k > 0 && out[k - 1] == ' ') k--;
    out[k] = '\0';
    free(tmp);
    return out;
}

static char *mailnesia_attr_value(const char *start, const char *end, const char *needle) {
    const char *p = strstr(start, needle);
    if (!p || p >= end) return tm_strdup("");
    p += strlen(needle);
    const char *q = strchr(p, '"');
    if (!q || q > end) return tm_strdup("");
    return mailnesia_clean_text(p, q, 0);
}

static char *mailnesia_next_anchor_text(const char **cursor, const char *end) {
    const char *a = strstr(*cursor, "<a");
    while (a && a < end) {
        const char *gt = strchr(a, '>');
        const char *close = gt ? strstr(gt + 1, "</a>") : NULL;
        if (!gt || !close || close > end) return tm_strdup("");
        *cursor = close + 4;
        if (strstr(a, "class=\"email\"") && strstr(a, "title=\"Open email\"")) {
            return mailnesia_clean_text(gt + 1, close, 1);
        }
        a = strstr(*cursor, "<a");
    }
    return tm_strdup("");
}

tm_email_info_t* tm_provider_mailnesia_generate(void) {
    srand((unsigned)time(NULL));
    char local[32];
    mailnesia_random_local(local, sizeof(local));
    char *url = mailnesia_mailbox_url(local);
    if (!url) return NULL;
    char *html = mailnesia_get_html(url);
    free(url);
    if (!html) return NULL;
    free(html);

    tm_email_info_t *info = tm_email_info_new();
    if (!info) return NULL;
    char email[160];
    snprintf(email, sizeof(email), "%s@%s", local, MAILNESIA_DOMAIN);
    info->channel = CHANNEL_MAILNESIA;
    info->email = tm_strdup(email);
    return info;
}

static void mailnesia_extract_bodies(const char *page, const char *id, char **text_out, char **html_out) {
    *text_out = tm_strdup("");
    *html_out = tm_strdup("");

    char needle[128];
    snprintf(needle, sizeof(needle), "id=\"text_plain_%s\"", id);
    const char *p = strstr(page, needle);
    if (p) {
        const char *pre = strstr(p, "<pre>");
        const char *close = pre ? strstr(pre + 5, "</pre>") : NULL;
        if (pre && close) {
            free(*text_out);
            *text_out = mailnesia_clean_text(pre + 5, close, 0);
        }
    }

    snprintf(needle, sizeof(needle), "id=\"text_html_%s\"", id);
    p = strstr(page, needle);
    if (p) {
        const char *start = strchr(p, '>');
        char next[128];
        snprintf(next, sizeof(next), "<div id=\"text_plain_%s\"", id);
        const char *end = start ? strstr(start + 1, next) : NULL;
        if (!end && start) end = strstr(start + 1, "</div>");
        if (start && end) {
            const char *body_end = end;
            while (body_end > start + 1 && (*(body_end - 1) == '\n' || *(body_end - 1) == '\r' || *(body_end - 1) == ' ' || *(body_end - 1) == '\t')) {
                body_end--;
            }
            if (body_end - (start + 1) >= 6 && strncmp(body_end - 6, "</div>", 6) == 0) {
                body_end -= 6;
            }
            free(*html_out);
            *html_out = mailnesia_clean_text(start + 1, body_end, 0);
        }
    }
}

tm_email_t* tm_provider_mailnesia_get_emails(const char *email, int *count) {
    *count = -1;
    char *local = mailnesia_local_part(email);
    if (!local || !local[0]) {
        free(local);
        return NULL;
    }
    char *url = mailnesia_mailbox_url(local);
    if (!url) {
        free(local);
        return NULL;
    }
    char *page = mailnesia_get_html(url);
    free(url);
    if (!page) {
        free(local);
        return NULL;
    }

    int n = 0;
    const char *scan = page;
    while ((scan = strstr(scan, "<tr id=\"")) != NULL) {
        const char *end = strstr(scan, "</tr>");
        if (!end) break;
        if (strstr(scan, "emailheader")) n++;
        scan = end + 5;
    }
    *count = n;
    if (n == 0) {
        free(page);
        free(local);
        return NULL;
    }

    tm_email_t *emails = tm_emails_new(n);
    if (!emails) {
        *count = -1;
        free(page);
        free(local);
        return NULL;
    }

    int idx = 0;
    scan = page;
    while (idx < n && (scan = strstr(scan, "<tr id=\"")) != NULL) {
        const char *row_end = strstr(scan, "</tr>");
        if (!row_end) break;
        if (!strstr(scan, "emailheader")) {
            scan = row_end + 5;
            continue;
        }

        const char *id_start = scan + strlen("<tr id=\"");
        const char *id_end = strchr(id_start, '"');
        char *id = id_end ? mailnesia_strndup(id_start, (size_t)(id_end - id_start)) : tm_strdup("");
        char *date = mailnesia_attr_value(scan, row_end, "datetime=\"");
        const char *anchor_cursor = scan;
        char *from = mailnesia_next_anchor_text(&anchor_cursor, row_end);
        char *to = mailnesia_next_anchor_text(&anchor_cursor, row_end);
        char *subject = mailnesia_next_anchor_text(&anchor_cursor, row_end);

        char *text_body = tm_strdup("");
        char *html_body = tm_strdup("");
        char *detail_url = (id && id[0]) ? mailnesia_detail_url(local, id) : NULL;
        if (detail_url) {
            char *detail = mailnesia_get_html(detail_url);
            if (detail) {
                mailnesia_extract_bodies(detail, id, &text_body, &html_body);
                free(detail);
            }
            free(detail_url);
        }

        cJSON *obj = cJSON_CreateObject();
        cJSON_AddStringToObject(obj, "id", id ? id : "");
        cJSON_AddStringToObject(obj, "date", date ? date : "");
        cJSON_AddStringToObject(obj, "from", from ? from : "");
        cJSON_AddStringToObject(obj, "to", (to && to[0]) ? to : email);
        cJSON_AddStringToObject(obj, "subject", subject ? subject : "");
        cJSON_AddStringToObject(obj, "text", text_body ? text_body : "");
        cJSON_AddStringToObject(obj, "html", html_body ? html_body : "");
        emails[idx++] = tm_normalize_email(obj, email);
        cJSON_Delete(obj);

        free(id);
        free(date);
        free(from);
        free(to);
        free(subject);
        free(text_body);
        free(html_body);
        scan = row_end + 5;
    }

    free(page);
    free(local);
    return emails;
}
