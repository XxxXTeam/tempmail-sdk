/**
 * email10min — https://email10min.com
 * GET /zh 拿 CSRF + Cookie，POST /messages 获取邮件。
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define E10M_BASE "https://email10min.com"
#define E10M_TOK_PREFIX "e10m:"

static const char *e10m_browser_hdrs[] = {
    "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
    "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
    "Cache-Control: no-cache",
    "DNT: 1",
    "Pragma: no-cache",
    "Upgrade-Insecure-Requests: 1",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    "sec-ch-ua: \"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"146\"",
    "sec-ch-ua-mobile: ?0",
    "sec-ch-ua-platform: \"Windows\"",
    NULL
};

static const char *e10m_ajax_hdrs[] = {
    "Accept: application/json, text/plain, */*",
    "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
    "Content-Type: application/x-www-form-urlencoded; charset=UTF-8",
    "Origin: https://email10min.com",
    "Referer: https://email10min.com/zh",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    "X-Requested-With: XMLHttpRequest",
    "sec-ch-ua: \"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"146\"",
    "sec-ch-ua-mobile: ?0",
    "sec-ch-ua-platform: \"Windows\"",
    "sec-fetch-dest: empty",
    "sec-fetch-mode: cors",
    "sec-fetch-site: same-origin",
    NULL
};

/* 简易 base64 编码 */
static const char b64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static char *e10m_base64_encode(const char *input, size_t len) {
    size_t out_len = 4 * ((len + 2) / 3);
    char *output = (char *)malloc(out_len + 1);
    if (!output) return NULL;
    size_t i = 0, j = 0;
    while (i < len) {
        unsigned int a = i < len ? (unsigned char)input[i++] : 0;
        unsigned int b = i < len ? (unsigned char)input[i++] : 0;
        unsigned int c = i < len ? (unsigned char)input[i++] : 0;
        unsigned int triple = (a << 16) | (b << 8) | c;
        output[j++] = b64_chars[(triple >> 18) & 0x3F];
        output[j++] = b64_chars[(triple >> 12) & 0x3F];
        output[j++] = (i > len + 1) ? '=' : b64_chars[(triple >> 6) & 0x3F];
        output[j++] = (i > len) ? '=' : b64_chars[triple & 0x3F];
    }
    output[j] = '\0';
    return output;
}

static int b64_decode_char(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static char *e10m_base64_decode(const char *input, size_t *out_len) {
    size_t in_len = strlen(input);
    if (in_len % 4 != 0) return NULL;
    size_t decoded_len = in_len / 4 * 3;
    if (input[in_len - 1] == '=') decoded_len--;
    if (input[in_len - 2] == '=') decoded_len--;
    char *output = (char *)malloc(decoded_len + 1);
    if (!output) return NULL;
    size_t i = 0, j = 0;
    while (i < in_len) {
        int a = b64_decode_char(input[i++]);
        int b = b64_decode_char(input[i++]);
        int c = (input[i] == '=') ? 0 : b64_decode_char(input[i]); i++;
        int d = (input[i] == '=') ? 0 : b64_decode_char(input[i]); i++;
        if (a < 0 || b < 0) { free(output); return NULL; }
        unsigned int triple = ((unsigned)a << 18) | ((unsigned)b << 12) | ((unsigned)c << 6) | (unsigned)d;
        if (j < decoded_len) output[j++] = (char)((triple >> 16) & 0xFF);
        if (j < decoded_len) output[j++] = (char)((triple >> 8) & 0xFF);
        if (j < decoded_len) output[j++] = (char)(triple & 0xFF);
    }
    output[decoded_len] = '\0';
    *out_len = decoded_len;
    return output;
}

/* 编码 token: cookie + csrf */
static char *e10m_encode_token(const char *cookie, const char *csrf) {
    char raw[8192];
    int n = snprintf(raw, sizeof(raw), "{\"c\":\"%s\",\"t\":\"%s\"}", cookie, csrf);
    char *b64 = e10m_base64_encode(raw, (size_t)n);
    if (!b64) return NULL;
    size_t prefix_len = strlen(E10M_TOK_PREFIX);
    size_t b64_len = strlen(b64);
    char *token = (char *)malloc(prefix_len + b64_len + 1);
    if (!token) { free(b64); return NULL; }
    memcpy(token, E10M_TOK_PREFIX, prefix_len);
    memcpy(token + prefix_len, b64, b64_len + 1);
    free(b64);
    return token;
}

/* 解码 token */
static int e10m_decode_token(const char *token, char **out_cookie, char **out_csrf) {
    size_t prefix_len = strlen(E10M_TOK_PREFIX);
    if (strncmp(token, E10M_TOK_PREFIX, prefix_len) != 0) return 0;
    size_t decoded_len = 0;
    char *decoded = e10m_base64_decode(token + prefix_len, &decoded_len);
    if (!decoded) return 0;
    cJSON *cj = cJSON_Parse(decoded);
    free(decoded);
    if (!cj) return 0;
    const char *c = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(cj, "c"));
    const char *t = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(cj, "t"));
    if (!c || !c[0] || !t || !t[0]) {
        cJSON_Delete(cj);
        return 0;
    }
    *out_cookie = tm_strdup(c);
    *out_csrf = tm_strdup(t);
    cJSON_Delete(cj);
    return 1;
}

