/**
 * disposablemail.com 临时邮箱渠道 — https://www.disposablemail.com（METRONET s.r.o. 后端）
 *
 * 与 minuteinbox.com / fakemail.net 共享相同的 PHP API 结构。
 * 创建邮箱:
 *   1. GET / 获取 session cookie + 从 HTML 提取 CSRF="xxx"
 *   2. GET /index/index?csrf_token={csrf} → {"email":"user@domain.com"}
 * 获取邮件:
 *   1. GET /index/refresh → JSON 数组（空收件箱返回数字 0）
 *      字段（捷克语）: predmet=subject, od=from, id=邮件ID, kdy=when, precteno=read status
 *   2. 对每封邮件 GET /email/id/{id} → HTML 正文
 * 已读判断: precteno == "precteno" 表示已读
 *
 * token 存储策略: "dispmail1:" + base64(json{"c":"cookie_header","csrf":"csrf_token"})
 */
#include "tempmail_internal.h"
#include <stdlib.h>
#include <string.h>

#define DISPM_ORIGIN "https://www.disposablemail.com"
#define DISPM_TOK_PREFIX "dispmail1:"
#define DISPM_MAX_COOKIE 8192
#define DISPM_MAX_MAILS 128

static const char *dispm_ua =
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";

/* ========== Cookie 合并 ========== */

typedef struct {
    char k[128];
    char v[2048];
} dispm_ck_t;

static int dispm_ck_find(dispm_ck_t *tab, int n, const char *k) {
    for (int i = 0; i < n; i++) {
        if (strcmp(tab[i].k, k) == 0) return i;
    }
    return -1;
}

static int dispm_ck_parse_merge(dispm_ck_t *tab, int n, int maxn, const char *hdr) {
    if (!hdr || !*hdr) return n;
    const char *p = hdr;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == ';') p++;
        if (!*p) break;
        const char *semi = strchr(p, ';');
        const char *end = semi ? semi : p + strlen(p);
        const char *eq = memchr(p, '=', (size_t)(end - p));
        if (!eq || eq >= end) {
            p = semi ? semi + 1 : end;
            continue;
        }
        const char *ks = p;
        const char *vs = eq + 1;
        size_t klen = (size_t)(eq - ks);
        while (klen > 0 && (ks[klen - 1] == ' ' || ks[klen - 1] == '\t')) klen--;
        size_t vlen = (size_t)(end - vs);
        while (vlen > 0 && (vs[vlen - 1] == ' ' || vs[vlen - 1] == '\t')) vlen--;
        if (klen == 0 || klen >= sizeof(tab[0].k)) {
            p = semi ? semi + 1 : end;
            continue;
        }
        char key[128];
        memcpy(key, ks, klen);
        key[klen] = '\0';
        if (vlen >= sizeof(tab[0].v)) vlen = sizeof(tab[0].v) - 1;
        int ix = dispm_ck_find(tab, n, key);
        if (ix < 0) {
            if (n >= maxn) return n;
            ix = n++;
            strcpy(tab[ix].k, key);
        }
        memcpy(tab[ix].v, vs, vlen);
        tab[ix].v[vlen] = '\0';
        p = semi ? semi + 1 : end;
    }
    return n;
}

static char *dispm_cookie_join(dispm_ck_t *tab, int n) {
    size_t need = 1;
    for (int i = 0; i < n; i++) {
        need += strlen(tab[i].k) + 1 + strlen(tab[i].v) + 2;
    }
    char *out = (char *)malloc(need);
    if (!out) return NULL;
    char *w = out;
    for (int i = 0; i < n; i++) {
        if (i > 0) {
            *w++ = ';';
            *w++ = ' ';
        }
        w += (size_t)sprintf(w, "%s=%s", tab[i].k, tab[i].v);
    }
    *w = '\0';
    return out;
}

static char *dispm_cookie_merge(const char *a, const char *b) {
    dispm_ck_t tab[128];
    int n = 0;
    n = dispm_ck_parse_merge(tab, n, 128, a);
    n = dispm_ck_parse_merge(tab, n, 128, b);
    return dispm_cookie_join(tab, n);
}

/* ========== Base64 编解码 ========== */

