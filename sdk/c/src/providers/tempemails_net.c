/**
 * tempemails.net 渠道
 *
 * Laravel Cookie Session + CSRF + REST JSON
 * 域名: @tempemails.net
 *
 * 创建邮箱流程:
 *   1. GET https://tempemails.net/ -> 获取 session cookie + CSRF token
 *   2. POST https://tempemails.net/get_messages -> 获取自动分配的邮箱地址
 *
 * 获取邮件流程:
 *   1. POST https://tempemails.net/get_messages -> messages 数组
 *   2. GET https://tempemails.net/view/{id} -> 邮件 HTML 正文
 *
 * token 存储策略:
 *   "tempemailsnet1:" + base64(json{"csrf":"...","c":"cookie_header"})
 */
#include "tempmail_internal.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEN_BASE "https://tempemails.net"
#define TEN_TOK_PREFIX "tempemailsnet1:"
#define TEN_MAX_COOKIE 8192
#define TEN_MAX_MAILS 128

/* ========== Cookie 工具 ========== */

typedef struct {
    char k[128];
    char v[2048];
} ten_ck_t;

static int ten_ck_find(ten_ck_t *tab, int n, const char *k) {
    for (int i = 0; i < n; i++) {
        if (strcmp(tab[i].k, k) == 0) return i;
    }
    return -1;
}

/**
 * 将 Cookie 串解析并入 tab（同名覆盖），返回新的 n
 */
