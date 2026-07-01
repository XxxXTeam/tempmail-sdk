/**
 * smailpro.com 临时邮箱渠道 — https://smailpro.com
 *
 * 两段式调用：
 *   1. GET https://smailpro.com/app/payload?url={目标API}[&email=&mid=] → 返回 JWT（纯文本）
 *   2. 带 JWT 调用 api.sonjj.com 对应接口（payload={JWT}）
 * 创建邮箱、获取列表、获取详情均需先取 payload 再调用 sonjj API。
 * token 未使用，传空字符串即可。
 */
#include "tempmail_internal.h"
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>

#define SMAILPRO_BASE "https://smailpro.com"
#define SMAILPRO_API_BASE "https://api.sonjj.com/v1/temp_email"
#define SMAILPRO_MAX_MAILS 128

static const char *smailpro_headers[] = {
    "Accept: application/json, text/plain, */*",
    "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8",
    "Referer: https://smailpro.com/",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    NULL
};

static char *smailpro_curl_escape(const char *s) {
    if (!s) return tm_strdup("");
    CURL *c = curl_easy_init();
    if (!c) return tm_strdup(s);
    char *e = curl_easy_escape(c, s, 0);
    curl_easy_cleanup(c);
    if (!e) return tm_strdup(s);
    char *d = tm_strdup(e);
    curl_free(e);
    return d;
}

/*
 * 获取访问 sonjj API 所需的 JWT
 * target_url 为目标 sonjj 接口地址（未编码），email/mid 为附加查询参数（可为 NULL）
 * 返回堆分配的 JWT 字符串，调用者负责 free；失败返回 NULL
 */
static char *smailpro_fetch_payload(const char *target_url, const char *email, const char *mid, int timeout) {
    char *enc_url = smailpro_curl_escape(target_url);
    char *enc_email = email ? smailpro_curl_escape(email) : NULL;
    char *enc_mid = mid ? smailpro_curl_escape(mid) : NULL;

    /* 拼接 payload 请求 URL */
    size_t cap = strlen(SMAILPRO_BASE) + strlen(enc_url) + 64;
    if (enc_email) cap += strlen(enc_email) + 8;
    if (enc_mid) cap += strlen(enc_mid) + 6;
    char *url = (char *)malloc(cap);
    if (!url) {
        free(enc_url); free(enc_email); free(enc_mid);
        return NULL;
    }
    int off = snprintf(url, cap, "%s/app/payload?url=%s", SMAILPRO_BASE, enc_url);
    if (enc_email) off += snprintf(url + off, cap - (size_t)off, "&email=%s", enc_email);
    if (enc_mid) off += snprintf(url + off, cap - (size_t)off, "&mid=%s", enc_mid);
    free(enc_url); free(enc_email); free(enc_mid);

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, smailpro_headers, NULL, timeout);
    free(url);
    if (!resp || resp->status < 200 || resp->status >= 300 || !resp->body) {
        tm_http_response_free(resp);
        return NULL;
    }

    /* payload 接口返回纯文本 JWT，去除两端空白与引号 */
    char *jwt = tm_strdup(resp->body);
    tm_http_response_free(resp);
    if (!jwt) return NULL;
    char *p = jwt;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '"') p++;
    size_t len = strlen(p);
    while (len > 0 && (p[len - 1] == ' ' || p[len - 1] == '\t' || p[len - 1] == '\n' ||
                       p[len - 1] == '\r' || p[len - 1] == '"')) len--;
    char *trimmed = (char *)malloc(len + 1);
    if (!trimmed) { free(jwt); return NULL; }
    memcpy(trimmed, p, len);
    trimmed[len] = '\0';
    free(jwt);
    if (!trimmed[0]) { free(trimmed); return NULL; }
    return trimmed;
}

/*
 * 携带 JWT 调用 sonjj API，返回解析后的 cJSON（调用者 cJSON_Delete）
 * target_url 为目标接口地址，email/mid 为获取 payload 时需要的附加参数
 */
