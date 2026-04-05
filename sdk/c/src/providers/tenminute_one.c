/**
 * 10minutemail.one：SSR __NUXT_DATA__ 中的 mailServiceToken + GET web API 收信
 */
#include "tempmail_internal.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define tenm_strcasecmp _stricmp
#else
#include <strings.h>
#define tenm_strcasecmp strcasecmp
#endif

#define TENM_SITE "https://10minutemail.one"
#define TENM_API  "https://web.10minutemail.one/api/v1"

static void tenm_random_hex(char *out, size_t nbytes) {
    static const char *hex = "0123456789abcdef";
    for (size_t i = 0; i < nbytes; i++) {
        unsigned v = (unsigned)rand() % 256;
        out[i * 2] = hex[(v >> 4) & 15];
        out[i * 2 + 1] = hex[v & 15];
    }
    out[nbytes * 2] = '\0';
}

static void tenm_random_local(char *buf, int len) {
    static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    for (int i = 0; i < len; i++)
        buf[i] = chars[rand() % (int)(sizeof(chars) - 1)];
    buf[len] = '\0';
}

static void tenm_enc_email_path(const char *email, char *out, size_t cap) {
    size_t o = 0;
    for (const char *p = email; *p && o + 4 < cap; p++) {
        if (*p == '@') {
            out[o++] = '%';
            out[o++] = '4';
            out[o++] = '0';
        } else {
            out[o++] = *p;
        }
    }
    out[o] = '\0';
}

static char *tenm_extract_nuxt_inner(const char *html) {
    const char *p = strstr(html, "id=\"__NUXT_DATA__\"");
    if (!p)
        p = strstr(html, "id='__NUXT_DATA__'");
    if (!p)
        return NULL;
    p = strchr(p, '>');
    if (!p)
        return NULL;
    p++;
    const char *end = strstr(p, "</script>");
    if (!end)
        return NULL;
    size_t n = (size_t)(end - p);
    char *out = (char *)malloc(n + 1);
    if (!out)
        return NULL;
    memcpy(out, p, n);
    out[n] = '\0';
    return out;
}

static cJSON *tenm_resolve(cJSON *arr, cJSON *v, int depth) {
    if (depth > 64 || !v)
        return v;
    if (cJSON_IsNumber(v)) {
        int idx = (int)v->valuedouble;
        if (idx >= 0 && idx < cJSON_GetArraySize(arr))
            return tenm_resolve(arr, cJSON_GetArrayItem(arr, idx), depth + 1);
    }
    return v;
}

static int tenm_is_jwt_shape(const char *s) {
    if (!s)
        return 0;
    int dots = 0;
    for (const char *p = s; *p; p++) {
        if (*p == '.')
            dots++;
        else if (!isalnum((unsigned char)*p) && *p != '_' && *p != '-')
            return 0;
    }
    return dots == 2 && strlen(s) > 20;
}

static const char *tenm_find_jwt_in_array(cJSON *arr) {
    int n = cJSON_GetArraySize(arr);
    int lim = n < 48 ? n : 48;
    for (int i = 0; i < lim; i++) {
        cJSON *el = cJSON_GetArrayItem(arr, i);
        if (!cJSON_IsObject(el))
            continue;
        cJSON *ref = cJSON_GetObjectItemCaseSensitive(el, "mailServiceToken");
        if (!ref)
            continue;
        cJSON *tok = tenm_resolve(arr, ref, 0);
        if (cJSON_IsString(tok) && tenm_is_jwt_shape(tok->valuestring))
            return tok->valuestring;
    }
    for (int i = 0; i < n; i++) {
        cJSON *el = cJSON_GetArrayItem(arr, i);
        if (!cJSON_IsObject(el))
            continue;
        cJSON *ref = cJSON_GetObjectItemCaseSensitive(el, "mailServiceToken");
        if (!ref)
            continue;
        cJSON *tok = tenm_resolve(arr, ref, 0);
        if (cJSON_IsString(tok) && tenm_is_jwt_shape(tok->valuestring))
            return tok->valuestring;
    }
    for (int i = 0; i < n; i++) {
        cJSON *el = cJSON_GetArrayItem(arr, i);
        if (cJSON_IsString(el) && tenm_is_jwt_shape(el->valuestring))
            return el->valuestring;
    }
    return NULL;
}

