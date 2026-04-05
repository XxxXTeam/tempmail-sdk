/**
 * moakt.com：HTML 收件箱 + tm_session Cookie；详情 GET /{locale}/email/{uuid}/html
 */
#include "tempmail_internal.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MOK_ORIGIN "https://www.moakt.com"
#define MOK_TOK_PREFIX "mok1:"
#define MOK_MAX_COOKIE 8192
#define MOK_MAX_IDS 64
#define MOK_UUID_LEN 36

#ifdef _WIN32
#define tm_strncasecmp _strnicmp
#else
#define tm_strncasecmp strncasecmp
#endif

typedef struct {
    char k[128];
    char v[2048];
} mok_ck_t;

static int mok_ck_find(mok_ck_t *tab, int n, const char *k) {
    for (int i = 0; i < n; i++) {
        if (strcmp(tab[i].k, k) == 0) return i;
    }
    return -1;
}

/* 将 Cookie 串解析并入 tab（同名覆盖），返回新的 n */
static int mok_ck_parse_merge(mok_ck_t *tab, int n, int maxn, const char *hdr) {
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
        int ix = mok_ck_find(tab, n, key);
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

static char *mok_cookie_join(mok_ck_t *tab, int n) {
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

static char *mok_cookie_merge(const char *a, const char *b) {
    mok_ck_t tab[128];
    int n = 0;
    n = mok_ck_parse_merge(tab, n, 128, a);
    n = mok_ck_parse_merge(tab, n, 128, b);
    return mok_cookie_join(tab, n);
}

static int mok_cookie_has_tm_session(const char *hdr) {
    if (!hdr) return 0;
    const char *p = hdr;
    const char name[] = "tm_session";
    size_t nl = sizeof(name) - 1;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (tm_strncasecmp(p, name, (int)nl) == 0 && p[nl] == '=') return 1;
        const char *semi = strchr(p, ';');
        if (!semi) break;
        p = semi + 1;
    }
    return 0;
}

static void mok_locale(const char *domain, char *out, size_t cap) {
    if (!out || cap < 4) return;
    if (!domain || !*domain) {
        strcpy(out, "zh");
        return;
    }
    const char *s = domain;
    while (*s == ' ' || *s == '\t') s++;
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t')) len--;
    if (len == 0 || len >= cap) {
        strcpy(out, "zh");
        return;
    }
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '/' || s[i] == '?' || s[i] == '#' || s[i] == '\\') {
            strcpy(out, "zh");
            return;
        }
    }
    memcpy(out, s, len);
    out[len] = '\0';
}