static int ten_ck_parse_merge(ten_ck_t *tab, int n, int maxn, const char *hdr) {
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
        int ix = ten_ck_find(tab, n, key);
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

static char *ten_cookie_join(ten_ck_t *tab, int n) {
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
static char *ten_cookie_merge(const char *a, const char *b) {
    ten_ck_t tab[128];
    int n = 0;
    n = ten_ck_parse_merge(tab, n, 128, a);
    n = ten_ck_parse_merge(tab, n, 128, b);
    return ten_cookie_join(tab, n);
}

/* ========== Base64 编解码 ========== */

static int ten_b64_encode(const unsigned char *in, size_t inlen, char *out, size_t outcap) {
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

static int ten_b64_val(int c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static unsigned char *ten_b64_decode_alloc(const char *s, size_t *outlen) {
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
        int v0 = ten_b64_val((unsigned char)s[i]);
        int v1 = ten_b64_val((unsigned char)s[i + 1]);
        if (v0 < 0 || v1 < 0) { free(buf); return NULL; }
        unsigned triple = ((unsigned)v0 << 18) | ((unsigned)v1 << 12);
        if (s[i + 2] != '=') {
            int v2 = ten_b64_val((unsigned char)s[i + 2]);
            if (v2 < 0) { free(buf); return NULL; }
            triple |= (unsigned)v2 << 6;
            if (s[i + 3] != '=') {
                int v3 = ten_b64_val((unsigned char)s[i + 3]);
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
 * token 格式: tempemailsnet1:<base64(json)>
 * json 内含 csrf 和 cookie 头
 */
static char *ten_build_token(const char *csrf, const char *cookie_hdr) {
    cJSON *o = cJSON_CreateObject();
    if (!o) return NULL;
    cJSON_AddStringToObject(o, "csrf", csrf ? csrf : "");
    cJSON_AddStringToObject(o, "c", cookie_hdr ? cookie_hdr : "");
    char *json = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    if (!json) return NULL;
    size_t jl = strlen(json);
    size_t bcap = 4 * ((jl + 2) / 3) + 8 + strlen(TEN_TOK_PREFIX);
    char *tok = (char *)malloc(bcap);
    if (!tok) { free(json); return NULL; }
    strcpy(tok, TEN_TOK_PREFIX);
    if (ten_b64_encode((const unsigned char *)json, jl, tok + strlen(TEN_TOK_PREFIX), bcap - strlen(TEN_TOK_PREFIX)) != 0) {
        free(json);
        free(tok);
        return NULL;
    }
    free(json);
    return tok;
}

/**
 * 从 token 中解析出 csrf 和 cookie 头
 */
static int ten_parse_token(const char *tok, char **csrf, char **cookie_hdr) {
    *csrf = NULL;
    *cookie_hdr = NULL;
    if (!tok || strncmp(tok, TEN_TOK_PREFIX, strlen(TEN_TOK_PREFIX)) != 0) return -1;
    const char *b64 = tok + strlen(TEN_TOK_PREFIX);
    size_t rawlen = 0;
    unsigned char *raw = ten_b64_decode_alloc(b64, &rawlen);
    if (!raw) return -1;
    raw[rawlen] = '\0';
    cJSON *j = cJSON_Parse((char *)raw);
    free(raw);
    if (!j) return -1;
    const cJSON *jcsrf = cJSON_GetObjectItemCaseSensitive(j, "csrf");
    const cJSON *jc = cJSON_GetObjectItemCaseSensitive(j, "c");
    if (!cJSON_IsString(jcsrf) || !jcsrf->valuestring || !jcsrf->valuestring[0] ||
        !cJSON_IsString(jc) || !jc->valuestring || !jc->valuestring[0]) {
        cJSON_Delete(j);
        return -1;
    }
    *csrf = tm_strdup(jcsrf->valuestring);
    *cookie_hdr = tm_strdup(jc->valuestring);
    cJSON_Delete(j);
    if (!*csrf || !*cookie_hdr) {
        free(*csrf);
        free(*cookie_hdr);
        *csrf = *cookie_hdr = NULL;
        return -1;
    }
    return 0;
}

/* ========== 公共请求头 ========== */

static const char *ten_ua =
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36";

/* ========== CSRF 提取 ========== */

/**
 * 从 HTML 中提取 <meta name="csrf-token" content="xxx">
 * 返回堆分配的 CSRF token 字符串，调用者需 free
 */
static char *ten_extract_csrf(const char *html) {
    if (!html) return NULL;
    /* 查找 <meta name="csrf-token" content="..."> */
    const char *needle = "name=\"csrf-token\"";
    const char *p = strstr(html, needle);
    if (!p) {
        /* 尝试单引号变体 */
        needle = "name='csrf-token'";
        p = strstr(html, needle);
    }
    if (!p) return NULL;

    /* 向前或向后查找 content 属性 */
    const char *content_needle = "content=\"";
    const char *c = strstr(p, content_needle);
    if (!c) {
        /* 也可能 content 在 name 之前，回退搜索 */
        const char *tag_start = p;
        while (tag_start > html && *tag_start != '<') tag_start--;
        c = strstr(tag_start, content_needle);
    }
    if (!c) return NULL;
    c += strlen(content_needle);
    const char *end = strchr(c, '"');
    if (!end || end == c) return NULL;

    size_t len = (size_t)(end - c);
    char *csrf = (char *)malloc(len + 1);
    if (!csrf) return NULL;
    memcpy(csrf, c, len);
    csrf[len] = '\0';
    return csrf;
}

/* ========== URL 编码 ========== */

/**
 * 对字符串进行 URL 编码，用于 POST body
 */
static char *ten_url_encode(const char *s) {
    if (!s) return tm_strdup("");
    size_t len = strlen(s);
    /* 最坏情况每个字符变3个 */
    char *out = (char *)malloc(len * 3 + 1);
    if (!out) return NULL;
    char *w = out;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            *w++ = (char)c;
        } else {
            w += sprintf(w, "%%%02X", c);
        }
    }
    *w = '\0';
    return out;
}

/* ========== 创建邮箱 ========== */

/**
 * 创建 tempemails.net 临时邮箱
 *
 * 流程:
 *   1. GET / -> 获取 session cookie 和 CSRF token
 *   2. POST /get_messages -> 获取自动分配的邮箱地址
 *   3. token 存储 csrf 和 cookie
 */
tm_email_info_t *tm_provider_tempemails_net_generate(void) {
    int timeout = tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

    /* 第一步: GET / 获取 session cookie 和 CSRF token */
    const char *hdrs_home[] = {
        ten_ua,
        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
        "Accept-Language: en-US,en;q=0.9",
        NULL
    };

    tm_http_response_t *r1 = tm_http_request(TM_HTTP_GET, TEN_BASE "/", hdrs_home, NULL, timeout);
    if (!r1 || r1->status != 200 || !r1->body) {
        TM_LOG_ERR("[tempemails-net] 获取首页失败, status=%ld", r1 ? r1->status : 0);
        tm_http_response_free(r1);
        return NULL;
    }

    /* 提取 CSRF token */
    char *csrf = ten_extract_csrf(r1->body);
    if (!csrf || !csrf[0]) {
        TM_LOG_ERR("[tempemails-net] 从 HTML 中提取 CSRF token 失败");
        free(csrf);
        tm_http_response_free(r1);
        return NULL;
    }

    /* 保存 Cookie */
    char *ck = tm_strdup(r1->cookies ? r1->cookies : "");
    tm_http_response_free(r1);

    if (!ck[0]) {
        TM_LOG_ERR("[tempemails-net] 未获取到 session cookie");
        free(csrf);
        free(ck);
        return NULL;
    }

    /* 第二步: POST /get_messages 获取邮箱地址 */
    char h_ck[TEN_MAX_COOKIE];
    snprintf(h_ck, sizeof(h_ck), "Cookie: %s", ck);

    char h_csrf[1024];
    snprintf(h_csrf, sizeof(h_csrf), "X-CSRF-TOKEN: %s", csrf);

    const char *hdrs_msg[] = {
        ten_ua,
        "Accept: application/json",
        "Accept-Language: en-US,en;q=0.9",
        "X-Requested-With: XMLHttpRequest",
        "Referer: " TEN_BASE "/",
        "Content-Type: application/x-www-form-urlencoded",
        h_csrf,
        h_ck,
        NULL
    };

    tm_http_response_t *r2 = tm_http_request(TM_HTTP_POST, TEN_BASE "/get_messages", hdrs_msg, "", timeout);
    if (!r2 || r2->status != 200 || !r2->body) {
        TM_LOG_ERR("[tempemails-net] POST /get_messages 失败, status=%ld", r2 ? r2->status : 0);
        tm_http_response_free(r2);
        free(csrf);
        free(ck);
        return NULL;
    }

    /* 合并新 cookie */
    if (r2->cookies && r2->cookies[0]) {
        char *merged = ten_cookie_merge(ck, r2->cookies);
        free(ck);
        ck = merged;
    }

    /* 解析 JSON 响应: {"status":true,"mailbox":"user@tempemails.net",...} */
    cJSON *json = cJSON_Parse(r2->body);
    tm_http_response_free(r2);

    if (!json || !cJSON_IsObject(json)) {
        TM_LOG_ERR("[tempemails-net] 解析 get_messages JSON 失败");
        cJSON_Delete(json);
        free(csrf);
        free(ck);
        return NULL;
    }

    /* 检查 status 字段 */
    const cJSON *j_status = cJSON_GetObjectItemCaseSensitive(json, "status");
    if (!cJSON_IsBool(j_status) || !cJSON_IsTrue(j_status)) {
        TM_LOG_ERR("[tempemails-net] get_messages 响应 status 不为 true");
        cJSON_Delete(json);
        free(csrf);
        free(ck);
        return NULL;
    }

    /* 提取 mailbox 字段 */
    const cJSON *j_mailbox = cJSON_GetObjectItemCaseSensitive(json, "mailbox");
    if (!cJSON_IsString(j_mailbox) || !j_mailbox->valuestring || !j_mailbox->valuestring[0]) {
        TM_LOG_ERR("[tempemails-net] 响应中未找到 mailbox 字段");
        cJSON_Delete(json);
        free(csrf);
        free(ck);
        return NULL;
    }

    /* 验证邮箱格式 */
    if (!strchr(j_mailbox->valuestring, '@')) {
        TM_LOG_ERR("[tempemails-net] 返回的邮箱格式无效: %s", j_mailbox->valuestring);
        cJSON_Delete(json);
        free(csrf);
        free(ck);
        return NULL;
    }

    /* 构建 token（存储 csrf 和 cookie） */
    char *tok = ten_build_token(csrf, ck);
    if (!tok) {
        TM_LOG_ERR("[tempemails-net] 构建 token 失败");
        cJSON_Delete(json);
        free(csrf);
        free(ck);
        return NULL;
    }

    /* 构建结果 */
    tm_email_info_t *info = tm_email_info_new();
    if (!info) {
        free(tok);
        cJSON_Delete(json);
        free(csrf);
        free(ck);
        return NULL;
    }

    info->channel = CHANNEL_TEMPEMAILS_NET;
    info->email = tm_strdup(j_mailbox->valuestring);
    info->token = tok;
    info->expires_at = 0;
    info->created_at = tm_strdup("");

    cJSON_Delete(json);
    free(csrf);
    free(ck);

    TM_LOG_DBG("[tempemails-net] 创建邮箱成功: %s", info->email);
    return info;
}

/* ========== 获取邮件 ========== */

/**
 * 获取 tempemails.net 邮件列表
 *
 * 流程:
 *   1. POST /get_messages -> messages 数组
 *   2. 对每封有 id 的邮件，GET /view/{id} -> HTML 正文
 *
 * @param token  tempemailsnet1:... 编码的 csrf + cookie
 * @param email  完整邮箱地址
 * @param count  输出邮件数量，-1 表示请求失败
 */
tm_email_t *tm_provider_tempemails_net_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    if (!token || !email) return NULL;

    char *csrf = NULL;
    char *ck = NULL;
    if (ten_parse_token(token, &csrf, &ck) != 0) {
        TM_LOG_ERR("[tempemails-net] token 解析失败");
        return NULL;
    }

    int timeout = tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

    /* POST /get_messages 获取邮件列表 */
    char h_ck[TEN_MAX_COOKIE];
    snprintf(h_ck, sizeof(h_ck), "Cookie: %s", ck);

    char h_csrf[1024];
    snprintf(h_csrf, sizeof(h_csrf), "X-CSRF-TOKEN: %s", csrf);

    const char *hdrs[] = {
        ten_ua,
        "Accept: application/json",
        "Accept-Language: en-US,en;q=0.9",
        "X-Requested-With: XMLHttpRequest",
        "Referer: " TEN_BASE "/",
        "Content-Type: application/x-www-form-urlencoded",
        h_csrf,
        h_ck,
        NULL
    };

    tm_http_response_t *resp = tm_http_request(TM_HTTP_POST, TEN_BASE "/get_messages", hdrs, "", timeout);
    if (!resp || resp->status != 200 || !resp->body) {
        TM_LOG_ERR("[tempemails-net] 获取邮件列表请求失败, status=%ld", resp ? resp->status : 0);
        tm_http_response_free(resp);
        free(csrf);
        free(ck);
        return NULL;
    }

    /* 解析 JSON */
    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);

    if (!json || !cJSON_IsObject(json)) {
        TM_LOG_ERR("[tempemails-net] 解析邮件列表 JSON 失败");
        cJSON_Delete(json);
        free(csrf);
        free(ck);
        return NULL;
    }

    /* 获取 messages 数组 */
    const cJSON *j_messages = cJSON_GetObjectItemCaseSensitive(json, "messages");
    if (!cJSON_IsArray(j_messages)) {
        TM_LOG_ERR("[tempemails-net] 响应中 messages 不是数组");
        cJSON_Delete(json);
        free(csrf);
        free(ck);
        return NULL;
    }

    int msg_count = cJSON_GetArraySize(j_messages);
    if (msg_count <= 0) {
        cJSON_Delete(json);
        free(csrf);
        free(ck);
        *count = 0;
        return NULL;
    }

    if (msg_count > TEN_MAX_MAILS) msg_count = TEN_MAX_MAILS;

    tm_email_t *emails = tm_emails_new(msg_count);
    if (!emails) {
        cJSON_Delete(json);
        free(csrf);
        free(ck);
        return NULL;
    }

    int valid = 0;
    for (int i = 0; i < msg_count; i++) {
        const cJSON *msg = cJSON_GetArrayItem(j_messages, i);
        if (!cJSON_IsObject(msg)) continue;

        /* 构建标准化用的 cJSON 对象 */
        cJSON *raw = cJSON_CreateObject();
        if (!raw) continue;

        /* 提取 id */
        const cJSON *j_id = cJSON_GetObjectItemCaseSensitive(msg, "id");
        char id_str[64] = "";
        if (cJSON_IsNumber(j_id)) {
            snprintf(id_str, sizeof(id_str), "%d", (int)j_id->valuedouble);
        } else if (cJSON_IsString(j_id) && j_id->valuestring) {
            snprintf(id_str, sizeof(id_str), "%s", j_id->valuestring);
        }
        cJSON_AddStringToObject(raw, "id", id_str);

        /* 提取 from / from_email */
        const cJSON *j_from_email = cJSON_GetObjectItemCaseSensitive(msg, "from_email");
        const cJSON *j_from = cJSON_GetObjectItemCaseSensitive(msg, "from");
        if (cJSON_IsString(j_from_email) && j_from_email->valuestring && j_from_email->valuestring[0]) {
            cJSON_AddStringToObject(raw, "from", j_from_email->valuestring);
        } else if (cJSON_IsString(j_from) && j_from->valuestring) {
            cJSON_AddStringToObject(raw, "from", j_from->valuestring);
        }

        /* 收件人 */
        cJSON_AddStringToObject(raw, "to", email);

        /* 主题 */
        const cJSON *j_subject = cJSON_GetObjectItemCaseSensitive(msg, "subject");
        if (cJSON_IsString(j_subject) && j_subject->valuestring) {
            cJSON_AddStringToObject(raw, "subject", j_subject->valuestring);
        }

        /* 日期 */
        const cJSON *j_date = cJSON_GetObjectItemCaseSensitive(msg, "receivedAt");
        if (cJSON_IsString(j_date) && j_date->valuestring) {
            cJSON_AddStringToObject(raw, "date", j_date->valuestring);
        }

        /* 获取邮件 HTML 正文: GET /view/{id} */
        if (id_str[0]) {
            char view_url[256];
            snprintf(view_url, sizeof(view_url), TEN_BASE "/view/%s", id_str);

            char h_ck_v[TEN_MAX_COOKIE];
            snprintf(h_ck_v, sizeof(h_ck_v), "Cookie: %s", ck);

            const char *hdrs_view[] = {
                ten_ua,
                "Accept: text/html,*/*;q=0.8",
                "Accept-Language: en-US,en;q=0.9",
                "Referer: " TEN_BASE "/",
                h_ck_v,
                NULL
            };

            tm_http_response_t *rv = tm_http_request(TM_HTTP_GET, view_url, hdrs_view, NULL, timeout);
            if (rv && rv->status == 200 && rv->body && rv->body[0]) {
                cJSON_AddStringToObject(raw, "html", rv->body);
            }
            tm_http_response_free(rv);
        }

        emails[valid] = tm_normalize_email(raw, email);
        cJSON_Delete(raw);
        valid++;
    }

    cJSON_Delete(json);
    free(csrf);
    free(ck);

    if (valid == 0) {
        free(emails);
        *count = 0;
        return NULL;
    }

    *count = valid;
    return emails;
}
