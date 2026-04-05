/**
 * tmpmails.com：GET 首页 + user_sign Cookie；收信 Next.js Server Action POST
 */
#include "tempmail_internal.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define TM_ORIGIN "https://tmpmails.com"
#define TM_TOKEN_SEP '\t'
#define TM_SUPPORT "support@tmpmails.com"
#define TM_MAX_UNIQ 96
#define TM_MAX_LOCAL 80

typedef struct {
    char addr[128];
    int cnt;
} tm_tmpmails_uniq_t;

#ifdef _WIN32
#define tm_strncasecmp _strnicmp
#else
#define tm_strncasecmp strncasecmp
#endif

static int tm_is_local(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
        || c == '.' || c == '_' || c == '-';
}

static int tm_is_hex(unsigned char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/* 从合并后的 Cookie 串中取 name（大小写不敏感） */
static int tm_cookie_get(const char *cookies, const char *name, char *out, size_t cap) {
    if (!cookies || !name || !out || cap < 2) return -1;
    size_t nl = strlen(name);
    const char *p = cookies;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (tm_strncasecmp(p, name, nl) == 0 && p[nl] == '=') {
            p += nl + 1;
            const char *semi = strchr(p, ';');
            size_t len = semi ? (size_t)(semi - p) : strlen(p);
            while (len > 0 && (p[len - 1] == ' ' || p[len - 1] == '\t')) len--;
            if (len == 0 || len >= cap) return -1;
            memcpy(out, p, len);
            out[len] = '\0';
            return 0;
        }
        const char *semi = strchr(p, ';');
        if (!semi) break;
        p = semi + 1;
    }
    return -1;
}

static void tm_tmpmails_bump(tm_tmpmails_uniq_t *tab, int *n, const char *addr) {
    int i;
    for (i = 0; i < *n; i++) {
        if (strcmp(tab[i].addr, addr) == 0) {
            tab[i].cnt++;
            return;
        }
    }
    if (*n >= TM_MAX_UNIQ) return;
    size_t al = strlen(addr);
    if (al >= sizeof(tab[*n].addr)) return;
    memcpy(tab[*n].addr, addr, al + 1);
    tab[*n].cnt = 1;
    (*n)++;
}

static void tm_tmpmails_scan_html(const char *html, tm_tmpmails_uniq_t *tab, int *n) {
    *n = 0;
    const char *p = html;
    const char dom[] = "@tmpmails.com";
    const size_t dlen = sizeof(dom) - 1;
    while (*p) {
        const char *at = strchr(p, '@');
        if (!at) break;
        if (tm_strncasecmp(at, dom, dlen) != 0) {
            p = at + 1;
            continue;
        }
        char term = at[dlen];
        if (term != '\0' && term != '"' && term != '\'' && term != '<' && term != ' '
            && term != '/' && term != '&' && term != '\\') {
            p = at + 1;
            continue;
        }
        const char *ls = at;
        while (ls > html && tm_is_local((unsigned char)ls[-1])) ls--;
        size_t loclen = (size_t)(at - ls);
        if (loclen == 0 || loclen > TM_MAX_LOCAL) {
            p = at + 1;
            continue;
        }
        char buf[160];
        memcpy(buf, ls, loclen);
        memcpy(buf + loclen, dom, dlen + 1);
        if (tm_strncasecmp(buf, TM_SUPPORT, (int)strlen(TM_SUPPORT)) == 0 && buf[strlen(TM_SUPPORT)] == '\0') {
            p = at + 1;
            continue;
        }
        tm_tmpmails_bump(tab, n, buf);
        p = at + 1;
    }
}

static int tm_tmpmails_pick_email(tm_tmpmails_uniq_t *tab, int n, char *out, size_t cap) {
    if (n <= 0) return -1;
    int bi = 0;
    for (int i = 1; i < n; i++) {
        if (tab[i].cnt > tab[bi].cnt) bi = i;
        else if (tab[i].cnt == tab[bi].cnt && strcmp(tab[i].addr, tab[bi].addr) < 0) bi = i;
    }
    if (strlen(tab[bi].addr) >= cap) return -1;
    strcpy(out, tab[bi].addr);
    return 0;
}

static char *tm_tmpmails_chunk_path(const char *html) {
    const char *needle = "/_next/static/chunks/app/%5Blocale%5D/page-";
    const char *p = strstr(html, needle);
    if (!p) return NULL;
    const char *dot = strstr(p + strlen(needle), ".js");
    if (!dot) return NULL;
    size_t len = (size_t)(dot - p + 3);
    char *path = (char *)malloc(len + 1);
    if (!path) return NULL;
    memcpy(path, p, len);
    path[len] = '\0';
    return path;
}

