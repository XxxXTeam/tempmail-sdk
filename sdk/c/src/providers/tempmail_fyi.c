/**
 * temp-mail.fyi 临时邮箱渠道 — https://temp-mail.fyi
 *
 * 创建邮箱:
 *   1. GET / 获取 PHPSESSID 会话 Cookie + 从 HTML 正则提取 csrfToken（csrfToken" value="xxx"）
 *   2. POST /api/generate_email.php（body {}，头 X-CSRF-Token，带 Cookie）
 *      → {"success":true,"email_address":"xxx@...","expires_at":"...","error":null}
 * 获取邮件:
 *   POST /api/get_emails.php（body {"email_address":"xxx@..."}，头 X-CSRF-Token，带 Cookie）
 *   → {"success":true,"emails":[...],"error":null}
 *
 * token 存储策略: "tmfyi1:" + base64(json{"csrf":"...","c":"cookie_header"})
 */
#include "tempmail_internal.h"
#include <stdlib.h>
#include <string.h>

#define TMFYI_BASE "https://temp-mail.fyi"
#define TMFYI_TOK_PREFIX "tmfyi1:"
#define TMFYI_MAX_COOKIE 8192
#define TMFYI_MAX_MAILS 128

static const char *tmfyi_ua =
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";

/* ========== Base64 编解码 ========== */