static cJSON *smailpro_call_api(const char *target_url, const char *email, const char *mid, int timeout) {
    char *payload = smailpro_fetch_payload(target_url, email, mid, timeout);
    if (!payload) return NULL;

    char *enc_payload = smailpro_curl_escape(payload);
    free(payload);

    size_t cap = strlen(target_url) + strlen(enc_payload) + 16;
    char *url = (char *)malloc(cap);
    if (!url) { free(enc_payload); return NULL; }
    snprintf(url, cap, "%s?payload=%s", target_url, enc_payload);
    free(enc_payload);

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, smailpro_headers, NULL, timeout);
    free(url);
    if (!resp || resp->status < 200 || resp->status >= 300 || !resp->body) {
        tm_http_response_free(resp);
        return NULL;
    }
    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    return root;
}

/*
 * 创建 smailpro 临时邮箱
 * 调用 sonjj create 接口，返回 {"email":"...","expired_at":...}
 */
tm_email_info_t *tm_provider_smailpro_generate(void) {
    int timeout = tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

    cJSON *root = smailpro_call_api(SMAILPRO_API_BASE "/create", NULL, NULL, timeout);
    if (!root) return NULL;

    const char *email = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(root, "email"));
    if (!email || !email[0]) {
        cJSON_Delete(root);
        return NULL;
    }

    tm_email_info_t *info = tm_email_info_new();
    if (!info) {
        cJSON_Delete(root);
        return NULL;
    }
    info->channel = CHANNEL_SMAILPRO;
    info->email = tm_strdup(email);
    info->token = tm_strdup("");

    /* expired_at 可能是数字毫秒/秒时间戳 */
    const cJSON *exp = cJSON_GetObjectItemCaseSensitive(root, "expired_at");
    if (cJSON_IsNumber(exp) && exp->valuedouble > 0) {
        double v = exp->valuedouble;
        info->expires_at = (v > 1e12) ? (long long)v : (long long)(v * 1000.0);
    }
    cJSON_Delete(root);

    if (!info->email) {
        tm_free_email_info(info);
        return NULL;
    }
    return info;
}

/*
 * 获取 smailpro 邮件列表
 *   1. 调用 sonjj inbox 接口获取列表（mid/from/subject/datetime）
 *   2. 对每封邮件调用 message 接口获取正文（body/textBody）
 * token 未使用
 */
tm_email_t *tm_provider_smailpro_get_emails(const char *token, const char *email, int *count) {
    (void)token;
    *count = -1;
    if (!email || !email[0]) return NULL;

    int timeout = tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

    cJSON *root = smailpro_call_api(SMAILPRO_API_BASE "/inbox", email, NULL, timeout);
    if (!root) return NULL;

    cJSON *mails = cJSON_GetObjectItemCaseSensitive(root, "mails");
    int n = (mails && cJSON_IsArray(mails)) ? cJSON_GetArraySize(mails) : 0;
    if (n == 0) {
        cJSON_Delete(root);
        *count = 0;
        return NULL;
    }
    if (n > SMAILPRO_MAX_MAILS) n = SMAILPRO_MAX_MAILS;

    tm_email_t *emails = tm_emails_new(n);
    if (!emails) {
        cJSON_Delete(root);
        return NULL;
    }

    int valid = 0;
    for (int i = 0; i < n; i++) {
        const cJSON *item = cJSON_GetArrayItem(mails, i);
        if (!cJSON_IsObject(item)) continue;
        const char *mid = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(item, "mid"));

        cJSON *raw = cJSON_CreateObject();
        if (!raw) continue;

        cJSON_AddStringToObject(raw, "id", mid ? mid : "");
        cJSON_AddStringToObject(raw, "to", email);
        const char *from = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(item, "from"));
        cJSON_AddStringToObject(raw, "from", from ? from : "");
        const char *subject = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(item, "subject"));
        cJSON_AddStringToObject(raw, "subject", subject ? subject : "");
        const char *datetime = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(item, "datetime"));
        cJSON_AddStringToObject(raw, "date", datetime ? datetime : "");

        /* 拉取正文（body=HTML, textBody=纯文本），失败时保留列表元信息 */
        if (mid && mid[0]) {
            cJSON *msg = smailpro_call_api(SMAILPRO_API_BASE "/message", email, mid, timeout);
            if (msg) {
                const char *body = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(msg, "body"));
                const char *textBody = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(msg, "textBody"));
                if (body && body[0]) cJSON_AddStringToObject(raw, "html", body);
                if (textBody && textBody[0]) cJSON_AddStringToObject(raw, "text", textBody);
                cJSON_Delete(msg);
            }
        }

        emails[valid] = tm_normalize_email(raw, email);
        cJSON_Delete(raw);
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
