/**
 * minuteinbox.com 渠道
 *
 * Cookie Session + CSRF + REST JSON
 * 域名: @minafter.com（由 API 返回，可能变化）
 *
 * 创建邮箱流程:
 *   1. GET https://www.minuteinbox.com/ -> 获取 Cookie 并从 HTML 中提取 CSRF 值
 *   2. GET https://www.minuteinbox.com/index/index?csrf_token={CSRF} -> 返回 {"email":"user@minafter.com"}
 *
 * 获取邮件流程:
 *   1. GET https://www.minuteinbox.com/index/refresh -> JSON 数组或数字 0
 *   2. GET https://www.minuteinbox.com/email/id/{id} -> HTML 邮件正文
 *
 * token 存储策略:
 *   "minuteinbox1:" + base64(json{"c":"cookie_header","csrf":"csrf_token"})
 */
#include "tempmail_internal.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MINBOX_ORIGIN "https://www.minuteinbox.com"
#define MINBOX_TOK_PREFIX "minuteinbox1:"
#define MINBOX_MAX_COOKIE 8192
#define MINBOX_MAX_MAILS 128

#ifdef _WIN32
#define minbox_strncasecmp _strnicmp
#else
#define minbox_strncasecmp strncasecmp
#endif

/* ========== Cookie 工具 ========== */

typedef struct {
    char k[128];
    char v[2048];
} minbox_ck_t;

static int minbox_ck_find(minbox_ck_t *tab, int n, const char *k) {
    for (int i = 0; i < n; i++) {
        if (strcmp(tab[i].k, k) == 0) return i;
    }
    return -1;
}

/**
 * 将 Cookie 串解析并入 tab（同名覆盖），返回新的 n
 */
