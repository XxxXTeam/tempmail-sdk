/**
 * eTempMail — https://etempmail.com
 * GET /zh 会话，POST /getEmailAddress、/getInbox。
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ET_BASE "https://etempmail.com"

static const char *et_hdrs[] = {
    "Accept: */*",
    "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
    "Cache-Control: no-cache",
    "DNT: 1",
    "Origin: https://etempmail.com",
    "Pragma: no-cache",
    "Referer: https://etempmail.com/zh",
    "sec-ch-ua: \"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"146\"",
    "sec-ch-ua-mobile: ?0",
    "sec-ch-ua-platform: \"Windows\"",
    "sec-fetch-dest: empty",
    "sec-fetch-mode: cors",
    "sec-fetch-site: same-origin",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    "X-Requested-With: XMLHttpRequest",
    NULL
};

static char *et_concat_cookie(const char *a, const char *b) {
    if (!b || !b[0]) return tm_strdup(a ? a : "");
    if (!a || !a[0]) return tm_strdup(b);
    size_t na = strlen(a), nb = strlen(b);
    char *o = (char *)malloc(na + nb + 8);
    if (!o) return NULL;
    snprintf(o, na + nb + 8, "%s; %s", a, b);
    return o;
}

static int et_has_ci_session(const char *ck) {
    return ck && strstr(ck, "ci_session=");
}

static int et_is_domain_char(unsigned char ch) {
    return (ch >= 'A' && ch <= 'Z') ||
           (ch >= 'a' && ch <= 'z') ||
           (ch >= '0' && ch <= '9') ||
           ch == '-';
}

static int et_valid_domain(const char *domain) {
    if (!domain || !domain[0]) return 0;
    size_t len = strlen(domain);
    if (len > 253 || domain[0] == '.' || domain[len - 1] == '.') return 0;

    int labels = 0;
    const char *start = domain;
    for (const char *p = domain;; p++) {
        if (*p == '.' || *p == '\0') {
            size_t label_len = (size_t)(p - start);
            if (label_len == 0 || label_len > 63) return 0;
            if (start[0] == '-' || p[-1] == '-') return 0;
            for (const char *q = start; q < p; q++) {
                if (!et_is_domain_char((unsigned char)*q)) return 0;
            }
            labels++;
            if (*p == '\0') break;
            start = p + 1;
        }
    }
    return labels >= 2;
}

static int et_valid_generated_address(const char *addr) {
    if (!addr || !addr[0]) return 0;
    size_t len = strlen(addr);
    if (len > 254) return 0;
    const char *at = strchr(addr, '@');
    if (!at || strchr(at + 1, '@')) return 0;

    size_t local_len = (size_t)(at - addr);
    if (local_len == 0 || local_len > 64) return 0;
    if (addr[0] == '.' || at[-1] == '.') return 0;
    for (const char *p = addr; p < at; p++) {
        unsigned char ch = (unsigned char)*p;
        if (ch <= 32 || ch == 127) return 0;
        if (p + 1 < at && p[0] == '.' && p[1] == '.') return 0;
    }
    return et_valid_domain(at + 1);
}

tm_email_info_t *tm_provider_etempmail_generate(void) {
    tm_http_response_t *r = tm_http_request(TM_HTTP_GET, ET_BASE "/zh", et_hdrs, NULL, 15);
    if (!r || r->status < 200 || r->status >= 300) {
        tm_http_response_free(r);
        return NULL;
    }
    char *cookie = r->cookies ? tm_strdup(r->cookies) : tm_strdup("");
    tm_http_response_free(r);
    if (!et_has_ci_session(cookie)) {
        free(cookie);
        return NULL;
    }

    char ck_line[8704];
    snprintf(ck_line, sizeof(ck_line), "Cookie: %s", cookie);
    const char *post_hdrs[24];
    int i = 0;
    for (const char **p = et_hdrs; *p; p++) post_hdrs[i++] = *p;
    post_hdrs[i++] = ck_line;
    post_hdrs[i] = NULL;

    tm_http_response_t *r2 = tm_http_request(
        TM_HTTP_POST, ET_BASE "/getEmailAddress", post_hdrs, "", 15);
    if (!r2 || r2->status < 200 || r2->status >= 300) {
        tm_http_response_free(r2);
        free(cookie);
        return NULL;
    }
    if (r2->cookies && r2->cookies[0]) {
        char *m = et_concat_cookie(cookie, r2->cookies);
        free(cookie);
        cookie = m;
        if (!cookie) {
            tm_http_response_free(r2);
            return NULL;
        }
    }

    cJSON *cj = cJSON_Parse(r2->body ? r2->body : "");
    tm_http_response_free(r2);
    if (!cj || !cJSON_IsObject(cj)) {
        cJSON_Delete(cj);
        free(cookie);
        return NULL;
    }
    const char *addr = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(cj, "address"));
    if (!addr || !addr[0]) {
        cJSON_Delete(cj);
        free(cookie);
        return NULL;
    }
    if (!et_valid_generated_address(addr)) {
        cJSON_Delete(cj);
        free(cookie);
        return NULL;
    }

    tm_email_info_t *info = tm_email_info_new();
    info->channel = CHANNEL_ETEMPMAIL;
    info->email = tm_strdup(addr);
    info->token = cookie;
    cookie = NULL;

    cJSON_Delete(cj);
    return info;
}

tm_email_t *tm_provider_etempmail_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    if (!token || !token[0] || !email) return NULL;

    char ck_line[8704];
    snprintf(ck_line, sizeof(ck_line), "Cookie: %s", token);
    const char *post_hdrs[24];
    int i = 0;
    for (const char **p = et_hdrs; *p; p++) post_hdrs[i++] = *p;
    post_hdrs[i++] = ck_line;
    post_hdrs[i] = NULL;

    tm_http_response_t *r = tm_http_request(
        TM_HTTP_POST, ET_BASE "/getInbox", post_hdrs, "", 15);
    if (!r || r->status < 200 || r->status >= 300) {
        tm_http_response_free(r);
        return NULL;
    }
    cJSON *arr = cJSON_Parse(r->body ? r->body : "");
    tm_http_response_free(r);
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
    for (int i = 0; i < n; i++) {
        cJSON *raw = cJSON_GetArrayItem(arr, i);
        const char *from = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(raw, "from"));
        const char *subj = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(raw, "subject"));
        const char *dt = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(raw, "date"));
        const char *body = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(raw, "body"));
        char idbuf[2048];
        snprintf(idbuf, sizeof(idbuf), "%s\n%s\n%s\n%d\n%s",
            from ? from : "", subj ? subj : "", dt ? dt : "", i, email);
        cJSON *one = cJSON_CreateObject();
        cJSON_AddStringToObject(one, "id", idbuf);
        cJSON_AddStringToObject(one, "from", from ? from : "");
        cJSON_AddStringToObject(one, "subject", subj ? subj : "");
        cJSON_AddStringToObject(one, "body", body ? body : "");
        cJSON_AddStringToObject(one, "date", dt ? dt : "");
        cJSON_AddBoolToObject(one, "isRead", 0);
        emails[i] = tm_normalize_email(one, email);
        cJSON_Delete(one);
    }
    cJSON_Delete(arr);
    return emails;
}
