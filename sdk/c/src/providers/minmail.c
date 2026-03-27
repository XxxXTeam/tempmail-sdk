/**
 * MinMail — https://minmail.app/api
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MM_BASE "https://minmail.app/api"

static void mm_random_seg(char *buf, int len) {
    static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    for (int i = 0; i < len; i++) buf[i] = chars[rand() % (int)(sizeof(chars) - 1)];
    buf[len] = '\0';
}

static void mm_visitor_id(char *out, size_t cap) {
    char a[16], b[8], c[8], d[8], e[20];
    mm_random_seg(a, 8); mm_random_seg(b, 4); mm_random_seg(c, 4); mm_random_seg(d, 4); mm_random_seg(e, 12);
    snprintf(out, cap, "%s-%s-%s-%s-%s", a, b, c, d, e);
}

static void mm_cookie(char *out, size_t cap) {
    long long now = (long long)time(NULL) * 1000LL;
    int rnd = rand() % 1000000;
    snprintf(out, cap,
        "_ga=GA1.1.%lld.%d; _ga_DFGB8WF1WG=GS2.1.s%lld$o1$g0$t%lld$j60$l0$h0",
        now, rnd, now, now);
}

#define MM_H_STATIC \
    static char h_vid[512]; \
    static char h_ck_hdr[8192]; \
    static char h_cookie_line[2048]; \
    static const char *arr[20]

static const char **mm_headers_address(char *cookie_buf, size_t cookie_cap) {
    mm_cookie(cookie_buf, cookie_cap);
    MM_H_STATIC;
    snprintf(h_cookie_line, sizeof(h_cookie_line), "Cookie: %s", cookie_buf);
    int i = 0;
    arr[i++] = "Accept: */*";
    arr[i++] = "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6";
    arr[i++] = "Origin: https://minmail.app";
    arr[i++] = "Referer: https://minmail.app/cn";
    arr[i++] = "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";
    arr[i++] = "cache-control: no-cache";
    arr[i++] = "dnt: 1";
    arr[i++] = "pragma: no-cache";
    arr[i++] = "priority: u=1, i";
    arr[i++] = "sec-ch-ua: \"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"146\"";
    arr[i++] = "sec-ch-ua-mobile: ?0";
    arr[i++] = "sec-ch-ua-platform: \"Windows\"";
    arr[i++] = "sec-fetch-dest: empty";
    arr[i++] = "sec-fetch-mode: cors";
    arr[i++] = "sec-fetch-site: same-origin";
    arr[i++] = h_cookie_line;
    arr[i++] = NULL;
    return arr;
}

static const char **mm_headers_list(const char *visitor_id, const char *ck_opt, char *cookie_buf, size_t cookie_cap) {
    mm_cookie(cookie_buf, cookie_cap);
    MM_H_STATIC;
    snprintf(h_vid, sizeof(h_vid), "visitor-id: %s", visitor_id);
    snprintf(h_cookie_line, sizeof(h_cookie_line), "Cookie: %s", cookie_buf);
    int i = 0;
    arr[i++] = "Accept: */*";
    arr[i++] = "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6";
    arr[i++] = "Origin: https://minmail.app";
    arr[i++] = "Referer: https://minmail.app/cn";
    arr[i++] = "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";
    arr[i++] = "cache-control: no-cache";
    arr[i++] = "dnt: 1";
    arr[i++] = "pragma: no-cache";
    arr[i++] = "priority: u=1, i";
    arr[i++] = "sec-ch-ua: \"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"146\"";
    arr[i++] = "sec-ch-ua-mobile: ?0";
    arr[i++] = "sec-ch-ua-platform: \"Windows\"";
    arr[i++] = "sec-fetch-dest: empty";
    arr[i++] = "sec-fetch-mode: cors";
    arr[i++] = "sec-fetch-site: same-origin";
    arr[i++] = h_vid;
    if (ck_opt && ck_opt[0]) {
        snprintf(h_ck_hdr, sizeof(h_ck_hdr), "ck: %s", ck_opt);
        arr[i++] = h_ck_hdr;
    }
    arr[i++] = h_cookie_line;
    arr[i++] = NULL;
    return arr;
}

static void mm_parse_token(const char *token, char *vid, size_t vid_cap, char *ck, size_t ck_cap) {
    vid[0] = '\0';
    ck[0] = '\0';
    if (!token || !token[0]) return;
    if (token[0] == '{') {
        cJSON *j = cJSON_Parse(token);
        if (j) {
            cJSON *v = cJSON_GetObjectItemCaseSensitive(j, "visitorId");
            if (!cJSON_IsString(v)) v = cJSON_GetObjectItemCaseSensitive(j, "visitor_id");
            const char *vs = cJSON_GetStringValue(v);
            if (vs && vs[0]) {
                strncpy(vid, vs, vid_cap - 1);
                vid[vid_cap - 1] = '\0';
            }
            const char *cs = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(j, "ck"));
            if (cs && cs[0]) {
                strncpy(ck, cs, ck_cap - 1);
                ck[ck_cap - 1] = '\0';
            }
            cJSON_Delete(j);
            return;
        }
    }
    strncpy(vid, token, vid_cap - 1);
    vid[vid_cap - 1] = '\0';
}