static int mok_b64_encode(const unsigned char *in, size_t inlen, char *out, size_t outcap) {
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

static int mok_b64_val(int c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static unsigned char *mok_b64_decode_alloc(const char *s, size_t *outlen) {
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
        int v0 = mok_b64_val((unsigned char)s[i]);
        int v1 = mok_b64_val((unsigned char)s[i + 1]);
        if (v0 < 0 || v1 < 0) {
            free(buf);
            return NULL;
        }
        unsigned triple = ((unsigned)v0 << 18) | ((unsigned)v1 << 12);
        if (s[i + 2] != '=') {
            int v2 = mok_b64_val((unsigned char)s[i + 2]);
            if (v2 < 0) {
                free(buf);
                return NULL;
            }
            triple |= (unsigned)v2 << 6;
            if (s[i + 3] != '=') {
                int v3 = mok_b64_val((unsigned char)s[i + 3]);
                if (v3 < 0) {
                    free(buf);
                    return NULL;
                }
                triple |= (unsigned)v3;
                if (o + 3 > maxo) {
                    free(buf);
                    return NULL;
                }
                buf[o++] = (unsigned char)((triple >> 16) & 255);
                buf[o++] = (unsigned char)((triple >> 8) & 255);
                buf[o++] = (unsigned char)(triple & 255);
            } else {
                if (o + 2 > maxo) {
                    free(buf);
                    return NULL;
                }
                buf[o++] = (unsigned char)((triple >> 16) & 255);
                buf[o++] = (unsigned char)((triple >> 8) & 255);
            }
        } else {
            if (o + 1 > maxo) {
                free(buf);
                return NULL;
            }
            buf[o++] = (unsigned char)((triple >> 16) & 255);
        }
    }
    buf[o] = '\0';
    *outlen = o;
    return buf;
}

static char *mok_build_token(const char *locale, const char *cookie_hdr) {
    cJSON *o = cJSON_CreateObject();
    if (!o) return NULL;
    cJSON_AddStringToObject(o, "l", locale);
    cJSON_AddStringToObject(o, "c", cookie_hdr ? cookie_hdr : "");
    char *json = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    if (!json) return NULL;
    size_t jl = strlen(json);
    size_t bcap = 4 * ((jl + 2) / 3) + 8 + strlen(MOK_TOK_PREFIX);
    char *tok = (char *)malloc(bcap);
    if (!tok) {
        free(json);
        return NULL;
    }
    strcpy(tok, MOK_TOK_PREFIX);
    if (mok_b64_encode((const unsigned char *)json, jl, tok + strlen(MOK_TOK_PREFIX), bcap - strlen(MOK_TOK_PREFIX)) != 0) {
        free(json);
        free(tok);
        return NULL;
    }
    free(json);
    return tok;
}

static int mok_parse_token(const char *tok, char **locale, char **cookie_hdr) {
    *locale = NULL;
    *cookie_hdr = NULL;
    if (!tok || strncmp(tok, MOK_TOK_PREFIX, strlen(MOK_TOK_PREFIX)) != 0) return -1;
    const char *b64 = tok + strlen(MOK_TOK_PREFIX);
    size_t rawlen = 0;
    unsigned char *raw = mok_b64_decode_alloc(b64, &rawlen);
    if (!raw) return -1;
    raw[rawlen] = '\0';
    cJSON *j = cJSON_Parse((char *)raw);
    free(raw);
    if (!j) return -1;
    const cJSON *jl = cJSON_GetObjectItemCaseSensitive(j, "l");
    const cJSON *jc = cJSON_GetObjectItemCaseSensitive(j, "c");
    if (!cJSON_IsString(jl) || !jl->valuestring || !cJSON_IsString(jc) || !jc->valuestring) {
        cJSON_Delete(j);
        return -1;
    }
    *locale = tm_strdup(jl->valuestring);
    *cookie_hdr = tm_strdup(jc->valuestring);
    cJSON_Delete(j);
    if (!*locale || !*cookie_hdr || !(*locale)[0] || !(*cookie_hdr)[0]) {
        free(*locale);
        free(*cookie_hdr);
        *locale = *cookie_hdr = NULL;
        return -1;
    }
    return 0;
}

static int mok_parse_inbox_email(const char *html, char *out, size_t cap) {
    const char *id = strstr(html, "id=\"email-address\"");
    if (!id) id = strstr(html, "id='email-address'");
    if (!id) return -1;
    const char *gt = strchr(id, '>');
    if (!gt) return -1;
    gt++;
    const char *lt = strchr(gt, '<');
    if (!lt) return -1;
    size_t n = (size_t)(lt - gt);
    if (n == 0 || n >= cap) return -1;
    memcpy(out, gt, n);
    out[n] = '\0';
    /* trim */
    char *a = out;
    while (*a == ' ' || *a == '\t') a++;
    char *b = out + strlen(out);
    while (b > a && (b[-1] == ' ' || b[-1] == '\t')) b--;
    *b = '\0';
    if (a != out) memmove(out, a, (size_t)(b - a) + 1);
    return 0;
}

static int mok_is_uuid(const char *s) {
    if (!s || strlen(s) != MOK_UUID_LEN) return 0;
    for (int i = 0; i < MOK_UUID_LEN; i++) {
        char c = s[i];
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (c != '-') return 0;
        } else {
            if (!isxdigit((unsigned char)c)) return 0;
        }
    }
    return 1;
}