static int dispm_b64_encode(const unsigned char *in, size_t inlen, char *out, size_t outcap) {
    static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t o = 0;
    for (size_t i = 0; i < inlen; i += 3) {
        unsigned n = (unsigned)in[i] << 16;
        if (i + 1 < inlen) n |= (unsigned)in[i + 1] << 8;
        if (i + 2 < inlen) n |= in[i + 2];
        if (o + 4 >= outcap) return -1;
        out[o++] = T[(n >> 18) & 63];
        out[o++] = T[(n >> 12) & 63];
        if (i + 1 < inlen) out[o++] = T[(n >> 6) & 63];
        else out[o++] = '=';
        if (i + 2 < inlen) out[o++] = T[n & 63];
        else out[o++] = '=';
    }
    out[o] = '\0';
    return 0;
}

static int dispm_b64_val(int c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static unsigned char *dispm_b64_decode_alloc(const char *s, size_t *outlen) {
    *outlen = 0;
    if (!s) return NULL;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    size_t sl = strlen(s);
    while (sl > 0 && (s[sl - 1] == ' ' || s[sl - 1] == '\t')) sl--;
    if (sl == 0) return NULL;
    size_t maxo = (sl / 4) * 3 + 8;
    unsigned char *buf = (unsigned char *)malloc(maxo);
    if (!buf) return NULL;
    size_t o = 0;
    for (size_t i = 0; i + 3 < sl; i += 4) {
        int v0 = dispm_b64_val((unsigned char)s[i]);
        int v1 = dispm_b64_val((unsigned char)s[i + 1]);
        if (v0 < 0 || v1 < 0) { free(buf); return NULL; }
        unsigned triple = ((unsigned)v0 << 18) | ((unsigned)v1 << 12);
        if (s[i + 2] != '=') {
            int v2 = dispm_b64_val((unsigned char)s[i + 2]);
            if (v2 < 0) { free(buf); return NULL; }
            triple |= (unsigned)v2 << 6;
            if (s[i + 3] != '=') {
                int v3 = dispm_b64_val((unsigned char)s[i + 3]);
                if (v3 < 0) { free(buf); return NULL; }
                triple |= (unsigned)v3;
                if (o + 3 > maxo) { free(buf); return NULL; }
                buf[o++] = (unsigned char)((triple >> 16) & 255);
                buf[o++] = (unsigned char)((triple >> 8) & 255);
                buf[o++] = (unsigned char)(triple & 255);
            } else {
                if (o + 2 > maxo) { free(buf); return NULL; }
                buf[o++] = (unsigned char)((triple >> 16) & 255);
                buf[o++] = (unsigned char)((triple >> 8) & 255);
            }
        } else {
            if (o + 1 > maxo) { free(buf); return NULL; }
            buf[o++] = (unsigned char)((triple >> 16) & 255);
        }
    }
    buf[o] = '\0';
    *outlen = o;
    return buf;
}

/* ========== Token 构建与解析 ========== */

static char *dispm_build_token(const char *cookie_hdr, const char *csrf) {
    cJSON *o = cJSON_CreateObject();
    if (!o) return NULL;
    cJSON_AddStringToObject(o, "c", cookie_hdr ? cookie_hdr : "");
    cJSON_AddStringToObject(o, "csrf", csrf ? csrf : "");
    char *json = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    if (!json) return NULL;
    size_t jl = strlen(json);
    size_t bcap = 4 * ((jl + 2) / 3) + 8 + strlen(DISPM_TOK_PREFIX);
    char *tok = (char *)malloc(bcap);
    if (!tok) { free(json); return NULL; }
    strcpy(tok, DISPM_TOK_PREFIX);
    if (dispm_b64_encode((const unsigned char *)json, jl, tok + strlen(DISPM_TOK_PREFIX), bcap - strlen(DISPM_TOK_PREFIX)) != 0) {
        free(json);
        free(tok);
        return NULL;
    }
    free(json);
    return tok;
}

static int dispm_parse_token(const char *tok, char **cookie_hdr, char **csrf) {
    *cookie_hdr = NULL;
    *csrf = NULL;
    if (!tok || strncmp(tok, DISPM_TOK_PREFIX, strlen(DISPM_TOK_PREFIX)) != 0) return -1;
    const char *b64 = tok + strlen(DISPM_TOK_PREFIX);
    size_t rawlen = 0;
    unsigned char *raw = dispm_b64_decode_alloc(b64, &rawlen);
    if (!raw) return -1;
    raw[rawlen] = '\0';
    cJSON *j = cJSON_Parse((char *)raw);
    free(raw);
    if (!j) return -1;
    const cJSON *jc = cJSON_GetObjectItemCaseSensitive(j, "c");
    const cJSON *jcsrf = cJSON_GetObjectItemCaseSensitive(j, "csrf");
    if (!cJSON_IsString(jc) || !jc->valuestring || !jc->valuestring[0]) {
        cJSON_Delete(j);
        return -1;
    }
    *cookie_hdr = tm_strdup(jc->valuestring);
    *csrf = tm_strdup(cJSON_IsString(jcsrf) && jcsrf->valuestring ? jcsrf->valuestring : "");
    cJSON_Delete(j);
    return 0;
}

/* ========== CSRF 提取 ========== */

/* 从 HTML 提取 CSRF="xxx" */
static char *dispm_extract_csrf(const char *html) {
    if (!html) return NULL;
    const char *needles[] = { "CSRF=\"", "csrf=\"", "CSRF = \"" };
    const char *p = NULL;
    const char *used = NULL;
    for (int i = 0; i < 3; i++) {
        p = strstr(html, needles[i]);
        if (p) { used = needles[i]; break; }
    }
    if (!p || !used) return NULL;
    p += strlen(used);
    const char *end = strchr(p, '"');
    if (!end || end == p) return NULL;
    size_t len = (size_t)(end - p);
    char *csrf = (char *)malloc(len + 1);
    if (!csrf) return NULL;
    memcpy(csrf, p, len);
    csrf[len] = '\0';
    return csrf;
}

/* ========== 创建邮箱 ========== */

tm_email_info_t *tm_provider_disposablemail_com_generate(void) {
    int timeout = tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

    /* 1. GET / 获取 session cookie 和 CSRF */
    const char *hdr_init[] = {
        dispm_ua,
        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
        "Accept-Language: en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
        "Upgrade-Insecure-Requests: 1",
        NULL
    };
    tm_http_response_t *r1 = tm_http_request(TM_HTTP_GET, DISPM_ORIGIN "/", hdr_init, NULL, timeout);
    if (!r1 || r1->status != 200 || !r1->body) {
        tm_http_response_free(r1);
        return NULL;
    }
    char *csrf = dispm_extract_csrf(r1->body);
    char *ck = tm_strdup(r1->cookies ? r1->cookies : "");
    tm_http_response_free(r1);
    if (!csrf || !csrf[0]) {
        free(csrf);
        free(ck);
        return NULL;
    }
    if (!ck) ck = tm_strdup("");

    /* 2. GET /index/index?csrf_token={csrf} 创建邮箱 */
    char create_url[512];
    snprintf(create_url, sizeof(create_url), DISPM_ORIGIN "/index/index?csrf_token=%s", csrf);

    char h_ck[DISPM_MAX_COOKIE];
    snprintf(h_ck, sizeof(h_ck), "Cookie: %s", ck);

    const char *hdr_create[] = {
        dispm_ua,
        "Accept: application/json, text/javascript, */*; q=0.01",
        "Accept-Language: en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
        "X-Requested-With: XMLHttpRequest",
        "Referer: " DISPM_ORIGIN "/",
        h_ck,
        NULL
    };

    tm_http_response_t *r2 = tm_http_request(TM_HTTP_GET, create_url, hdr_create, NULL, timeout);
    if (!r2 || r2->status != 200 || !r2->body) {
        tm_http_response_free(r2);
        free(ck);
        free(csrf);
        return NULL;
    }

    /* 合并可能的新 Cookie */
    if (r2->cookies && r2->cookies[0]) {
        char *merged = dispm_cookie_merge(ck, r2->cookies);
        if (merged) { free(ck); ck = merged; }
    }

    cJSON *json = cJSON_Parse(r2->body);
    tm_http_response_free(r2);
    if (!json) {
        free(ck);
        free(csrf);
        return NULL;
    }

    const char *email = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(json, "email"));
    if (!email || !strchr(email, '@')) {
        cJSON_Delete(json);
        free(ck);
        free(csrf);
        return NULL;
    }

    char *tok = dispm_build_token(ck, csrf);
    char *email_dup = tm_strdup(email);
    cJSON_Delete(json);
    free(ck);
    free(csrf);

    if (!tok || !email_dup) {
        free(tok);
        free(email_dup);
        return NULL;
    }

    tm_email_info_t *info = tm_email_info_new();
    if (!info) {
        free(tok);
        free(email_dup);
        return NULL;
    }
    info->channel = CHANNEL_DISPOSABLEMAIL_COM;
    info->email = email_dup;
    info->token = tok;
    return info;
}