static int tm_tmpmails_action_from_js(const char *js, char *out, size_t cap) {
    const char *needle = ",o.callServer,void 0,o.findSourceMapURL,\"getInboxList\"";
    const char *spot = strstr(js, needle);
    if (!spot || spot <= js + 2) return -1;
    const char *close_q = spot - 1;
    if (*close_q != '"') return -1;
    const char *e = close_q - 1;
    while (e > js && tm_is_hex((unsigned char)*e)) e--;
    if (e < js || *e != '"') return -1;
    const char *s = e + 1;
    size_t hlen = (size_t)(close_q - s);
    if (hlen == 0 || hlen >= cap) return -1;
    memcpy(out, s, hlen);
    out[hlen] = '\0';
    return 0;
}

static char *tm_tmpmails_sanitize(const char *body) {
    const char *p = body;
    size_t extra = 0;
    const char *u = "\"$undefined\"";
    size_t ulen = strlen(u);
    while ((p = strstr(p, u)) != NULL) {
        extra++;
        p += ulen;
    }
    size_t bl = strlen(body);
    char *buf = (char *)malloc(bl + extra * 16 + 1);
    if (!buf) return NULL;
    char *w = buf;
    p = body;
    while (*p) {
        if (strncmp(p, u, ulen) == 0) {
            memcpy(w, "null", 4);
            w += 4;
            p += ulen;
        } else {
            *w++ = *p++;
        }
    }
    *w = '\0';
    return buf;
}

static tm_email_t *tm_tmpmails_parse_inbox(const char *body, const char *recipient, int *count) {
    *count = -1;
    char *san = tm_tmpmails_sanitize(body);
    if (!san) return NULL;

    char *cur = san;
    while (*cur) {
        char *line_start = cur;
        while (*cur && *cur != '\r' && *cur != '\n') cur++;
        int had_break = (*cur != '\0');
        if (*cur) {
            *cur = '\0';
            cur++;
        }
        char *line = line_start;
        while (*line == ' ' || *line == '\t') line++;
        size_t linelen = strlen(line);
        while (linelen > 0 && (line[linelen - 1] == '\r' || line[linelen - 1] == ' ' || line[linelen - 1] == '\t')) {
            line[--linelen] = '\0';
        }
        char *colon = strchr(line, ':');
        if (colon && colon[1] != '\0') {
            char *json_part = colon + 1;
            while (*json_part == ' ' || *json_part == '\t') json_part++;
            if (*json_part == '{') {
                cJSON *root = cJSON_Parse(json_part);
                if (root) {
                    const cJSON *code = cJSON_GetObjectItemCaseSensitive(root, "code");
                    if (cJSON_IsNumber(code) && (int)code->valuedouble == 200) {
                        const cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
                        const cJSON *list = data ? cJSON_GetObjectItemCaseSensitive(data, "list") : NULL;
                        if (cJSON_IsArray(list)) {
                            int n = cJSON_GetArraySize(list);
                            *count = n;
                            if (n == 0) {
                                cJSON_Delete(root);
                                free(san);
                                return NULL;
                            }
                            tm_email_t *result = tm_emails_new(n);
                            if (!result) {
                                cJSON_Delete(root);
                                free(san);
                                *count = -1;
                                return NULL;
                            }
                            for (int i = 0; i < n; i++) {
                                result[i] = tm_normalize_email(cJSON_GetArrayItem(list, i), recipient);
                            }
                            cJSON_Delete(root);
                            free(san);
                            return result;
                        }
                    }
                    cJSON_Delete(root);
                }
            }
        }
        if (!had_break) break;
    }
    free(san);
    return NULL;
}

static const char *tm_tmpmails_headers_get[] = {
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
    "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
    "Cache-Control: no-cache",
    "DNT: 1",
    "Pragma: no-cache",
    "Upgrade-Insecure-Requests: 1",
    NULL,
};