static void mok_collect_ids(const char *html, char ids[][40], int *nids, int maxids) {
    *nids = 0;
    const char *needle = "/email/";
    const char *p = html;
    while (*p && *nids < maxids) {
        const char *h = strstr(p, "href=\"");
        if (!h) break;
        h += 6;
        const char *slash = strstr(h, needle);
        if (!slash || slash > h + 256) {
            p = h;
            continue;
        }
        const char *start = slash + strlen(needle);
        if (strncmp(start, "delete", 6) == 0) {
            p = start + 6;
            continue;
        }
        char uid[40];
        memcpy(uid, start, MOK_UUID_LEN);
        uid[MOK_UUID_LEN] = '\0';
        if (!mok_is_uuid(uid) || (start[MOK_UUID_LEN] != '"' && start[MOK_UUID_LEN] != '\'')) {
            p = h;
            continue;
        }
        int dup = 0;
        for (int i = 0; i < *nids; i++) {
            if (strcmp(ids[i], uid) == 0) {
                dup = 1;
                break;
            }
        }
        if (!dup) strcpy(ids[(*nids)++], uid);
        p = start + MOK_UUID_LEN;
    }
}

static char *mok_extract_tag_content(const char *html, const char *class_attr, const char *end_tag) {
    char pat[96];
    snprintf(pat, sizeof(pat), "class=\"%s\"", class_attr);
    const char *p = strstr(html, pat);
    if (!p) {
        snprintf(pat, sizeof(pat), "class='%s'", class_attr);
        p = strstr(html, pat);
    }
    if (!p) return tm_strdup("");
    p = strchr(p, '>');
    if (!p) return tm_strdup("");
    p++;
    const char *end = strstr(p, end_tag);
    if (!end) return tm_strdup("");
    size_t n = (size_t)(end - p);
    char *s = (char *)malloc(n + 1);
    if (!s) return tm_strdup("");
    memcpy(s, p, n);
    s[n] = '\0';
    return s;
}

static void mok_strip_tags_copy(const char *in, char *out, size_t cap) {
    size_t o = 0;
    int in_tag = 0;
    for (const char *p = in; *p && o + 1 < cap; p++) {
        if (*p == '<') in_tag = 1;
        else if (*p == '>') in_tag = 0;
        else if (!in_tag) {
            if (*p == '\n' || *p == '\r') out[o++] = ' ';
            else out[o++] = *p;
        }
    }
    while (o > 0 && out[o - 1] == ' ') o--;
    out[o] = '\0';
}

static char *mok_extract_email_angle(const char *html) {
    const char *at = strchr(html, '@');
    if (!at) return NULL;
    const char *l = at;
    while (l > html && l[-1] != '<') l--;
    if (l <= html || l[-1] != '<') return NULL;
    l++;
    const char *r = strchr(at, '>');
    if (!r) return NULL;
    size_t n = (size_t)(r - l);
    if (n == 0 || n > 254) return NULL;
    char *e = (char *)malloc(n + 1);
    if (!e) return NULL;
    memcpy(e, l, n);
    e[n] = '\0';
    return e;
}

