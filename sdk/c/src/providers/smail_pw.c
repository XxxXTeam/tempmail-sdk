/**
 * smail.pw：POST _root.data intent=generate，Cookie __session；GET 拉取 RSC 响应并解析邮件
 */
#include "tempmail_internal.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SM_ROOT "https://smail.pw/_root.data"

static const char *sm_post_headers[] = {
    "Accept: */*",
    "Content-Type: application/x-www-form-urlencoded;charset=UTF-8",
    "accept-language: zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
    "cache-control: no-cache",
    "dnt: 1",
    "origin: https://smail.pw",
    "pragma: no-cache",
    "referer: https://smail.pw/",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    "sec-ch-ua: \"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"146\"",
    "sec-ch-ua-mobile: ?0",
    "sec-ch-ua-platform: \"Windows\"",
    "sec-fetch-dest: empty",
    "sec-fetch-mode: cors",
    "sec-fetch-site: same-origin",
    NULL
};

static const char *sm_get_headers_base[] = {
    "Accept: */*",
    "accept-language: zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
    "cache-control: no-cache",
    "dnt: 1",
    "pragma: no-cache",
    "referer: https://smail.pw/",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    "sec-ch-ua: \"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"146\"",
    "sec-ch-ua-mobile: ?0",
    "sec-ch-ua-platform: \"Windows\"",
    "sec-fetch-dest: empty",
    "sec-fetch-mode: cors",
    "sec-fetch-site: same-origin",
    NULL
};

static const char *read_jstr(const char *p, char *buf, size_t cap) {
    if (!p || *p != '"') return NULL;
    p++;
    const char *e = strchr(p, '"');
    if (!e) return NULL;
    size_t n = (size_t)(e - p);
    if (n >= cap) return NULL;
    memcpy(buf, p, n);
    buf[n] = '\0';
    return e + 1;
}

static char *tm_smail_extract_inbox(const char *body) {
    if (!body) return NULL;
    for (const char *p = body; *p; p++) {
        if (*p != '"') continue;
        const char *q = strchr(p + 1, '"');
        if (!q) break;
        size_t n = (size_t)(q - (p + 1));
        if (n < 8 || n >= 200) { p = q; continue; }
        char tmp[256];
        memcpy(tmp, p + 1, n);
        tmp[n] = '\0';
        if (strstr(tmp, "@smail.pw")) return tm_strdup(tmp);
        p = q;
    }
    return NULL;
}

static char *tm_smail_extract_session(tm_http_response_t *resp) {
    if (!resp || !resp->cookies) return NULL;
    char *s = strstr(resp->cookies, "__session=");
    if (!s) return NULL;
    s += 10;
    char *end = strchr(s, ';');
    size_t len = end ? (size_t)(end - s) : strlen(s);
    char *out = (char *)malloc(len + 12);
    if (!out) return NULL;
    memcpy(out, "__session=", 10);
    memcpy(out + 10, s, len);
    out[10 + len] = '\0';
    return out;
}

static const char *smail_try_parse_mail_v1(const char *p, const char *recipient,
        char *id, char *to, char *fn, char *fa, char *subj, long long *tms) {
    (void)recipient;
    if (strncmp(p, "\"id\",", 5) != 0) return NULL;
    p += 5;
    p = read_jstr(p, id, 256);
    if (!p || strncmp(p, ",\"to_address\",", 14) != 0) return NULL;
    p += 14;
    p = read_jstr(p, to, 512);
    if (!p || strncmp(p, ",\"from_name\",", 13) != 0) return NULL;
    p += 13;
    p = read_jstr(p, fn, 256);
    if (!p || strncmp(p, ",\"from_address\",", 16) != 0) return NULL;
    p += 16;
    p = read_jstr(p, fa, 256);
    if (!p || strncmp(p, ",\"subject\",", 11) != 0) return NULL;
    p += 11;
    p = read_jstr(p, subj, 512);
    if (!p || strncmp(p, ",\"time\",", 8) != 0) return NULL;
    p += 8;
    char *endptr = NULL;
    *tms = strtoll(p, &endptr, 10);
    if (!endptr || endptr == p) return NULL;
    return (const char *)endptr;
}

