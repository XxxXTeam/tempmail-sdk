/**
 * FakeMail -- https://www.fakemail.net
 */

#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FAKEMAIL_BASE "https://www.fakemail.net"

static const char *fakemail_home_headers[] = {
    "Accept: text/html,application/xhtml+xml",
    "User-Agent: Mozilla/5.0",
    NULL
};

static char *fakemail_merge_cookie(const char *prev, const char *set_cookie) {
    if (!set_cookie || !set_cookie[0]) return tm_strdup(prev ? prev : "");
    const char *end = strchr(set_cookie, ';');
    size_t n = end ? (size_t)(end - set_cookie) : strlen(set_cookie);
    if (!prev || !prev[0]) {
        char *out = (char*)malloc(n + 1);
        if (!out) return NULL;
        memcpy(out, set_cookie, n);
        out[n] = '\0';
        return out;
    }
    size_t cap = strlen(prev) + n + 3;
    char *out = (char*)malloc(cap);
    if (!out) return NULL;
    snprintf(out, cap, "%s; %.*s", prev, (int)n, set_cookie);
    return out;
}

static char *fakemail_csrf(const char *html) {
    const char *needle = "const CSRF=\"";
    const char *p = html ? strstr(html, needle) : NULL;
    if (!p) return tm_strdup("");
    p += strlen(needle);
    const char *end = strchr(p, '"');
    if (!end) return tm_strdup("");
    size_t n = (size_t)(end - p);
    char *out = (char*)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, p, n);
    out[n] = '\0';
    return out;
}

static const char *fakemail_json_start(const char *body) {
    if (!body) return "";
    const unsigned char *p = (const unsigned char*)body;
    if (p[0] == 0xef && p[1] == 0xbb && p[2] == 0xbf) return body + 3;
    return body;
}

static char *fakemail_urlencode(const char *s) {
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

static cJSON *fakemail_ajax_json(const char *path, const char *cookie, const char *post_body) {
    size_t cookie_cap = strlen(cookie) + 9;
    char *cookie_header = (char*)malloc(cookie_cap);
    if (!cookie_header) return NULL;
    snprintf(cookie_header, cookie_cap, "Cookie: %s", cookie);
    const char *headers[] = {
        "Accept: application/json, text/javascript, */*; q=0.01",
        "X-Requested-With: XMLHttpRequest",
        "Referer: https://www.fakemail.net/",
        "User-Agent: Mozilla/5.0",
        cookie_header,
        post_body ? "Content-Type: application/x-www-form-urlencoded" : NULL,
        NULL
    };
    char url[512];
    snprintf(url, sizeof(url), "%s%s", FAKEMAIL_BASE, path);
    tm_http_response_t *resp = tm_http_request_ipv4(post_body ? TM_HTTP_POST : TM_HTTP_GET, url, headers, post_body, 15);
    free(cookie_header);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }
    cJSON *root = cJSON_Parse(fakemail_json_start(resp->body));
    tm_http_response_free(resp);
    return root;
}

tm_email_info_t* tm_provider_fakemail_generate(void) {
    tm_http_response_t *home = tm_http_request_ipv4(TM_HTTP_GET, FAKEMAIL_BASE "/", fakemail_home_headers, NULL, 15);
    if (!home || home->status < 200 || home->status >= 300) {
        tm_http_response_free(home);
        return NULL;
    }
    char *csrf = fakemail_csrf(home->body);
    char *cookie = fakemail_merge_cookie("", home->cookies);
    tm_http_response_free(home);
    if (!csrf || !csrf[0] || !cookie || !cookie[0]) {
        free(csrf);
        free(cookie);
        return NULL;
    }
    char *csrf_enc = fakemail_urlencode(csrf);
    free(csrf);
    if (!csrf_enc) {
        free(cookie);
        return NULL;
    }
    size_t path_cap = strlen(csrf_enc) + 32;
    char *path = (char*)malloc(path_cap);
    if (!path) {
        free(csrf_enc);
        free(cookie);
        return NULL;
    }
    snprintf(path, path_cap, "/index/index?csrf_token=%s", csrf_enc);
    free(csrf_enc);
    cJSON *root = fakemail_ajax_json(path, cookie, NULL);
    free(path);
    if (!root) {
        free(cookie);
        return NULL;
    }
    const char *email = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "email"), "");
    if (!email[0] || !strchr(email, '@')) {
        cJSON_Delete(root);
        free(cookie);
        return NULL;
    }
    tm_email_info_t *info = tm_email_info_new();
    if (!info) {
        cJSON_Delete(root);
        free(cookie);
        return NULL;
    }
    info->channel = CHANNEL_FAKEMAIL;
    info->email = tm_strdup(email);
    info->token = cookie;
    cJSON_Delete(root);
    return info;
}