/* 从 HTML 提取 CSRF token */
static char *e10m_extract_csrf(const char *html) {
    /* 尝试 <meta name="csrf-token" content="..."> */
    const char *p = strstr(html, "name=\"csrf-token\"");
    if (p) {
        const char *q = strstr(p, "content=\"");
        if (q) {
            q += 9;
            const char *end = strchr(q, '"');
            if (end && end > q) {
                size_t len = (size_t)(end - q);
                char *csrf = (char *)malloc(len + 1);
                memcpy(csrf, q, len);
                csrf[len] = '\0';
                return csrf;
            }
        }
    }
    return NULL;
}

/* 从 HTML 提取邮箱地址 */
static char *e10m_extract_email(const char *html) {
    /* 尝试 id="emailAddress">xxx@yyy */
    const char *p = strstr(html, "id=\"emailAddress\"");
    if (p) {
        const char *gt = strchr(p, '>');
        if (gt) {
            gt++;
            const char *lt = strchr(gt, '<');
            if (lt && lt > gt) {
                size_t len = (size_t)(lt - gt);
                char *buf = (char *)malloc(len + 1);
                memcpy(buf, gt, len);
                buf[len] = '\0';
                if (strchr(buf, '@')) return buf;
                free(buf);
            }
        }
    }
    /* 尝试 value="xxx@yyy" */
    const char *v = html;
    while ((v = strstr(v, "value=\"")) != NULL) {
        v += 7;
        const char *end = strchr(v, '"');
        if (end && end > v) {
            size_t len = (size_t)(end - v);
            char *buf = (char *)malloc(len + 1);
            memcpy(buf, v, len);
            buf[len] = '\0';
            if (strchr(buf, '@') && !strstr(buf, "email10min") && !strstr(buf, "example")) {
                return buf;
            }
            free(buf);
        }
    }
    return NULL;
}

tm_email_info_t *tm_provider_email10min_generate(void) {
    tm_http_response_t *r = tm_http_request(
        TM_HTTP_GET, E10M_BASE "/zh", e10m_browser_hdrs, NULL, 15);
    if (!r || r->status < 200 || r->status >= 300) {
        tm_http_response_free(r);
        return NULL;
    }

    char *cookie = r->cookies ? tm_strdup(r->cookies) : tm_strdup("");
    char *html = r->body ? tm_strdup(r->body) : tm_strdup("");
    tm_http_response_free(r);

    char *csrf = e10m_extract_csrf(html);
    if (!csrf) {
        free(cookie);
        free(html);
        return NULL;
    }

    char *email_addr = e10m_extract_email(html);
    free(html);
    if (!email_addr) {
        free(cookie);
        free(csrf);
        return NULL;
    }

    char *token = e10m_encode_token(cookie, csrf);
    free(cookie);
    free(csrf);
    if (!token) {
        free(email_addr);
        return NULL;
    }

    tm_email_info_t *info = tm_email_info_new();
    info->channel = CHANNEL_EMAIL10MIN;
    info->email = email_addr;
    info->token = token;
    return info;
}

