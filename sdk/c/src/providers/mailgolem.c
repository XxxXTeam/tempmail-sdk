/**
 * mailgolem.com 渠道
 *
 * 创建邮箱流程:
 *   1. GET https://mailgolem.com/ 获取 session cookie + CSRF token
 *   2. GET https://mailgolem.com/random-email-address 获取随机邮箱地址
 *
 * 获取邮件流程:
 *   1. GET https://mailgolem.com/ 获取新 session + CSRF token
 *   2. POST https://mailgolem.com/fetch-emails/{email} 获取邮件列表
 *      body: _token={csrf_token}
 *      响应: JSON 数组 [{id, from, subject}] 或空数组 []
 *
 * token 存储 CSRF token 值（通过 base64 编码的 JSON，包含 cookie 和 csrf）
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAILGOLEM_BASE "https://mailgolem.com"
#define MAILGOLEM_TOK_PREFIX "mailgolem1:"
#define MAILGOLEM_MAX_COOKIE 8192
#define MAILGOLEM_MAX_MAILS 128

/* ========== Base64 编解码 ========== */

static int mailgolem_b64_encode(const unsigned char *in, size_t inlen, char *out, size_t outcap) {
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

static int mailgolem_b64_val(int c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static unsigned char *mailgolem_b64_decode_alloc(const char *s, size_t *outlen) {
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
        int v0 = mailgolem_b64_val((unsigned char)s[i]);
        int v1 = mailgolem_b64_val((unsigned char)s[i + 1]);
        if (v0 < 0 || v1 < 0) { free(buf); return NULL; }
        unsigned triple = ((unsigned)v0 << 18) | ((unsigned)v1 << 12);
        if (s[i + 2] != '=') {
            int v2 = mailgolem_b64_val((unsigned char)s[i + 2]);
            if (v2 < 0) { free(buf); return NULL; }
            triple |= (unsigned)v2 << 6;
            if (s[i + 3] != '=') {
                int v3 = mailgolem_b64_val((unsigned char)s[i + 3]);
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
 * 构建 token: "mailgolem1:" + base64(json)
 * json: {"c":"cookie_str","t":"csrf_token"}
 */
static char *mailgolem_build_token(const char *cookie_hdr, const char *csrf_token) {
    cJSON *o = cJSON_CreateObject();
    if (!o) return NULL;
    cJSON_AddStringToObject(o, "c", cookie_hdr ? cookie_hdr : "");
    cJSON_AddStringToObject(o, "t", csrf_token ? csrf_token : "");
    char *json = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    if (!json) return NULL;
    size_t jl = strlen(json);
    size_t bcap = 4 * ((jl + 2) / 3) + 8 + strlen(MAILGOLEM_TOK_PREFIX);
    char *tok = (char *)malloc(bcap);
    if (!tok) { free(json); return NULL; }
    strcpy(tok, MAILGOLEM_TOK_PREFIX);
    if (mailgolem_b64_encode((const unsigned char *)json, jl,
                              tok + strlen(MAILGOLEM_TOK_PREFIX),
                              bcap - strlen(MAILGOLEM_TOK_PREFIX)) != 0) {
        free(json);
        free(tok);
        return NULL;
    }
    free(json);
    return tok;
}

/**
 * 解析 token，提取 cookie 和 CSRF token
 * 成功返回 0，失败返回 -1
 */
static int mailgolem_parse_token(const char *tok, char **cookie_hdr, char **csrf_token) {
    *cookie_hdr = NULL;
    *csrf_token = NULL;
    if (!tok || strncmp(tok, MAILGOLEM_TOK_PREFIX, strlen(MAILGOLEM_TOK_PREFIX)) != 0) return -1;
    const char *b64 = tok + strlen(MAILGOLEM_TOK_PREFIX);
    size_t rawlen = 0;
    unsigned char *raw = mailgolem_b64_decode_alloc(b64, &rawlen);
    if (!raw) return -1;
    raw[rawlen] = '\0';
    cJSON *j = cJSON_Parse((char *)raw);
    free(raw);
    if (!j) return -1;
    const cJSON *jc = cJSON_GetObjectItemCaseSensitive(j, "c");
    const cJSON *jt = cJSON_GetObjectItemCaseSensitive(j, "t");
    if (!cJSON_IsString(jc) || !jc->valuestring) {
        cJSON_Delete(j);
        return -1;
    }
    *cookie_hdr = tm_strdup(jc->valuestring);
    *csrf_token = tm_strdup(cJSON_IsString(jt) && jt->valuestring ? jt->valuestring : "");
    cJSON_Delete(j);
    if (!*cookie_hdr) {
        free(*csrf_token);
        *csrf_token = NULL;
        return -1;
    }
    return 0;
}

/* ========== HTML 解析工具 ========== */

/**
 * 从 HTML 中提取 CSRF token
 * 查找: <input type="hidden" name="_token" id="token" value="{csrf_token}">
 */
static char *mailgolem_extract_csrf(const char *html) {
    if (!html) return NULL;

    /* 查找 name="_token" */
    const char *p = html;
    while ((p = strstr(p, "name=\"_token\"")) != NULL) {
        /* 回溯到 <input 找到完整标签范围 */
        const char *tag_start = p;
        while (tag_start > html && *tag_start != '<') tag_start--;

        /* 向前查找标签结束 */
        const char *tag_end = strchr(p, '>');
        if (!tag_end) { p++; continue; }

        /* 在标签范围内查找 value="..." */
        const char *val = strstr(tag_start, "value=\"");
        if (val && val < tag_end) {
            val += strlen("value=\"");
            const char *end = strchr(val, '"');
            if (end && end < tag_end) {
                size_t len = (size_t)(end - val);
                if (len > 0 && len < 256) {
                    char *csrf = (char *)malloc(len + 1);
                    if (csrf) {
                        memcpy(csrf, val, len);
                        csrf[len] = '\0';
                        return csrf;
                    }
                }
            }
        }
        p++;
    }
    return NULL;
}

/* URL 编码单个字符 */
static int mailgolem_url_encode_char(char c, char *out) {
    static const char hex[] = "0123456789ABCDEF";
    unsigned char uc = (unsigned char)c;
    if ((uc >= 'A' && uc <= 'Z') || (uc >= 'a' && uc <= 'z') ||
        (uc >= '0' && uc <= '9') || uc == '-' || uc == '_' || uc == '.' || uc == '~') {
        out[0] = c;
        return 1;
    }
    out[0] = '%';
    out[1] = hex[(uc >> 4) & 0x0F];
    out[2] = hex[uc & 0x0F];
    return 3;
}

/* URL 编码字符串 */
static char *mailgolem_url_encode(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *out = (char *)malloc(len * 3 + 1);
    if (!out) return NULL;
    size_t o = 0;
    for (size_t i = 0; i < len; i++) {
        o += (size_t)mailgolem_url_encode_char(s[i], out + o);
    }
    out[o] = '\0';
    return out;
}

/* 公共请求头 UA */
static const char *mailgolem_ua =
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36";

/**
 * 获取 session cookie 和 CSRF token
 * 成功返回 0，失败返回 -1
 */
static int mailgolem_fetch_session(char **out_cookie, char **out_csrf) {
    int timeout = tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

    const char *hdr[] = {
        mailgolem_ua,
        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
        "Accept-Language: en-US,en;q=0.9",
        NULL
    };

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, MAILGOLEM_BASE "/", hdr, NULL, timeout);
    if (!resp || (resp->status != 200 && resp->status != 302) || !resp->body) {
        TM_LOG_ERR("[mailgolem] 获取首页失败, status=%ld", resp ? resp->status : 0);
        tm_http_response_free(resp);
        return -1;
    }

    /* 提取 cookie */
    *out_cookie = tm_strdup(resp->cookies ? resp->cookies : "");

    /* 从 HTML 中提取 CSRF token */
    *out_csrf = mailgolem_extract_csrf(resp->body);
    tm_http_response_free(resp);

    if (!*out_csrf || !(*out_csrf)[0]) {
        TM_LOG_ERR("[mailgolem] 未找到 CSRF token");
        free(*out_cookie);
        free(*out_csrf);
        *out_cookie = NULL;
        *out_csrf = NULL;
        return -1;
    }

    return 0;
}

/* ========== 创建邮箱 ========== */

/**
 * 创建 mailgolem 临时邮箱
 *
 * 流程:
 *   1. GET https://mailgolem.com/ 获取 session cookie + CSRF token
 *   2. GET https://mailgolem.com/random-email-address 获取随机邮箱地址
 */
tm_email_info_t *tm_provider_mailgolem_generate(void) {
    int timeout = tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

    /* 第一步: 获取 session 和 CSRF */
    char *cookie = NULL;
    char *csrf = NULL;
    if (mailgolem_fetch_session(&cookie, &csrf) != 0) {
        return NULL;
    }

    /* 第二步: GET /random-email-address 获取随机邮箱 */
    char h_cookie[MAILGOLEM_MAX_COOKIE];
    snprintf(h_cookie, sizeof(h_cookie), "Cookie: %s", cookie);

    const char *hdr_random[] = {
        mailgolem_ua,
        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
        "Accept-Language: en-US,en;q=0.9",
        "Referer: " MAILGOLEM_BASE "/",
        h_cookie,
        NULL
    };

    tm_http_response_t *r2 = tm_http_request(TM_HTTP_GET, MAILGOLEM_BASE "/random-email-address",
                                              hdr_random, NULL, timeout);
    if (!r2 || r2->status != 200 || !r2->body) {
        TM_LOG_ERR("[mailgolem] 获取随机邮箱失败, status=%ld", r2 ? r2->status : 0);
        tm_http_response_free(r2);
        free(cookie);
        free(csrf);
        return NULL;
    }

    /* 合并新返回的 cookie */
    if (r2->cookies && r2->cookies[0]) {
        size_t clen = strlen(cookie) + strlen(r2->cookies) + 4;
        char *merged = (char *)malloc(clen);
        if (merged) {
            if (cookie[0]) {
                snprintf(merged, clen, "%s; %s", cookie, r2->cookies);
            } else {
                strcpy(merged, r2->cookies);
            }
            free(cookie);
            cookie = merged;
        }
    }

    /* 响应体为纯文本邮箱地址，去除首尾空白 */
    char *email_addr = tm_strdup(r2->body);
    tm_http_response_free(r2);

    if (!email_addr || !email_addr[0]) {
        TM_LOG_ERR("[mailgolem] 响应中无邮箱地址");
        free(email_addr);
        free(cookie);
        free(csrf);
        return NULL;
    }

    /* 去除首尾空白字符 */
    char *start = email_addr;
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r') start++;
    char *end = start + strlen(start);
    while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' || end[-1] == '\r')) end--;
    *end = '\0';

    /* 验证邮箱格式 */
    if (!strchr(start, '@')) {
        TM_LOG_ERR("[mailgolem] 返回的邮箱格式无效: %s", start);
        free(email_addr);
        free(cookie);
        free(csrf);
        return NULL;
    }

    /* 构建 token */
    char *tok = mailgolem_build_token(cookie, csrf);
    free(cookie);
    free(csrf);
    if (!tok) {
        free(email_addr);
        return NULL;
    }

    tm_email_info_t *info = tm_email_info_new();
    if (!info) {
        free(email_addr);
        free(tok);
        return NULL;
    }
    info->channel = CHANNEL_MAILGOLEM;

    /* 如果 start 不等于 email_addr 起始位置，需要重新复制 */
    if (start != email_addr) {
        info->email = tm_strdup(start);
        free(email_addr);
    } else {
        info->email = email_addr;
    }
    info->token = tok;
    info->expires_at = 0;
    info->created_at = tm_strdup("");

    TM_LOG_DBG("[mailgolem] 创建邮箱成功: %s", info->email);
    return info;
}

/* ========== 获取邮件 ========== */

/**
 * 获取 mailgolem 邮件列表
 *
 * 流程:
 *   1. GET https://mailgolem.com/ 获取新 session + CSRF token
 *   2. POST https://mailgolem.com/fetch-emails/{email}
 *      body: _token={csrf_token}
 *   3. 解析 JSON 数组
 */
tm_email_t *tm_provider_mailgolem_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    if (!token || !email) return NULL;

    int timeout = tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

    /* 获取新的 session 和 CSRF token（因为 Laravel session 可能过期） */
    char *cookie = NULL;
    char *csrf = NULL;
    if (mailgolem_fetch_session(&cookie, &csrf) != 0) {
        return NULL;
    }

    /* 构建 POST URL: /fetch-emails/{email} */
    char url[512];
    snprintf(url, sizeof(url), MAILGOLEM_BASE "/fetch-emails/%s", email);

    /* 构建 POST body: _token={csrf_token} */
    char *encoded_csrf = mailgolem_url_encode(csrf);
    if (!encoded_csrf) {
        free(cookie);
        free(csrf);
        return NULL;
    }
    char post_body[1024];
    snprintf(post_body, sizeof(post_body), "_token=%s", encoded_csrf);
    free(encoded_csrf);

    /* 构建请求头 */
    char h_cookie[MAILGOLEM_MAX_COOKIE];
    snprintf(h_cookie, sizeof(h_cookie), "Cookie: %s", cookie);

    const char *hdr_fetch[] = {
        mailgolem_ua,
        "Accept: application/json, text/javascript, */*; q=0.01",
        "Accept-Language: en-US,en;q=0.9",
        "Content-Type: application/x-www-form-urlencoded",
        "X-Requested-With: XMLHttpRequest",
        "Referer: " MAILGOLEM_BASE "/",
        h_cookie,
        NULL
    };

    tm_http_response_t *resp = tm_http_request(TM_HTTP_POST, url, hdr_fetch, post_body, timeout);
    free(cookie);
    free(csrf);

    if (!resp || resp->status != 200 || !resp->body) {
        TM_LOG_ERR("[mailgolem] fetch-emails 请求失败, status=%ld", resp ? resp->status : 0);
        tm_http_response_free(resp);
        return NULL;
    }

    /* 解析 JSON 数组 */
    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);

    if (!json) {
        TM_LOG_ERR("[mailgolem] 解析 fetch-emails 响应 JSON 失败");
        return NULL;
    }

    if (!cJSON_IsArray(json)) {
        TM_LOG_ERR("[mailgolem] fetch-emails 响应不是 JSON 数组");
        cJSON_Delete(json);
        return NULL;
    }

    int mail_count = cJSON_GetArraySize(json);
    if (mail_count <= 0) {
        cJSON_Delete(json);
        *count = 0;
        return NULL;
    }
    if (mail_count > MAILGOLEM_MAX_MAILS) mail_count = MAILGOLEM_MAX_MAILS;

    tm_email_t *emails = tm_emails_new(mail_count);
    if (!emails) {
        cJSON_Delete(json);
        return NULL;
    }

    int valid_count = 0;
    for (int i = 0; i < mail_count; i++) {
        const cJSON *item = cJSON_GetArrayItem(json, i);
        if (!cJSON_IsObject(item)) continue;

        /* 提取字段: id, from, subject */
        const cJSON *j_id = cJSON_GetObjectItemCaseSensitive(item, "id");
        const cJSON *j_from = cJSON_GetObjectItemCaseSensitive(item, "from");
        const cJSON *j_subject = cJSON_GetObjectItemCaseSensitive(item, "subject");

        /* 构建标准化邮件 JSON */
        cJSON *raw = cJSON_CreateObject();
        if (!raw) continue;

        /* id 可能是数字或字符串 */
        if (cJSON_IsNumber(j_id)) {
            char id_buf[32];
            snprintf(id_buf, sizeof(id_buf), "%d", j_id->valueint);
            cJSON_AddStringToObject(raw, "id", id_buf);
        } else if (cJSON_IsString(j_id) && j_id->valuestring) {
            cJSON_AddStringToObject(raw, "id", j_id->valuestring);
        } else {
            cJSON_AddStringToObject(raw, "id", "");
        }

        cJSON_AddStringToObject(raw, "from", TM_JSON_STR(j_from, ""));
        cJSON_AddStringToObject(raw, "to", email);
        cJSON_AddStringToObject(raw, "subject", TM_JSON_STR(j_subject, ""));
        cJSON_AddStringToObject(raw, "date", "");
        cJSON_AddStringToObject(raw, "html", "");
        cJSON_AddBoolToObject(raw, "isRead", 0);
        cJSON_AddItemToObject(raw, "attachments", cJSON_CreateArray());

        emails[valid_count] = tm_normalize_email(raw, email);
        cJSON_Delete(raw);
        valid_count++;
    }

    cJSON_Delete(json);

    if (valid_count == 0) {
        free(emails);
        *count = 0;
        return NULL;
    }

    *count = valid_count;
    return emails;
}