static cJSON *tenm_parse_domain_array(const char *html, const char *field) {
    char key[64];
    snprintf(key, sizeof(key), "%s:\"", field);
    const char *start = strstr(html, key);
    if (!start)
        return NULL;
    start += strlen(key);
    if (*start != '[')
        return NULL;
    int depth = 0;
    const char *j = start;
    for (; *j; j++) {
        if (*j == '[')
            depth++;
        else if (*j == ']') {
            depth--;
            if (depth == 0) {
                j++;
                break;
            }
        }
    }
    size_t len = (size_t)(j - start);
    char *buf = (char *)malloc(len + 1);
    if (!buf)
        return NULL;
    memcpy(buf, start, len);
    buf[len] = '\0';
    char *r = buf, *w = buf;
    while (*r) {
        if (r[0] == '\\' && r[1] == '"') {
            *w++ = '"';
            r += 2;
        } else
            *w++ = *r++;
    }
    *w = '\0';
    cJSON *ar = cJSON_Parse(buf);
    free(buf);
    return ar;
}

static const char *tenm_pick_domain(cJSON *doms, const char *domain_opt) {
    int n = cJSON_GetArraySize(doms);
    if (n <= 0)
        return NULL;
    if (domain_opt && strchr(domain_opt, '.')) {
        for (int i = 0; i < n; i++) {
            cJSON *it = cJSON_GetArrayItem(doms, i);
            const char *s = cJSON_GetStringValue(it);
            if (s && tenm_strcasecmp(s, domain_opt) == 0)
                return s;
        }
    }
    int idx = rand() % n;
    return cJSON_GetStringValue(cJSON_GetArrayItem(doms, idx));
}

static void tenm_merge_obj(cJSON *dst, cJSON *src) {
    cJSON *it;
    if (!cJSON_IsObject(dst) || !cJSON_IsObject(src))
        return;
    for (it = src->child; it; it = it->next) {
        const char *k = it->string;
        if (k && !cJSON_GetObjectItemCaseSensitive(dst, k))
            cJSON_AddItemToObject(dst, k, cJSON_Duplicate(it, 1));
    }
}

static int tenm_item_needs_detail(cJSON *obj) {
    const char *id = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(obj, "id"));
    if (!id || !*id)
        return 0;
    const char *subj = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(obj, "subject"));
    if (!subj || !*subj)
        subj = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(obj, "mail_title"));
    const char *tx = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(obj, "text"));
    if (!tx || !*tx)
        tx = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(obj, "body"));
    if (!tx || !*tx)
        tx = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(obj, "html"));
    return (!subj || !*subj) && (!tx || !*tx);
}

tm_email_info_t *tm_provider_tenminute_one_generate(const char *domain) {
    srand((unsigned)time(NULL));

    const char *loc = "zh";
    char locbuf[32];
    if (domain && *domain) {
        int has_dot = strchr(domain, '.') != NULL;
        int has_slash = strchr(domain, '/') != NULL;
        if (!has_dot || has_slash) {
            snprintf(locbuf, sizeof(locbuf), "%s", domain);
            loc = locbuf;
        }
    }

    char page_url[160];
    snprintf(page_url, sizeof(page_url), "%s/%s", TENM_SITE, loc);

    const char *page_hdrs[] = {
        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
        "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8",
        "Cache-Control: no-cache",
        "Pragma: no-cache",
        "DNT: 1",
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
        NULL
    };
    char refh[192];
    snprintf(refh, sizeof(refh), "Referer: %s", page_url);
    const char *page_hdrs2[] = {
        page_hdrs[0], page_hdrs[1], page_hdrs[2], page_hdrs[3], page_hdrs[4], page_hdrs[5], refh, NULL
    };

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, page_url, page_hdrs2, NULL, 20);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }

    char *nuxt_raw = tenm_extract_nuxt_inner(resp->body);
    if (!nuxt_raw) {
        tm_http_response_free(resp);
        return NULL;
    }

    cJSON *arr = cJSON_Parse(nuxt_raw);
    free(nuxt_raw);
    if (!cJSON_IsArray(arr)) {
        cJSON_Delete(arr);
        tm_http_response_free(resp);
        return NULL;
    }

    const char *jwt = tenm_find_jwt_in_array(arr);
    cJSON_Delete(arr);
    if (!jwt) {
        tm_http_response_free(resp);
        return NULL;
    }

    cJSON *doms = tenm_parse_domain_array(resp->body, "emailDomains");
    tm_http_response_free(resp);
    if (!doms || cJSON_GetArraySize(doms) == 0) {
        cJSON_Delete(doms);
        doms = cJSON_Parse("[\"xghff.com\",\"oqqaj.com\",\"psovv.com\"]");
        if (!doms)
            return NULL;
    }

    const char *mail_dom = tenm_pick_domain(doms, domain);
    if (!mail_dom) {
        cJSON_Delete(doms);
        return NULL;
    }
    char dom_copy[128];
    snprintf(dom_copy, sizeof(dom_copy), "%s", mail_dom);
    cJSON_Delete(doms);

    char local[16];
    tenm_random_local(local, 10);
    char address[192];
    snprintf(address, sizeof(address), "%s@%s", local, dom_copy);

    tm_email_info_t *info = tm_email_info_new();
    info->channel = CHANNEL_10MINUTE_ONE;
    info->email = tm_strdup(address);
    info->token = tm_strdup(jwt);
    info->expires_at = 0;
    return info;
}