tm_email_t *tm_provider_email10min_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    if (!token || !token[0] || !email) return NULL;

    char *cookie = NULL, *csrf = NULL;
    if (!e10m_decode_token(token, &cookie, &csrf)) {
        return NULL;
    }

    char ts_str[32];
    snprintf(ts_str, sizeof(ts_str), "%ld", (long)time(NULL) * 1000);

    char url[256];
    snprintf(url, sizeof(url), E10M_BASE "/messages?%s", ts_str);

    char body[4096];
    snprintf(body, sizeof(body), "_token=%s&captcha=", csrf);

    char ck_line[8704];
    snprintf(ck_line, sizeof(ck_line), "Cookie: %s", cookie);

    /* 构建 headers 数组 */
    const char *post_hdrs[24];
    int i = 0;
    for (const char **p = e10m_ajax_hdrs; *p; p++) post_hdrs[i++] = *p;
    post_hdrs[i++] = ck_line;
    post_hdrs[i] = NULL;

    tm_http_response_t *r = tm_http_request(
        TM_HTTP_POST, url, post_hdrs, body, 15);
    free(cookie);
    free(csrf);
    if (!r || r->status < 200 || r->status >= 300) {
        tm_http_response_free(r);
        return NULL;
    }

    cJSON *cj = cJSON_Parse(r->body ? r->body : "");
    tm_http_response_free(r);
    if (!cj || !cJSON_IsObject(cj)) {
        cJSON_Delete(cj);
        return NULL;
    }

    cJSON *messages = cJSON_GetObjectItemCaseSensitive(cj, "messages");
    if (!messages || !cJSON_IsArray(messages)) {
        cJSON_Delete(cj);
        *count = 0;
        return NULL;
    }

    int n = cJSON_GetArraySize(messages);
    *count = n;
    if (n == 0) {
        cJSON_Delete(cj);
        return NULL;
    }

    tm_email_t *emails = tm_emails_new(n);
    for (int j = 0; j < n; j++) {
        cJSON *raw = cJSON_GetArrayItem(messages, j);
        const char *msg_id = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(raw, "id"));
        if (!msg_id) msg_id = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(raw, "message_id"));
        const char *from = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(raw, "from"));
        if (!from) from = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(raw, "sender"));
        const char *to = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(raw, "to"));
        const char *subj = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(raw, "subject"));
        const char *text = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(raw, "text"));
        if (!text) text = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(raw, "body"));
        const char *html_content = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(raw, "html"));
        if (!html_content) html_content = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(raw, "body_html"));
        const char *date = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(raw, "date"));
        if (!date) date = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(raw, "created_at"));

        char idbuf[64];
        if (!msg_id) {
            snprintf(idbuf, sizeof(idbuf), "%d", j);
            msg_id = idbuf;
        }

        cJSON *one = cJSON_CreateObject();
        cJSON_AddStringToObject(one, "id", msg_id);
        cJSON_AddStringToObject(one, "from", from ? from : "");
        cJSON_AddStringToObject(one, "to", to ? to : email);
        cJSON_AddStringToObject(one, "subject", subj ? subj : "");
        cJSON_AddStringToObject(one, "text", text ? text : "");
        cJSON_AddStringToObject(one, "html", html_content ? html_content : "");
        cJSON_AddStringToObject(one, "date", date ? date : "");
        cJSON_AddBoolToObject(one, "isRead", 0);
        emails[j] = tm_normalize_email(one, email);
        cJSON_Delete(one);
    }

    cJSON_Delete(cj);
    return emails;
}