static cJSON *mok_parse_detail_json(const char *page, const char *id, const char *recipient) {
    cJSON *raw = cJSON_CreateObject();
    if (!raw) return NULL;
    cJSON_AddStringToObject(raw, "id", id);
    cJSON_AddStringToObject(raw, "to", recipient);

    char *title = mok_extract_tag_content(page, "title", "</li>");
    if (title) {
        char tr[512];
        mok_strip_tags_copy(title, tr, sizeof(tr));
        cJSON_AddStringToObject(raw, "subject", tr);
        free(title);
    } else cJSON_AddStringToObject(raw, "subject", "");

    char *dateblk = NULL;
    const char *dp = strstr(page, "class=\"date\"");
    if (!dp) dp = strstr(page, "class='date'");
    if (dp) {
        dp = strstr(dp, "<span");
        if (dp) {
            dp = strchr(dp, '>');
            if (dp) {
                dp++;
                const char *dend = strstr(dp, "</span>");
                if (dend) {
                    size_t dn = (size_t)(dend - dp);
                    dateblk = (char *)malloc(dn + 1);
                    if (dateblk) {
                        memcpy(dateblk, dp, dn);
                        dateblk[dn] = '\0';
                    }
                }
            }
        }
    }
    if (dateblk) {
        char dr[128];
        mok_strip_tags_copy(dateblk, dr, sizeof(dr));
        cJSON_AddStringToObject(raw, "date", dr);
        free(dateblk);
    } else cJSON_AddStringToObject(raw, "date", "");

    char *from_s = tm_strdup("");
    const char *sp = strstr(page, "class=\"sender\"");
    if (!sp) sp = strstr(page, "class='sender'");
    if (sp) {
        sp = strstr(sp, "<span");
        if (sp) {
            sp = strchr(sp, '>');
            if (sp) {
                sp++;
                const char *send = strstr(sp, "</span>");
                if (send) {
                    size_t sn = (size_t)(send - sp);
                    char *sinner = (char *)malloc(sn + 1);
                    if (sinner) {
                        memcpy(sinner, sp, sn);
                        sinner[sn] = '\0';
                        char fr[512];
                        mok_strip_tags_copy(sinner, fr, sizeof(fr));
                        free(from_s);
                        from_s = tm_strdup(fr);
                        char *ang = mok_extract_email_angle(sinner);
                        if (ang) {
                            free(from_s);
                            from_s = ang;
                        }
                        free(sinner);
                    }
                }
            }
        }
    }
    cJSON_AddStringToObject(raw, "from", from_s ? from_s : "");
    free(from_s);

    char *html_body = tm_strdup("");
    const char *eb = strstr(page, "class=\"email-body\"");
    if (!eb) eb = strstr(page, "class='email-body'");
    if (eb) {
        eb = strchr(eb, '>');
        if (eb) {
            eb++;
            const char *ebd = strstr(eb, "</div>");
            if (ebd) {
                size_t hn = (size_t)(ebd - eb);
                free(html_body);
                html_body = (char *)malloc(hn + 1);
                if (html_body) {
                    memcpy(html_body, eb, hn);
                    html_body[hn] = '\0';
                } else html_body = tm_strdup("");
            }
        }
    }
    cJSON_AddStringToObject(raw, "html", html_body ? html_body : "");
    free(html_body);

    return raw;
}

tm_email_info_t *tm_provider_moakt_generate(const char *domain) {
    char loc[32];
    mok_locale(domain, loc, sizeof(loc));
    char base[256];
    snprintf(base, sizeof(base), "%s/%s", MOK_ORIGIN, loc);
    char inbox[288];
    snprintf(inbox, sizeof(inbox), "%s/inbox", base);

    char h_ref1[320], h_ref2[320], h_ck[MOK_MAX_COOKIE];
    snprintf(h_ref1, sizeof(h_ref1), "Referer: %s", base);
    snprintf(h_ref2, sizeof(h_ref2), "Referer: %s", base);

    const char *hdr_home[] = {
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
        "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
        "Cache-Control: no-cache",
        "DNT: 1",
        "Pragma: no-cache",
        h_ref1,
        "Upgrade-Insecure-Requests: 1",
        NULL};

    int timeout = tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;
    tm_http_response_t *r1 = tm_http_request(TM_HTTP_GET, base, hdr_home, NULL, timeout);
    if (!r1 || r1->status != 200) {
        tm_http_response_free(r1);
        return NULL;
    }
    char *ck = tm_strdup(r1->cookies ? r1->cookies : "");
    tm_http_response_free(r1);
    if (!ck) return NULL;

    if (strlen(ck) + 16 >= sizeof(h_ck)) {
        free(ck);
        return NULL;
    }
    snprintf(h_ck, sizeof(h_ck), "Cookie: %s", ck);

    const char *hdr_inbox[] = {
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
        "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
        "Cache-Control: no-cache",
        "DNT: 1",
        "Pragma: no-cache",
        h_ref2,
        "Upgrade-Insecure-Requests: 1",
        h_ck,
        NULL};

    tm_http_response_t *r2 = tm_http_request(TM_HTTP_GET, inbox, hdr_inbox, NULL, timeout);
    if (!r2 || r2->status != 200 || !r2->body) {
        tm_http_response_free(r2);
        free(ck);
        return NULL;
    }
    char *ck2 = mok_cookie_merge(ck, r2->cookies ? r2->cookies : "");
    free(ck);
    if (!ck2) {
        tm_http_response_free(r2);
        return NULL;
    }

    char email_buf[256];
    if (mok_parse_inbox_email(r2->body, email_buf, sizeof(email_buf)) != 0) {
        tm_http_response_free(r2);
        free(ck2);
        return NULL;
    }
    tm_http_response_free(r2);

    if (!mok_cookie_has_tm_session(ck2)) {
        free(ck2);
        return NULL;
    }

    char *tok = mok_build_token(loc, ck2);
    free(ck2);
    if (!tok) return NULL;

    tm_email_info_t *info = tm_email_info_new();
    if (!info) {
        free(tok);
        return NULL;
    }
    info->channel = CHANNEL_MOAKT;
    info->email = tm_strdup(email_buf);
    info->token = tok;
    if (!info->email) {
        tm_free_email_info(info);
        return NULL;
    }
    return info;
}