tm_email_info_t *tm_provider_tmpmails_generate(const char *domain) {
    const char *loc = (domain && domain[0]) ? domain : "zh";
    char page_url[256];
    if (snprintf(page_url, sizeof(page_url), TM_ORIGIN "/%s", loc) >= (int)sizeof(page_url)) return NULL;

    char referer_hdr[320];
    if (snprintf(referer_hdr, sizeof(referer_hdr), "Referer: %s", page_url) >= (int)sizeof(referer_hdr)) return NULL;

    const char *headers[16];
    int hi = 0;
    for (int i = 0; tm_tmpmails_headers_get[i] && hi < 14; i++) headers[hi++] = tm_tmpmails_headers_get[i];
    headers[hi++] = referer_hdr;
    headers[hi] = NULL;

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, page_url, headers, NULL, 15);
    if (!resp || resp->status != 200 || !resp->body) {
        tm_http_response_free(resp);
        return NULL;
    }

    char user_sign[128];
    if (tm_cookie_get(resp->cookies, "user_sign", user_sign, sizeof(user_sign)) != 0) {
        tm_http_response_free(resp);
        return NULL;
    }

    tm_tmpmails_uniq_t tab[TM_MAX_UNIQ];
    int nu = 0;
    tm_tmpmails_scan_html(resp->body, tab, &nu);
    char email[160];
    if (tm_tmpmails_pick_email(tab, nu, email, sizeof(email)) != 0) {
        tm_http_response_free(resp);
        return NULL;
    }

    char *chunk_rel = tm_tmpmails_chunk_path(resp->body);
    tm_http_response_free(resp);
    if (!chunk_rel) return NULL;

    char chunk_url[512];
    if (snprintf(chunk_url, sizeof(chunk_url), TM_ORIGIN "%s", chunk_rel) >= (int)sizeof(chunk_url)) {
        free(chunk_rel);
        return NULL;
    }
    free(chunk_rel);

    const char *ch_headers[] = {
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
        "Accept: */*",
        "Referer: " TM_ORIGIN "/",
        NULL,
    };
    tm_http_response_t *jsresp = tm_http_request(TM_HTTP_GET, chunk_url, ch_headers, NULL, 15);
    if (!jsresp || jsresp->status != 200 || !jsresp->body) {
        tm_http_response_free(jsresp);
        return NULL;
    }
    char action[96];
    if (tm_tmpmails_action_from_js(jsresp->body, action, sizeof(action)) != 0) {
        tm_http_response_free(jsresp);
        return NULL;
    }
    tm_http_response_free(jsresp);

    size_t tok_len = strlen(loc) + 1 + strlen(user_sign) + 1 + strlen(action) + 1;
    char *token = (char *)malloc(tok_len);
    if (!token) return NULL;
    snprintf(token, tok_len, "%s%c%s%c%s", loc, TM_TOKEN_SEP, user_sign, TM_TOKEN_SEP, action);

    tm_email_info_t *info = tm_email_info_new();
    if (!info) {
        free(token);
        return NULL;
    }
    info->channel = CHANNEL_TMPMAILS;
    info->email = tm_strdup(email);
    info->token = token;
    return info;
}

tm_email_t *tm_provider_tmpmails_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    if (!token || !email) return NULL;

    char loc[32], user_sign[128], action[96];
    const char *t1 = strchr(token, TM_TOKEN_SEP);
    if (!t1) return NULL;
    const char *t2 = strchr(t1 + 1, TM_TOKEN_SEP);
    if (!t2) return NULL;
    size_t l1 = (size_t)(t1 - token);
    size_t l2 = (size_t)(t2 - t1 - 1);
    size_t l3 = strlen(t2 + 1);
    if (l1 >= sizeof(loc) || l2 >= sizeof(user_sign) || l3 >= sizeof(action)) return NULL;
    memcpy(loc, token, l1);
    loc[l1] = '\0';
    memcpy(user_sign, t1 + 1, l2);
    user_sign[l2] = '\0';
    memcpy(action, t2 + 1, l3 + 1);

    char post_url[256];
    if (snprintf(post_url, sizeof(post_url), TM_ORIGIN "/%s", loc) >= (int)sizeof(post_url)) return NULL;

    cJSON *arr = cJSON_CreateArray();
    if (!arr) return NULL;
    cJSON_AddItemToArray(arr, cJSON_CreateString(user_sign));
    cJSON_AddItemToArray(arr, cJSON_CreateString(email));
    cJSON_AddItemToArray(arr, cJSON_CreateNumber(0));
    char *body = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);
    if (!body) return NULL;

    char ck[384];
    if (snprintf(ck, sizeof(ck), "Cookie: NEXT_LOCALE=%s; user_sign=%s", loc, user_sign) >= (int)sizeof(ck)) {
        free(body);
        return NULL;
    }
    char na[160];
    if (snprintf(na, sizeof(na), "Next-Action: %s", action) >= (int)sizeof(na)) {
        free(body);
        return NULL;
    }
    char refh[320];
    if (snprintf(refh, sizeof(refh), "Referer: %s", post_url) >= (int)sizeof(refh)) {
        free(body);
        return NULL;
    }

    const char *headers[] = {
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
        "Accept: text/x-component",
        "Content-Type: text/plain;charset=UTF-8",
        na,
        "Origin: " TM_ORIGIN,
        refh,
        ck,
        NULL,
    };

    tm_http_response_t *resp = tm_http_request(TM_HTTP_POST, post_url, headers, body, 15);
    free(body);
    if (!resp || resp->status != 200 || !resp->body) {
        tm_http_response_free(resp);
        return NULL;
    }

    tm_email_t *emails = tm_tmpmails_parse_inbox(resp->body, email, count);
    tm_http_response_free(resp);
    return emails;
}