static int tmfyi_b64_encode(const unsigned char *in, size_t inlen, char *out, size_t outcap) {
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

static int tmfyi_b64_val(int c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static unsigned char *tmfyi_b64_decode_alloc(const char *s, size_t *outlen) {
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
        int v0 = tmfyi_b64_val((unsigned char)s[i]);
        int v1 = tmfyi_b64_val((unsigned char)s[i + 1]);
        if (v0 < 0 || v1 < 0) { free(buf); return NULL; }
        unsigned triple = ((unsigned)v0 << 18) | ((unsigned)v1 << 12);
        if (s[i + 2] != '=') {
            int v2 = tmfyi_b64_val((unsigned char)s[i + 2]);
            if (v2 < 0) { free(buf); return NULL; }
            triple |= (unsigned)v2 << 6;
            if (s[i + 3] != '=') {
                int v3 = tmfyi_b64_val((unsigned char)s[i + 3]);
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

static char *tmfyi_build_token(const char *csrf, const char *cookie_hdr) {
    cJSON *o = cJSON_CreateObject();
    if (!o) return NULL;
    cJSON_AddStringToObject(o, "csrf", csrf ? csrf : "");
    cJSON_AddStringToObject(o, "c", cookie_hdr ? cookie_hdr : "");
    char *json = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    if (!json) return NULL;
    size_t jl = strlen(json);
    size_t bcap = 4 * ((jl + 2) / 3) + 8 + strlen(TMFYI_TOK_PREFIX);
    char *tok = (char *)malloc(bcap);
    if (!tok) { free(json); return NULL; }
    strcpy(tok, TMFYI_TOK_PREFIX);
    if (tmfyi_b64_encode((const unsigned char *)json, jl, tok + strlen(TMFYI_TOK_PREFIX), bcap - strlen(TMFYI_TOK_PREFIX)) != 0) {
        free(json);
        free(tok);
        return NULL;
    }
    free(json);
    return tok;
}

static int tmfyi_parse_token(const char *tok, char **csrf, char **cookie_hdr) {
    *csrf = NULL;
    *cookie_hdr = NULL;
    if (!tok || strncmp(tok, TMFYI_TOK_PREFIX, strlen(TMFYI_TOK_PREFIX)) != 0) return -1;
    const char *b64 = tok + strlen(TMFYI_TOK_PREFIX);
    size_t rawlen = 0;
    unsigned char *raw = tmfyi_b64_decode_alloc(b64, &rawlen);
    if (!raw) return -1;
    raw[rawlen] = '\0';
    cJSON *j = cJSON_Parse((char *)raw);
    free(raw);
    if (!j) return -1;
    const cJSON *jcsrf = cJSON_GetObjectItemCaseSensitive(j, "csrf");
    const cJSON *jc = cJSON_GetObjectItemCaseSensitive(j, "c");
    if (!cJSON_IsString(jcsrf) || !jcsrf->valuestring || !jcsrf->valuestring[0]) {
        cJSON_Delete(j);
        return -1;
    }
    *csrf = tm_strdup(jcsrf->valuestring);
    *cookie_hdr = tm_strdup(cJSON_IsString(jc) && jc->valuestring ? jc->valuestring : "");
    cJSON_Delete(j);
    return 0;
}

/* ========== CSRF 提取 ========== */

/* 从 HTML 提取 csrfToken" ... value="xxx"（对应 name="csrfToken" value="xxx"） */
static char *tmfyi_extract_csrf(const char *html) {
    if (!html) return NULL;
    const char *p = strstr(html, "csrfToken\"");
    if (!p) return NULL;
    const char *v = strstr(p, "value=\"");
    if (!v) return NULL;
    v += strlen("value=\"");
    const char *end = strchr(v, '"');
    if (!end || end == v) return NULL;
    size_t len = (size_t)(end - v);
    char *csrf = (char *)malloc(len + 1);
    if (!csrf) return NULL;
    memcpy(csrf, v, len);
    csrf[len] = '\0';
    return csrf;
}

/* ========== 创建邮箱 ========== */

tm_email_info_t *tm_provider_tempmail_fyi_generate(void) {
    int timeout = tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

    /* 1. GET / 获取 session cookie + CSRF */
    const char *hdr_home[] = {
        tmfyi_ua,
        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
        "Accept-Language: en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
        NULL
    };
    tm_http_response_t *r1 = tm_http_request(TM_HTTP_GET, TMFYI_BASE "/", hdr_home, NULL, timeout);
    if (!r1 || r1->status < 200 || r1->status >= 300 || !r1->body) {
        tm_http_response_free(r1);
        return NULL;
    }
    char *csrf = tmfyi_extract_csrf(r1->body);
    char *ck = tm_strdup(r1->cookies ? r1->cookies : "");
    tm_http_response_free(r1);
    if (!csrf || !csrf[0]) {
        free(csrf);
        free(ck);
        return NULL;
    }
    if (!ck) ck = tm_strdup("");

    /* 2. POST /api/generate_email.php 创建邮箱 */
    char h_csrf[512];
    snprintf(h_csrf, sizeof(h_csrf), "X-CSRF-Token: %s", csrf);
    char h_ck[TMFYI_MAX_COOKIE];
    int has_cookie = (ck && ck[0]);
    if (has_cookie) snprintf(h_ck, sizeof(h_ck), "Cookie: %s", ck);

    const char *hdr_create[] = {
        tmfyi_ua,
        "Accept: application/json, text/javascript, */*; q=0.01",
        "Accept-Language: en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
        "Content-Type: application/json",
        "X-Requested-With: XMLHttpRequest",
        "Referer: " TMFYI_BASE "/",
        h_csrf,
        has_cookie ? h_ck : NULL,
        NULL,
        NULL
    };

    tm_http_response_t *r2 = tm_http_request(TM_HTTP_POST, TMFYI_BASE "/api/generate_email.php", hdr_create, "{}", timeout);
    if (!r2 || r2->status < 200 || r2->status >= 300 || !r2->body) {
        tm_http_response_free(r2);
        free(csrf);
        free(ck);
        return NULL;
    }

    cJSON *root = cJSON_Parse(r2->body);
    tm_http_response_free(r2);
    if (!root) {
        free(csrf);
        free(ck);
        return NULL;
    }

    const cJSON *success = cJSON_GetObjectItemCaseSensitive(root, "success");
    const char *email = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(root, "email_address"));
    if (!cJSON_IsBool(success) || !cJSON_IsTrue(success) || !email || !strchr(email, '@')) {
        cJSON_Delete(root);
        free(csrf);
        free(ck);
        return NULL;
    }

    char *tok = tmfyi_build_token(csrf, ck);
    free(csrf);
    free(ck);
    if (!tok) {
        cJSON_Delete(root);
        return NULL;
    }

    tm_email_info_t *info = tm_email_info_new();
    if (!info) {
        cJSON_Delete(root);
        free(tok);
        return NULL;
    }
    info->channel = CHANNEL_TEMPMAIL_FYI;
    info->email = tm_strdup(email);
    info->token = tok;

    const char *exp = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(root, "expires_at"));
    info->created_at = tm_strdup(exp ? exp : "");
    cJSON_Delete(root);

    if (!info->email) {
        tm_free_email_info(info);
        return NULL;
    }
    return info;
}

/* ========== 获取邮件 ========== */

tm_email_t *tm_provider_tempmail_fyi_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    if (!token || !email || !email[0]) return NULL;

    char *csrf = NULL;
    char *ck = NULL;
    if (tmfyi_parse_token(token, &csrf, &ck) != 0) return NULL;

    int timeout = tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

    /* 构造请求体 {"email_address":"xxx@..."} */
    cJSON *reqObj = cJSON_CreateObject();
    if (!reqObj) { free(csrf); free(ck); return NULL; }
    cJSON_AddStringToObject(reqObj, "email_address", email);
    char *reqBody = cJSON_PrintUnformatted(reqObj);
    cJSON_Delete(reqObj);
    if (!reqBody) { free(csrf); free(ck); return NULL; }

    char h_csrf[512];
    snprintf(h_csrf, sizeof(h_csrf), "X-CSRF-Token: %s", csrf);
    char h_ck[TMFYI_MAX_COOKIE];
    int has_cookie = (ck && ck[0]);
    if (has_cookie) snprintf(h_ck, sizeof(h_ck), "Cookie: %s", ck);

    const char *hdr[] = {
        tmfyi_ua,
        "Accept: application/json, text/javascript, */*; q=0.01",
        "Accept-Language: en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
        "Content-Type: application/json",
        "X-Requested-With: XMLHttpRequest",
        "Referer: " TMFYI_BASE "/",
        h_csrf,
        has_cookie ? h_ck : NULL,
        NULL,
        NULL
    };

    tm_http_response_t *resp = tm_http_request(TM_HTTP_POST, TMFYI_BASE "/api/get_emails.php", hdr, reqBody, timeout);
    free(reqBody);
    free(csrf);
    free(ck);
    if (!resp || resp->status < 200 || resp->status >= 300 || !resp->body) {
        tm_http_response_free(resp);
        return NULL;
    }

    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!root) return NULL;

    const cJSON *success = cJSON_GetObjectItemCaseSensitive(root, "success");
    if (!cJSON_IsBool(success) || !cJSON_IsTrue(success)) {
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *arr = cJSON_GetObjectItemCaseSensitive(root, "emails");
    int n = (arr && cJSON_IsArray(arr)) ? cJSON_GetArraySize(arr) : 0;
    if (n == 0) {
        cJSON_Delete(root);
        *count = 0;
        return NULL;
    }
    if (n > TMFYI_MAX_MAILS) n = TMFYI_MAX_MAILS;

    tm_email_t *emails = tm_emails_new(n);
    if (!emails) {
        cJSON_Delete(root);
        return NULL;
    }

    int valid = 0;
    for (int i = 0; i < n; i++) {
        cJSON *item = cJSON_GetArrayItem(arr, i);
        if (!cJSON_IsObject(item)) continue;
        emails[valid] = tm_normalize_email(item, email);
        valid++;
    }

    cJSON_Delete(root);
    if (valid == 0) {
        free(emails);
        *count = 0;
        return NULL;
    }
    *count = valid;
    return emails;
}