tm_email_t *tm_provider_moakt_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    if (!token || !email) return NULL;

    char *loc_heap = NULL;
    char *ck = NULL;
    if (mok_parse_token(token, &loc_heap, &ck) != 0) return NULL;

    char loc_buf[64];
    if (strlen(loc_heap) >= sizeof(loc_buf)) {
        free(loc_heap);
        free(ck);
        return NULL;
    }
    strcpy(loc_buf, loc_heap);
    free(loc_heap);

    char base[256];
    snprintf(base, sizeof(base), "%s/%s", MOK_ORIGIN, loc_buf);
    char inbox[288];
    snprintf(inbox, sizeof(inbox), "%s/inbox", base);

    char h_ref[320], h_ck[MOK_MAX_COOKIE];
    snprintf(h_ref, sizeof(h_ref), "Referer: %s", base);
    if (strlen(ck) + 16 >= sizeof(h_ck)) {
        free(ck);
        return NULL;
    }
    snprintf(h_ck, sizeof(h_ck), "Cookie: %s", ck);

    const char *hdr_inbox[] = {
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
        "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
        "Cache-Control: no-cache",
        "DNT: 1",
        "Pragma: no-cache",
        h_ref,
        "Upgrade-Insecure-Requests: 1",
        h_ck,
        NULL};

    int timeout = tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;
    tm_http_response_t *r = tm_http_request(TM_HTTP_GET, inbox, hdr_inbox, NULL, timeout);
    if (!r || r->status != 200 || !r->body) {
        tm_http_response_free(r);
        free(ck);
        return NULL;
    }

    char ids[MOK_MAX_IDS][40];
    int nids = 0;
    mok_collect_ids(r->body, ids, &nids, MOK_MAX_IDS);
    tm_http_response_free(r);

    if (nids == 0) {
        free(ck);
        *count = 0;
        return NULL;
    }

    tm_email_t *arr = tm_emails_new(nids);
    if (!arr) {
        free(ck);
        return NULL;
    }

    for (int i = 0; i < nids; i++) {
        char url[384];
        char refer[384];
        char h_ref_d[400];
        char h_ck_d[MOK_MAX_COOKIE];
        snprintf(url, sizeof(url), "%s/%s/email/%s/html", MOK_ORIGIN, loc_buf, ids[i]);
        snprintf(refer, sizeof(refer), "%s/%s/email/%s", MOK_ORIGIN, loc_buf, ids[i]);
        snprintf(h_ref_d, sizeof(h_ref_d), "Referer: %s", refer);
        snprintf(h_ck_d, sizeof(h_ck_d), "Cookie: %s", ck);

        const char *hdr_d[] = {
            "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
            "Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
            "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
            "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
            "Cache-Control: no-cache",
            "DNT: 1",
            "Pragma: no-cache",
            h_ref_d,
            "Upgrade-Insecure-Requests: 1",
            h_ck_d,
            NULL};

        tm_http_response_t *rd = tm_http_request(TM_HTTP_GET, url, hdr_d, NULL, timeout);
        cJSON *raw = NULL;
        if (rd && rd->status == 200 && rd->body) {
            raw = mok_parse_detail_json(rd->body, ids[i], email);
        }
        tm_http_response_free(rd);
        if (!raw) {
            raw = cJSON_CreateObject();
            if (raw) {
                cJSON_AddStringToObject(raw, "id", ids[i]);
                cJSON_AddStringToObject(raw, "to", email);
            }
        }
        if (raw) {
            arr[i] = tm_normalize_email(raw, email);
            cJSON_Delete(raw);
        } else {
            memset(&arr[i], 0, sizeof(arr[i]));
        }
    }

    free(ck);
    *count = nids;
    return arr;
}
