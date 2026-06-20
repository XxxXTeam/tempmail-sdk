/**
 * Guerrillamail 镜像渠道实现
 * sharklasers.com / grr.la / guerrillamail.info 共用同一套 API，仅 baseURL 不同
 */
#include "tempmail_internal.h"
#include <ctype.h>

static const char* gm_mirror_headers[] = {
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36",
    NULL
};

static char *gm_mirror_strip_html(const char *html) {
    if (!html) return tm_strdup("");
    size_t len = strlen(html);
    char *out = (char*)malloc(len + 1);
    if (!out) return NULL;
    int in_tag = 0;
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        char c = html[i];
        if (c == '<') { in_tag = 1; out[j++] = ' '; continue; }
        if (c == '>') { in_tag = 0; out[j++] = ' '; continue; }
        if (!in_tag) out[j++] = c;
    }
    out[j] = '\0';
    char *w = out;
    char *r = out;
    int last_space = 1;
    while (*r) {
        unsigned char c = (unsigned char)*r++;
        if (isspace(c)) {
            if (!last_space) *w++ = ' ';
            last_space = 1;
        } else {
            *w++ = (char)c;
            last_space = 0;
        }
    }
    if (w > out && w[-1] == ' ') w--;
    *w = '\0';
    return out;
}

static char *gm_mirror_fetch_body(const char *base_url, const char *token, const cJSON *mid) {
    if (!mid) return NULL;
    char id_buf[64];
    const char *id = cJSON_GetStringValue((cJSON*)mid);
    if (!id && cJSON_IsNumber(mid)) {
        snprintf(id_buf, sizeof(id_buf), "%.0f", mid->valuedouble);
        id = id_buf;
    }
    if (!id || !id[0]) return NULL;
    char url[768];
    snprintf(url, sizeof(url), "%s?f=fetch_email&sid_token=%s&email_id=%s", base_url, token, id);
    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, gm_mirror_headers, NULL, 15);
    if (!resp || resp->status != 200) { tm_http_response_free(resp); return NULL; }
    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) return NULL;
    const char *body = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(json, "mail_body"));
    char *out = body && body[0] ? tm_strdup(body) : NULL;
    cJSON_Delete(json);
    return out;
}

tm_email_info_t* tm_provider_guerrillamail_mirror_generate(tm_channel_t channel, const char *base_url) {
    char url[512];
    snprintf(url, sizeof(url), "%s?f=get_email_address&lang=en", base_url);

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, gm_mirror_headers, NULL, 15);
    if (!resp || resp->status != 200) { tm_http_response_free(resp); return NULL; }

    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) return NULL;

    const char *addr = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(json, "email_addr"));
    const char *sid = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(json, "sid_token"));
    if (!addr || !sid) { cJSON_Delete(json); return NULL; }

    tm_email_info_t *info = tm_email_info_new();
    info->channel = channel;
    info->email = tm_strdup(addr);
    info->token = tm_strdup(sid);

    cJSON *ts = cJSON_GetObjectItemCaseSensitive(json, "email_timestamp");
    if (ts && cJSON_IsNumber(ts)) {
        info->expires_at = ((long long)ts->valuedouble + 3600) * 1000;
    }

    cJSON_Delete(json);
    return info;
}

tm_email_t* tm_provider_guerrillamail_mirror_get_emails(const char *base_url, const char *token, const char *email, int *count) {
    *count = -1;
    char url[512];
    snprintf(url, sizeof(url), "%s?f=check_email&seq=0&sid_token=%s", base_url, token);

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, gm_mirror_headers, NULL, 15);
    if (!resp || resp->status != 200) { tm_http_response_free(resp); return NULL; }

    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) return NULL;

    cJSON *list = cJSON_GetObjectItemCaseSensitive(json, "list");
    int n = cJSON_IsArray(list) ? cJSON_GetArraySize(list) : 0;
    *count = n;
    if (n == 0) { cJSON_Delete(json); return NULL; }

    tm_email_t *emails = tm_emails_new(n);
    for (int i = 0; i < n; i++) {
        cJSON *item = cJSON_GetArrayItem(list, i);
        cJSON *flat = cJSON_CreateObject();
        cJSON *mid = cJSON_GetObjectItemCaseSensitive(item, "mail_id");
        if (mid) cJSON_AddNumberToObject(flat, "id", mid->valuedouble);
        cJSON_AddStringToObject(flat, "from", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(item, "mail_from"), ""));
        cJSON_AddStringToObject(flat, "to", email);
        cJSON_AddStringToObject(flat, "subject", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(item, "mail_subject"), ""));
        char *body_owned = NULL;
        const char *body_str = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(item, "mail_body"));
        if (!body_str || !body_str[0]) {
            body_owned = gm_mirror_fetch_body(base_url, token, mid);
            body_str = body_owned;
        }
        if (body_str && body_str[0]) {
            char *text = gm_mirror_strip_html(body_str);
            cJSON_AddStringToObject(flat, "text", text ? text : "");
            cJSON_AddStringToObject(flat, "html", body_str);
            free(text);
        } else {
            cJSON_AddStringToObject(flat, "text", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(item, "mail_excerpt"), ""));
        }
        free(body_owned);
        cJSON_AddStringToObject(flat, "date", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(item, "mail_date"), ""));
        cJSON *mr = cJSON_GetObjectItemCaseSensitive(item, "mail_read");
        if (mr) cJSON_AddBoolToObject(flat, "isRead", mr->valueint == 1);

        emails[i] = tm_normalize_email(flat, email);
        cJSON_Delete(flat);
    }
    cJSON_Delete(json);
    return emails;
}