tm_email_info_t* tm_provider_minmail_generate(void) {
    srand((unsigned)time(NULL));
    char cookie[2048];
    const char **hdrs = mm_headers_address(cookie, sizeof(cookie));

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET,
        MM_BASE "/mail/address?refresh=true&expire=1440&part=main", hdrs, NULL, 15);
    if (!resp || resp->status < 200 || resp->status >= 300) { tm_http_response_free(resp); return NULL; }

    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) return NULL;
    const char *addr = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(json, "address"));
    if (!addr || !addr[0]) { cJSON_Delete(json); return NULL; }

    char vid[80];
    cJSON *vj = cJSON_GetObjectItemCaseSensitive(json, "visitorId");
    if (!cJSON_IsString(vj)) vj = cJSON_GetObjectItemCaseSensitive(json, "visitor_id");
    const char *vjs = cJSON_GetStringValue(vj);
    if (vjs && vjs[0]) {
        strncpy(vid, vjs, sizeof(vid) - 1);
        vid[sizeof(vid) - 1] = '\0';
    } else {
        mm_visitor_id(vid, sizeof(vid));
    }
    const char *cks = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(json, "ck"));
    if (!cks) cks = "";

    cJSON *tok = cJSON_CreateObject();
    cJSON_AddStringToObject(tok, "visitorId", vid);
    cJSON_AddStringToObject(tok, "ck", cks);
    char *tok_str = cJSON_PrintUnformatted(tok);
    cJSON_Delete(tok);

    cJSON *ex = cJSON_GetObjectItemCaseSensitive(json, "expire");
    long long expires_ms = 0;
    if (cJSON_IsNumber(ex)) {
        long long now = (long long)time(NULL) * 1000LL;
        expires_ms = now + (long long)ex->valuedouble * 60LL * 1000LL;
    }

    tm_email_info_t *info = tm_email_info_new();
    info->channel = CHANNEL_MINMAIL;
    info->email = tm_strdup(addr);
    info->token = tok_str ? tm_strdup(tok_str) : tm_strdup("");
    if (tok_str) cJSON_free(tok_str);
    if (expires_ms > 0) info->expires_at = expires_ms;
    cJSON_Delete(json);
    return info;
}

tm_email_t* tm_provider_minmail_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    if (!email) return NULL;

    char vid[80];
    char ckbuf[8192];
    mm_parse_token(token ? token : "", vid, sizeof(vid), ckbuf, sizeof(ckbuf));
    if (!vid[0]) mm_visitor_id(vid, sizeof(vid));

    char cookie[2048];
    const char **hdrs = mm_headers_list(vid, ckbuf[0] ? ckbuf : NULL, cookie, sizeof(cookie));

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, MM_BASE "/mail/list?part=main", hdrs, NULL, 15);
    if (!resp || resp->status < 200 || resp->status >= 300) { tm_http_response_free(resp); return NULL; }

    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) return NULL;
    cJSON *messages = cJSON_GetObjectItemCaseSensitive(json, "message");
    if (!cJSON_IsArray(messages)) { cJSON_Delete(json); *count = 0; return NULL; }

    int n = cJSON_GetArraySize(messages);
    int m = 0;
    for (int i = 0; i < n; i++) {
        cJSON *msg = cJSON_GetArrayItem(messages, i);
        const char *to = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(msg, "to"));
        if (to && to[0] && strcmp(to, email) != 0) continue;
        m++;
    }
    *count = m;
    if (m == 0) { cJSON_Delete(json); return NULL; }

    tm_email_t *emails = tm_emails_new(m);
    int j = 0;
    for (int i = 0; i < n; i++) {
        cJSON *raw = cJSON_GetArrayItem(messages, i);
        const char *to = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(raw, "to"));
        if (to && to[0] && strcmp(to, email) != 0) continue;

        cJSON *one = cJSON_CreateObject();
#define MM_S(f) (cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(raw, f)))
        cJSON_AddStringToObject(one, "id", MM_S("id") ? MM_S("id") : "");
        cJSON_AddStringToObject(one, "from", MM_S("from") ? MM_S("from") : "");
        cJSON_AddStringToObject(one, "to", (to && to[0]) ? to : email);
        cJSON_AddStringToObject(one, "subject", MM_S("subject") ? MM_S("subject") : "");
        cJSON_AddStringToObject(one, "text", MM_S("preview") ? MM_S("preview") : "");
        cJSON_AddStringToObject(one, "html", MM_S("content") ? MM_S("content") : "");
        cJSON_AddStringToObject(one, "date", MM_S("date") ? MM_S("date") : "");
        cJSON *ir = cJSON_GetObjectItemCaseSensitive(raw, "isRead");
        if (cJSON_IsBool(ir)) cJSON_AddBoolToObject(one, "isRead", cJSON_IsTrue(ir));
        else cJSON_AddBoolToObject(one, "isRead", 0);
#undef MM_S
        emails[j] = tm_normalize_email(one, email);
        cJSON_Delete(one);
        j++;
    }
    cJSON_Delete(json);
    return emails;
}