static const char *smail_try_parse_mail_v2(const char *p, const char *recipient,
        char *id, char *to, char *fn, char *fa, char *subj, long long *tms) {
    (void)recipient;
    if (strncmp(p, "\"id\",", 5) != 0) return NULL;
    p += 5;
    p = read_jstr(p, id, 256);
    if (!p || strncmp(p, ",\"from_name\",", 13) != 0) return NULL;
    p += 13;
    p = read_jstr(p, fn, 256);
    if (!p || strncmp(p, ",\"from_address\",", 16) != 0) return NULL;
    p += 16;
    p = read_jstr(p, fa, 256);
    if (!p || strncmp(p, ",\"subject\",", 11) != 0) return NULL;
    p += 11;
    p = read_jstr(p, subj, 512);
    if (!p || strncmp(p, ",\"time\",", 8) != 0) return NULL;
    p += 8;
    char *endptr = NULL;
    *tms = strtoll(p, &endptr, 10);
    snprintf(to, 512, "%s", recipient);
    if (!endptr || endptr == p) return NULL;
    return (const char *)endptr;
}

tm_email_info_t *tm_provider_smail_pw_generate(void) {
    tm_http_response_t *resp = tm_http_request(TM_HTTP_POST, SM_ROOT, sm_post_headers, "intent=generate", 15);
    if (!resp || resp->status < 200 || resp->status >= 300) { tm_http_response_free(resp); return NULL; }

    char *tok = tm_smail_extract_session(resp);
    if (!tok) { tm_http_response_free(resp); return NULL; }

    char *inbox = tm_smail_extract_inbox(resp->body);
    tm_http_response_free(resp);
    if (!inbox) { free(tok); return NULL; }

    tm_email_info_t *info = tm_email_info_new();
    info->channel = CHANNEL_SMAIL_PW;
    info->email = inbox;
    info->token = tok;
    return info;
}

tm_email_t *tm_provider_smail_pw_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    char cookie_hdr[768];
    snprintf(cookie_hdr, sizeof(cookie_hdr), "Cookie: %s", token);

    const char *gh[20];
    int gi = 0;
    gh[gi++] = cookie_hdr;
    for (int i = 0; sm_get_headers_base[i] && gi < 19; i++) gh[gi++] = sm_get_headers_base[i];
    gh[gi] = NULL;

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, SM_ROOT, gh, NULL, 15);
    if (!resp || resp->status < 200 || resp->status >= 300) { tm_http_response_free(resp); return NULL; }

    const char *body = resp->body;
    if (!body) { tm_http_response_free(resp); *count = 0; return NULL; }

    typedef struct { char *id; char *to; char *from_addr; char *subj; long long ts; } row_t;
    row_t rows[64];
    int nr = 0;

    for (const char *p = body; *p && nr < 64; ) {
        const char *hit = strstr(p, "\"id\",");
        if (!hit) break;
        char id[256], to[512], fn[256], fa[256], subj[512];
        long long tms = 0;
        const char *next = smail_try_parse_mail_v1(hit, email, id, to, fn, fa, subj, &tms);
        if (!next) next = smail_try_parse_mail_v2(hit, email, id, to, fn, fa, subj, &tms);
        if (next) {
            char from_comb[512];
            snprintf(from_comb, sizeof(from_comb), "%s <%s>", fn, fa);
            rows[nr].id = tm_strdup(id);
            rows[nr].to = tm_strdup((*to && *to) ? to : email);
            rows[nr].from_addr = tm_strdup(from_comb);
            rows[nr].subj = tm_strdup(subj);
            rows[nr].ts = tms;
            nr++;
            p = next;
        } else {
            p = hit + 5;
        }
    }

    tm_http_response_free(resp);

    if (nr == 0) { *count = 0; return NULL; }

    tm_email_t *emails = tm_emails_new(nr);
    for (int i = 0; i < nr; i++) {
        cJSON *flat = cJSON_CreateObject();
        cJSON_AddStringToObject(flat, "id", rows[i].id);
        cJSON_AddStringToObject(flat, "from_address", rows[i].from_addr);
        cJSON_AddStringToObject(flat, "to", rows[i].to);
        cJSON_AddStringToObject(flat, "subject", rows[i].subj);
        cJSON_AddStringToObject(flat, "text", "");
        cJSON_AddStringToObject(flat, "html", "");
        char datebuf[48];
        time_t sec = (time_t)(rows[i].ts / 1000);
        struct tm tmu;
#ifdef _WIN32
        if (gmtime_s(&tmu, &sec) != 0) memset(&tmu, 0, sizeof(tmu));
#else
        gmtime_r(&sec, &tmu);
#endif
        strftime(datebuf, sizeof(datebuf), "%Y-%m-%dT%H:%M:%SZ", &tmu);
        cJSON_AddStringToObject(flat, "date", datebuf);
        emails[i] = tm_normalize_email(flat, email);
        cJSON_Delete(flat);
        free(rows[i].id);
        free(rows[i].to);
        free(rows[i].from_addr);
        free(rows[i].subj);
    }
    *count = nr;
    return emails;
}