static cJSON *fakemail_fetch_detail(const char *cookie, const char *id) {
    char *id_enc = fakemail_urlencode(id);
    if (!id_enc) return NULL;
    size_t body_cap = strlen(id_enc) + 4;
    char *body = (char*)malloc(body_cap);
    if (!body) {
        free(id_enc);
        return NULL;
    }
    snprintf(body, body_cap, "id=%s", id_enc);
    free(id_enc);
    cJSON *root = fakemail_ajax_json("/index/email", cookie, body);
    free(body);
    return root;
}

static cJSON *fakemail_message_raw(const cJSON *row, const cJSON *detail, const char *recipient) {
    cJSON *raw = cJSON_CreateObject();
    if (!raw) return NULL;
    const cJSON *id = detail ? cJSON_GetObjectItemCaseSensitive((cJSON*)detail, "id") : NULL;
    if (!id) id = cJSON_GetObjectItemCaseSensitive((cJSON*)row, "id");
    const cJSON *from = detail ? cJSON_GetObjectItemCaseSensitive((cJSON*)detail, "od") : NULL;
    if (!from) from = cJSON_GetObjectItemCaseSensitive((cJSON*)row, "od");
    const cJSON *subject = detail ? cJSON_GetObjectItemCaseSensitive((cJSON*)detail, "predmet") : NULL;
    if (!subject) subject = cJSON_GetObjectItemCaseSensitive((cJSON*)row, "predmet");
    const cJSON *html = detail ? cJSON_GetObjectItemCaseSensitive((cJSON*)detail, "telo") : NULL;
    cJSON_AddStringToObject(raw, "id", TM_JSON_STR(id, ""));
    cJSON_AddStringToObject(raw, "from", TM_JSON_STR(from, ""));
    cJSON_AddStringToObject(raw, "to", recipient);
    cJSON_AddStringToObject(raw, "subject", TM_JSON_STR(subject, ""));
    cJSON_AddStringToObject(raw, "text", "");
    cJSON_AddStringToObject(raw, "html", TM_JSON_STR(html, ""));
    cJSON_AddStringToObject(raw, "date", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON*)row, "kdy"), ""));
    const char *read_flag = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON*)row, "precteno"), "");
    cJSON_AddBoolToObject(raw, "isRead", strcmp(read_flag, "precteno") == 0);
    return raw;
}

tm_email_t* tm_provider_fakemail_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    if (!token || !token[0] || !email || !email[0]) return NULL;
    cJSON *rows = fakemail_ajax_json("/index/refresh", token, NULL);
    if (!rows || !cJSON_IsArray(rows)) {
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
        cJSON *row = cJSON_GetArrayItem(rows, i);
        const char *id = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(row, "id"), "");
        cJSON *detail = id[0] ? fakemail_fetch_detail(token, id) : NULL;
        cJSON *raw = fakemail_message_raw(row, detail, email);
        emails[i] = tm_normalize_email(raw ? raw : row, email);
        if (raw) cJSON_Delete(raw);
        if (detail) cJSON_Delete(detail);
    }
    cJSON_Delete(rows);
    return emails;
}