/* ========== 获取邮件 ========== */

tm_email_t *tm_provider_disposablemail_com_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    if (!token || !email) return NULL;

    char *ck = NULL;
    char *csrf = NULL;
    if (dispm_parse_token(token, &ck, &csrf) != 0) return NULL;
    free(csrf); /* 列表/详情请求不需要 csrf，仅需 cookie */

    int timeout = tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

    /* GET /index/refresh 获取邮件列表 */
    char h_ck[DISPM_MAX_COOKIE];
    snprintf(h_ck, sizeof(h_ck), "Cookie: %s", ck);

    const char *hdr_refresh[] = {
        dispm_ua,
        "Accept: application/json, text/javascript, */*; q=0.01",
        "Accept-Language: en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
        "X-Requested-With: XMLHttpRequest",
        "Referer: " DISPM_ORIGIN "/",
        h_ck,
        NULL
    };

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, DISPM_ORIGIN "/index/refresh", hdr_refresh, NULL, timeout);
    if (!resp || resp->status != 200 || !resp->body) {
        tm_http_response_free(resp);
        free(ck);
        return NULL;
    }

    /* 空收件箱返回数字 0 或空数组 */
    const char *body = resp->body;
    while (*body == ' ' || *body == '\t' || *body == '\n' || *body == '\r') body++;
    if (strcmp(body, "0") == 0 || body[0] == '\0' || strcmp(body, "[]") == 0 || strcmp(body, "false") == 0) {
        tm_http_response_free(resp);
        free(ck);
        *count = 0;
        return NULL;
    }

    cJSON *list = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!list) {
        free(ck);
        return NULL;
    }
    if (!cJSON_IsArray(list)) {
        cJSON_Delete(list);
        free(ck);
        *count = 0;
        return NULL;
    }

    int n = cJSON_GetArraySize(list);
    if (n == 0) {
        cJSON_Delete(list);
        free(ck);
        *count = 0;
        return NULL;
    }
    if (n > DISPM_MAX_MAILS) n = DISPM_MAX_MAILS;

    tm_email_t *emails = tm_emails_new(n);
    if (!emails) {
        cJSON_Delete(list);
        free(ck);
        return NULL;
    }

    int valid = 0;
    for (int i = 0; i < n; i++) {
        const cJSON *item = cJSON_GetArrayItem(list, i);
        if (!cJSON_IsObject(item)) continue;

        const cJSON *j_id = cJSON_GetObjectItemCaseSensitive(item, "id");
        char id_str[64] = {0};
        if (cJSON_IsNumber(j_id)) {
            snprintf(id_str, sizeof(id_str), "%lld", (long long)j_id->valuedouble);
        } else if (cJSON_IsString(j_id) && j_id->valuestring) {
            snprintf(id_str, sizeof(id_str), "%s", j_id->valuestring);
        } else {
            continue;
        }

        const char *predmet = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(item, "predmet")); /* subject */
        const char *od = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(item, "od"));           /* from */
        const char *kdy = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(item, "kdy"));         /* when */
        const char *precteno = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(item, "precteno")); /* read */

        /* GET /email/id/{id} 获取正文 */
        char detail_url[256];
        snprintf(detail_url, sizeof(detail_url), DISPM_ORIGIN "/email/id/%s", id_str);

        char h_ck_d[DISPM_MAX_COOKIE];
        snprintf(h_ck_d, sizeof(h_ck_d), "Cookie: %s", ck);

        const char *hdr_detail[] = {
            dispm_ua,
            "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
            "Accept-Language: en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
            "Referer: " DISPM_ORIGIN "/",
            h_ck_d,
            NULL
        };

        tm_http_response_t *rd = tm_http_request(TM_HTTP_GET, detail_url, hdr_detail, NULL, timeout);

        cJSON *raw = cJSON_CreateObject();
        if (!raw) {
            tm_http_response_free(rd);
            continue;
        }
        cJSON_AddStringToObject(raw, "id", id_str);
        cJSON_AddStringToObject(raw, "to", email);
        if (predmet) cJSON_AddStringToObject(raw, "subject", predmet);
        if (od) cJSON_AddStringToObject(raw, "from", od);
        if (kdy) cJSON_AddStringToObject(raw, "date", kdy);
        /* precteno == "precteno" 表示已读 */
        if (precteno) {
            bool is_read = (strcmp(precteno, "precteno") == 0);
            cJSON_AddBoolToObject(raw, "is_read", is_read);
        }
        if (rd && rd->status == 200 && rd->body && rd->body[0]) {
            cJSON_AddStringToObject(raw, "html", rd->body);
        }
        tm_http_response_free(rd);

        emails[valid] = tm_normalize_email(raw, email);
        cJSON_Delete(raw);
        valid++;
    }

    cJSON_Delete(list);
    free(ck);

    if (valid == 0) {
        free(emails);
        *count = 0;
        return NULL;
    }
    *count = valid;
    return emails;
}