static int minbox_ck_parse_merge(minbox_ck_t *tab, int n, int maxn, const char *hdr) {
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
        int ix = minbox_ck_find(tab, n, key);
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

static char *minbox_cookie_join(minbox_ck_t *tab, int n) {
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

/**
 * 合并两个 Cookie 头字符串，同名覆盖
 */
static char *minbox_cookie_merge(const char *a, const char *b) {
    minbox_ck_t tab[128];
    int n = 0;
    n = minbox_ck_parse_merge(tab, n, 128, a);
    n = minbox_ck_parse_merge(tab, n, 128, b);
    return minbox_cookie_join(tab, n);
}

/* ========== Base64 编解码 ========== */

static int minbox_b64_encode(const unsigned char *in, size_t inlen, char *out, size_t outcap) {
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

static int minbox_b64_val(int c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static unsigned char *minbox_b64_decode_alloc(const char *s, size_t *outlen) {
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
        int v0 = minbox_b64_val((unsigned char)s[i]);
        int v1 = minbox_b64_val((unsigned char)s[i + 1]);
        if (v0 < 0 || v1 < 0) { free(buf); return NULL; }
        unsigned triple = ((unsigned)v0 << 18) | ((unsigned)v1 << 12);
        if (s[i + 2] != '=') {
            int v2 = minbox_b64_val((unsigned char)s[i + 2]);
            if (v2 < 0) { free(buf); return NULL; }
            triple |= (unsigned)v2 << 6;
            if (s[i + 3] != '=') {
                int v3 = minbox_b64_val((unsigned char)s[i + 3]);
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

/**
 * token 格式: minuteinbox1:<base64(json)>
 * json 内含 cookie 头和 csrf 值
 */
static char *minbox_build_token(const char *cookie_hdr, const char *csrf) {
    cJSON *o = cJSON_CreateObject();
    if (!o) return NULL;
    cJSON_AddStringToObject(o, "c", cookie_hdr ? cookie_hdr : "");
    cJSON_AddStringToObject(o, "csrf", csrf ? csrf : "");
    char *json = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    if (!json) return NULL;
    size_t jl = strlen(json);
    size_t bcap = 4 * ((jl + 2) / 3) + 8 + strlen(MINBOX_TOK_PREFIX);
    char *tok = (char *)malloc(bcap);
    if (!tok) { free(json); return NULL; }
    strcpy(tok, MINBOX_TOK_PREFIX);
    if (minbox_b64_encode((const unsigned char *)json, jl, tok + strlen(MINBOX_TOK_PREFIX), bcap - strlen(MINBOX_TOK_PREFIX)) != 0) {
        free(json);
        free(tok);
        return NULL;
    }
    free(json);
    return tok;
}

/**
 * 从 token 中解析出 cookie 头和 csrf 值
 */
static int minbox_parse_token(const char *tok, char **cookie_hdr, char **csrf) {
    *cookie_hdr = NULL;
    *csrf = NULL;
    if (!tok || strncmp(tok, MINBOX_TOK_PREFIX, strlen(MINBOX_TOK_PREFIX)) != 0) return -1;
    const char *b64 = tok + strlen(MINBOX_TOK_PREFIX);
    size_t rawlen = 0;
    unsigned char *raw = minbox_b64_decode_alloc(b64, &rawlen);
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
    if (!*cookie_hdr || !(*cookie_hdr)[0]) {
        free(*cookie_hdr);
        free(*csrf);
        *cookie_hdr = NULL;
        *csrf = NULL;
        return -1;
    }
    return 0;
}

/* ========== CSRF 提取 ========== */

/**
 * 从 HTML 页面中正则提取 CSRF="xxx" 值
 * 匹配模式: CSRF="..."
 */
static char *minbox_extract_csrf(const char *html) {
    if (!html) return NULL;
    const char *needle = "CSRF=\"";
    const char *p = strstr(html, needle);
    if (!p) {
        /* 备用：尝试小写 csrf=" */
        needle = "csrf=\"";
        p = strstr(html, needle);
    }
    if (!p) {
        /* 再尝试 CSRF = " */
        needle = "CSRF = \"";
        p = strstr(html, needle);
    }
    if (!p) return NULL;
    p += strlen(needle);
    const char *end = strchr(p, '"');
    if (!end || end == p) return NULL;
    size_t len = (size_t)(end - p);
    char *csrf = (char *)malloc(len + 1);
    if (!csrf) return NULL;
    memcpy(csrf, p, len);
    csrf[len] = '\0';
    return csrf;
}

/* ========== HTML 工具 ========== */

/**
 * 去除 HTML 标签，保留纯文本
 */
static void minbox_strip_tags(const char *in, char *out, size_t cap) {
    size_t o = 0;
    int in_tag = 0;
    for (const char *p = in; *p && o + 1 < cap; p++) {
        if (*p == '<') { in_tag = 1; continue; }
        if (*p == '>') { in_tag = 0; continue; }
        if (!in_tag) out[o++] = *p;
    }
    while (o > 0 && (out[o - 1] == ' ' || out[o - 1] == '\n' || out[o - 1] == '\r')) o--;
    out[o] = '\0';
}

/* ========== 公共请求头 ========== */

static const char *minbox_ua =
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36";

/* ========== 创建邮箱 ========== */

/**
 * 创建 minuteinbox.com 临时邮箱
 *
 * 流程:
 *   1. GET / -> 获取 session cookie + 从 HTML 提取 CSRF 值
 *   2. GET /index/index?csrf_token={CSRF} -> 返回 {"email":"user@minafter.com"}
 *   3. token 存储 cookie 和 CSRF
 */
tm_email_info_t *tm_provider_minuteinbox_generate(void) {
    int timeout = tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

    /* 第一步: GET / 获取 session cookie 和 CSRF */
    const char *hdr_init[] = {
        minbox_ua,
        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
        "Accept-Language: en-US,en;q=0.9",
        "Cache-Control: no-cache",
        "DNT: 1",
        "Upgrade-Insecure-Requests: 1",
        NULL
    };

    tm_http_response_t *r1 = tm_http_request(TM_HTTP_GET, MINBOX_ORIGIN "/", hdr_init, NULL, timeout);
    if (!r1 || r1->status != 200 || !r1->body) {
        TM_LOG_ERR("[minuteinbox] 获取首页失败, status=%ld", r1 ? r1->status : 0);
        tm_http_response_free(r1);
        return NULL;
    }

    /* 提取 CSRF 值 */
    char *csrf = minbox_extract_csrf(r1->body);
    if (!csrf || !*csrf) {
        TM_LOG_ERR("[minuteinbox] 无法从首页提取 CSRF 值");
        free(csrf);
        tm_http_response_free(r1);
        return NULL;
    }

    /* 保存 Cookie */
    char *ck = tm_strdup(r1->cookies ? r1->cookies : "");
    tm_http_response_free(r1);

    TM_LOG_DBG("[minuteinbox] 提取到 CSRF=%s", csrf);

    /* 第二步: GET /index/index?csrf_token={CSRF} 创建邮箱 */
    char create_url[512];
    snprintf(create_url, sizeof(create_url), MINBOX_ORIGIN "/index/index?csrf_token=%s", csrf);

    char h_ck[MINBOX_MAX_COOKIE];
    snprintf(h_ck, sizeof(h_ck), "Cookie: %s", ck);

    const char *hdr_create[] = {
        minbox_ua,
        "Accept: application/json, text/javascript, */*; q=0.01",
        "Accept-Language: en-US,en;q=0.9",
        "X-Requested-With: XMLHttpRequest",
        "Referer: " MINBOX_ORIGIN "/",
        h_ck,
        NULL
    };

    tm_http_response_t *r2 = tm_http_request(TM_HTTP_GET, create_url, hdr_create, NULL, timeout);
    if (!r2 || r2->status != 200 || !r2->body) {
        TM_LOG_ERR("[minuteinbox] 创建邮箱请求失败, status=%ld", r2 ? r2->status : 0);
        tm_http_response_free(r2);
        free(ck);
        free(csrf);
        return NULL;
    }

    /* 合并可能的新 Cookie */
    if (r2->cookies && r2->cookies[0]) {
        char *merged = minbox_cookie_merge(ck, r2->cookies);
        free(ck);
        ck = merged;
    }

    /* 解析响应: {"email":"user@minafter.com"} */
    cJSON *json = cJSON_Parse(r2->body);
    tm_http_response_free(r2);

    if (!json || !cJSON_IsObject(json)) {
        TM_LOG_ERR("[minuteinbox] 解析创建邮箱响应 JSON 失败");
        cJSON_Delete(json);
        free(ck);
        free(csrf);
        return NULL;
    }

    const cJSON *j_email = cJSON_GetObjectItemCaseSensitive(json, "email");
    if (!cJSON_IsString(j_email) || !j_email->valuestring || !j_email->valuestring[0]) {
        TM_LOG_ERR("[minuteinbox] 响应中未找到 email 字段");
        cJSON_Delete(json);
        free(ck);
        free(csrf);
        return NULL;
    }

    /* 验证邮箱格式 */
    if (!strchr(j_email->valuestring, '@')) {
        TM_LOG_ERR("[minuteinbox] 返回的邮箱格式无效: %s", j_email->valuestring);
        cJSON_Delete(json);
        free(ck);
        free(csrf);
        return NULL;
    }

    /* 构建 token */
    char *tok = minbox_build_token(ck, csrf);
    free(ck);
    free(csrf);

    if (!tok) {
        cJSON_Delete(json);
        return NULL;
    }

    /* 构建结果 */
    tm_email_info_t *info = tm_email_info_new();
    if (!info) {
        free(tok);
        cJSON_Delete(json);
        return NULL;
    }

    info->channel = CHANNEL_MINUTEINBOX;
    info->email = tm_strdup(j_email->valuestring);
    info->token = tok;
    info->expires_at = 0;
    info->created_at = tm_strdup("");

    cJSON_Delete(json);

    TM_LOG_DBG("[minuteinbox] 创建邮箱成功: %s", info->email);
    return info;
}

/* ========== 获取邮件 ========== */

/**
 * 获取 minuteinbox.com 邮件列表
 *
 * 流程:
 *   1. GET /index/refresh -> JSON 数组或数字 0
 *      数组元素: {"predmet":"subject","od":"from","id":1,"kdy":"time","precteno":"new|precteno"}
 *   2. 对每封邮件 GET /email/id/{id} -> HTML 正文
 *
 * @param token  minuteinbox1:... 编码的 cookie+csrf
 * @param email  完整邮箱地址
 * @param count  输出邮件数量，-1 表示请求失败
 */
tm_email_t *tm_provider_minuteinbox_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    if (!token || !email) return NULL;

    char *ck = NULL;
    char *csrf = NULL;
    if (minbox_parse_token(token, &ck, &csrf) != 0) {
        TM_LOG_ERR("[minuteinbox] token 解析失败");
        return NULL;
    }

    int timeout = tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

    /* GET /index/refresh 获取邮件列表 */
    char h_ck[MINBOX_MAX_COOKIE];
    snprintf(h_ck, sizeof(h_ck), "Cookie: %s", ck);

    const char *hdr_refresh[] = {
        minbox_ua,
        "Accept: application/json, text/javascript, */*; q=0.01",
        "Accept-Language: en-US,en;q=0.9",
        "X-Requested-With: XMLHttpRequest",
        "Referer: " MINBOX_ORIGIN "/",
        h_ck,
        NULL
    };

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, MINBOX_ORIGIN "/index/refresh", hdr_refresh, NULL, timeout);
    if (!resp || resp->status != 200 || !resp->body) {
        TM_LOG_ERR("[minuteinbox] 获取邮件列表失败, status=%ld", resp ? resp->status : 0);
        tm_http_response_free(resp);
        free(ck);
        free(csrf);
        return NULL;
    }

    /* 解析响应: 空收件箱返回数字 0，有邮件时返回 JSON 数组 */
    /* 先检查是否为单个数字 0 */
    const char *body = resp->body;
    while (*body == ' ' || *body == '\t' || *body == '\n' || *body == '\r') body++;
    if (strcmp(body, "0") == 0 || strcmp(body, "false") == 0) {
        tm_http_response_free(resp);
        free(ck);
        free(csrf);
        *count = 0;
        return NULL;
    }

    cJSON *list = cJSON_Parse(resp->body);
    tm_http_response_free(resp);

    if (!list) {
        TM_LOG_ERR("[minuteinbox] 解析邮件列表 JSON 失败");
        free(ck);
        free(csrf);
        return NULL;
    }

    if (!cJSON_IsArray(list)) {
        /* 非数组，可能是空对象或其他格式 */
        cJSON_Delete(list);
        free(ck);
        free(csrf);
        *count = 0;
        return NULL;
    }

    int list_size = cJSON_GetArraySize(list);
    if (list_size <= 0) {
        cJSON_Delete(list);
        free(ck);
        free(csrf);
        *count = 0;
        return NULL;
    }
    if (list_size > MINBOX_MAX_MAILS) list_size = MINBOX_MAX_MAILS;

    tm_email_t *emails = tm_emails_new(list_size);
    if (!emails) {
        cJSON_Delete(list);
        free(ck);
        free(csrf);
        return NULL;
    }

    int valid = 0;
    for (int i = 0; i < list_size; i++) {
        const cJSON *item = cJSON_GetArrayItem(list, i);
        if (!cJSON_IsObject(item)) continue;

        /* 提取邮件 ID（数字） */
        const cJSON *j_id = cJSON_GetObjectItemCaseSensitive(item, "id");
        if (!j_id) continue;

        char id_str[64];
        if (cJSON_IsNumber(j_id)) {
            snprintf(id_str, sizeof(id_str), "%d", (int)j_id->valuedouble);
        } else if (cJSON_IsString(j_id) && j_id->valuestring) {
            snprintf(id_str, sizeof(id_str), "%s", j_id->valuestring);
        } else {
            continue;
        }

        /* 提取列表中的基本信息（捷克语字段名） */
        const cJSON *j_predmet = cJSON_GetObjectItemCaseSensitive(item, "predmet"); /* subject */
        const cJSON *j_od = cJSON_GetObjectItemCaseSensitive(item, "od");           /* from */
        const cJSON *j_kdy = cJSON_GetObjectItemCaseSensitive(item, "kdy");         /* when/date */
        const cJSON *j_precteno = cJSON_GetObjectItemCaseSensitive(item, "precteno"); /* read status */

        /* GET /email/id/{id} 获取邮件正文 */
        char detail_url[256];
        snprintf(detail_url, sizeof(detail_url), MINBOX_ORIGIN "/email/id/%s", id_str);

        char h_ck_d[MINBOX_MAX_COOKIE];
        snprintf(h_ck_d, sizeof(h_ck_d), "Cookie: %s", ck);

        const char *hdr_detail[] = {
            minbox_ua,
            "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
            "Accept-Language: en-US,en;q=0.9",
            "Referer: " MINBOX_ORIGIN "/",
            h_ck_d,
            NULL
        };

        tm_http_response_t *rd = tm_http_request(TM_HTTP_GET, detail_url, hdr_detail, NULL, timeout);

        /* 构建 cJSON 对象用于标准化 */
        cJSON *raw = cJSON_CreateObject();
        if (!raw) {
            tm_http_response_free(rd);
            continue;
        }

        cJSON_AddStringToObject(raw, "id", id_str);
        cJSON_AddStringToObject(raw, "to", email);

        /* 映射捷克语字段到标准化字段 */
        if (cJSON_IsString(j_predmet) && j_predmet->valuestring) {
            cJSON_AddStringToObject(raw, "subject", j_predmet->valuestring);
        }
        if (cJSON_IsString(j_od) && j_od->valuestring) {
            cJSON_AddStringToObject(raw, "from", j_od->valuestring);
        }
        if (cJSON_IsString(j_kdy) && j_kdy->valuestring) {
            cJSON_AddStringToObject(raw, "date", j_kdy->valuestring);
        }

        /* 已读状态: "new" 表示未读 */
        if (cJSON_IsString(j_precteno) && j_precteno->valuestring) {
            bool is_read = (strcmp(j_precteno->valuestring, "new") != 0);
            cJSON_AddBoolToObject(raw, "is_read", is_read);
        }

        /* 填充 HTML 正文 */
        if (rd && rd->status == 200 && rd->body && rd->body[0]) {
            cJSON_AddStringToObject(raw, "html", rd->body);
            /* 生成纯文本版本 */
            size_t text_cap = strlen(rd->body) + 1;
            if (text_cap > 65536) text_cap = 65536;
            char *text_buf = (char *)malloc(text_cap);
            if (text_buf) {
                minbox_strip_tags(rd->body, text_buf, text_cap);
                cJSON_AddStringToObject(raw, "body_text", text_buf);
                free(text_buf);
            }
        }

        tm_http_response_free(rd);

        emails[valid] = tm_normalize_email(raw, email);
        cJSON_Delete(raw);
        valid++;
    }

    cJSON_Delete(list);
    free(ck);
    free(csrf);

    if (valid == 0) {
        free(emails);
        *count = 0;
        return NULL;
    }

    *count = valid;
    return emails;
}