tm_email_t *tm_provider_tenminute_one_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    if (!token || !email)
        return NULL;

    char enc[256];
    tenm_enc_email_path(email, enc, sizeof(enc));

    char path[512];
    snprintf(path, sizeof(path), "%s/mailbox/%s", TENM_API, enc);

    char rid[40];
    tenm_random_hex(rid, 16);
    char auth[640];
    char ts[32];
    snprintf(ts, sizeof(ts), "%lld", (long long)time(NULL));
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", token);

    char xr[96];
    snprintf(xr, sizeof(xr), "X-Request-ID: %s", rid);
    char xt[64];
    snprintf(xt, sizeof(xt), "X-Timestamp: %s", ts);

    const char *headers[] = {
        "Accept: */*",
        "Content-Type: application/json",
        "Origin: " TENM_SITE,
        "Referer: " TENM_SITE "/",
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
        auth,
        xr,
        xt,
        NULL
    };

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, path, headers, NULL, 20);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }

    cJSON *list = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!cJSON_IsArray(list)) {
        cJSON_Delete(list);
        return NULL;
    }

    int n = cJSON_GetArraySize(list);
    *count = n;
    if (n == 0) {
        cJSON_Delete(list);
        return NULL;
    }

    tm_email_t *emails = tm_emails_new(n);
    if (!emails) {
        cJSON_Delete(list);
        *count = -1;
        return NULL;
    }

    for (int i = 0; i < n; i++) {
        cJSON *row = cJSON_GetArrayItem(list, i);
        if (!cJSON_IsObject(row)) {
            memset(&emails[i], 0, sizeof(tm_email_t));
            continue;
        }
        cJSON *dup = cJSON_Duplicate(row, 1);
        if (tenm_item_needs_detail(dup)) {
            const char *mid = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(dup, "id"));
            if (mid && *mid) {
                char mid_enc[256];
                tenm_enc_email_path(mid, mid_enc, sizeof(mid_enc));
                char durl[768];
                snprintf(durl, sizeof(durl), "%s/mailbox/%s/%s", TENM_API, enc, mid_enc);
                tenm_random_hex(rid, 16);
                snprintf(xr, sizeof(xr), "X-Request-ID: %s", rid);
                snprintf(xt, sizeof(xt), "X-Timestamp: %lld", (long long)time(NULL));
                tm_http_response_t *dr = tm_http_request(TM_HTTP_GET, durl, headers, NULL, 20);
                if (dr && dr->status >= 200 && dr->status < 300) {
                    cJSON *detail = cJSON_Parse(dr->body);
                    if (detail && cJSON_IsObject(detail)) {
                        tenm_merge_obj(dup, detail);
                    }
                    cJSON_Delete(detail);
                }
                tm_http_response_free(dr);
            }
        }
        emails[i] = tm_normalize_email(dup, email);
        cJSON_Delete(dup);
    }
    cJSON_Delete(list);
    return emails;
}
