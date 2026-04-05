/**
 * 10mail.wangtz.cn（默认跳过 TLS 证书校验）
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <string.h>
#define TM10_STRCMP_ICASE _stricmp
#else
#include <strings.h>
#define TM10_STRCMP_ICASE strcasecmp
#endif

#define TM10_ORIGIN "https://10mail.wangtz.cn"
#define TM10_MAIL_DOMAIN "wangtz.cn"

static const char *tm10_json_headers[] = {
    "Accept: */*",
    "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8",
    "Content-Type: application/json; charset=utf-8",
    "Origin: " TM10_ORIGIN,
    "Referer: " TM10_ORIGIN "/",
    "token: null",
    "Authorization: null",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    NULL
};

static void tm10_local_part(const char *domain, char *out, size_t cap) {
    if (!out || cap == 0) return;
    out[0] = '\0';
    if (!domain) return;
    while (*domain == ' ' || *domain == '\t') domain++;
    const char *at = strchr(domain, '@');
    if (at) {
        size_t n = (size_t)(at - domain);
        if (n >= cap) n = cap - 1;
        memcpy(out, domain, n);
        out[n] = '\0';
    } else {
        strncpy(out, domain, cap - 1);
        out[cap - 1] = '\0';
    }
}

/* mailparser 风格 { text, value:[{address,name}] } → 一行地址字符串 */
static char *tm10_format_addr_obj(const cJSON *addr_obj) {
    if (!addr_obj || !cJSON_IsObject(addr_obj)) return NULL;
    cJSON *text = cJSON_GetObjectItemCaseSensitive(addr_obj, "text");
    if (cJSON_IsString(text) && text->valuestring && text->valuestring[0])
        return tm_strdup(text->valuestring);
    cJSON *val = cJSON_GetObjectItemCaseSensitive(addr_obj, "value");
    if (!cJSON_IsArray(val) || cJSON_GetArraySize(val) < 1) return NULL;
    cJSON *first = cJSON_GetArrayItem(val, 0);
    if (!cJSON_IsObject(first)) return NULL;
    const char *addr = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(first, "address"));
    if (!addr || !addr[0]) return NULL;
    const char *nm = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(first, "name"));
    if (nm && nm[0] && TM10_STRCMP_ICASE(nm, addr) != 0) {
        size_t n = strlen(nm) + 4 + strlen(addr);
        char *buf = (char *)malloc(n);
        if (!buf) return tm_strdup(addr);
        snprintf(buf, n, "%s <%s>", nm, addr);
        return buf;
    }
    return tm_strdup(addr);
}

static void tm10_replace_addr_field(cJSON *flat, const char *key) {
    cJSON *obj = cJSON_GetObjectItemCaseSensitive(flat, key);
    char *s = tm10_format_addr_obj(obj);
    if (s) {
        cJSON_DeleteItemFromObjectCaseSensitive(flat, key);
        cJSON_AddStringToObject(flat, key, s);
        free(s);
    }
}

static cJSON *tm10_flatten_item(cJSON *item) {
    if (!item) return NULL;
    cJSON *flat = cJSON_Duplicate(item, 1);
    if (!flat) return NULL;
    cJSON *mid = cJSON_GetObjectItemCaseSensitive(flat, "messageId");
    if (mid) {
        cJSON_DeleteItemFromObjectCaseSensitive(flat, "id");
        cJSON_AddItemToObject(flat, "id", cJSON_Duplicate(mid, 1));
    }
    tm10_replace_addr_field(flat, "from");
    tm10_replace_addr_field(flat, "to");
    return flat;
}

tm_email_info_t *tm_provider_tenmail_wangtz_generate(const char *domain) {
    char local[256];
    tm10_local_part(domain, local, sizeof(local));

    cJSON *root = cJSON_CreateObject();
    if (!root) return NULL;
    cJSON_AddStringToObject(root, "emailName", local);
    char *body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!body) return NULL;

    tm_http_response_t *resp = tm_http_request_skip_cert_verify(
        TM_HTTP_POST, TM10_ORIGIN "/api/tempMail", tm10_json_headers, body, 15);
    free(body);

    if (!resp || resp->status != 200) {
        tm_http_response_free(resp);
        return NULL;
    }

    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) return NULL;

    cJSON *code = cJSON_GetObjectItemCaseSensitive(json, "code");
    int ok = 0;
    if (cJSON_IsNumber(code) && code->valuedouble == 0.0) ok = 1;
    if (!ok) {
        cJSON_Delete(json);
        return NULL;
    }

    cJSON *data = cJSON_GetObjectItemCaseSensitive(json, "data");
    if (!cJSON_IsObject(data)) {
        cJSON_Delete(json);
        return NULL;
    }
    const char *mail_name = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(data, "mailName"));
    if (!mail_name || mail_name[0] == '\0') {
        cJSON_Delete(json);
        return NULL;
    }

    tm_email_info_t *info = tm_email_info_new();
    if (!info) {
        cJSON_Delete(json);
        return NULL;
    }
    info->channel = CHANNEL_10MAIL_WANGTZ;
    size_t elen = strlen(mail_name) + 1 + strlen(TM10_MAIL_DOMAIN) + 1;
    info->email = (char *)malloc(elen);
    if (!info->email) {
        free(info);
        cJSON_Delete(json);
        return NULL;
    }
    snprintf(info->email, elen, "%s@%s", mail_name, TM10_MAIL_DOMAIN);

    cJSON *end = cJSON_GetObjectItemCaseSensitive(data, "endTime");
    if (cJSON_IsNumber(end)) {
        info->expires_at = (long long)end->valuedouble;
    }

    cJSON_Delete(json);
    return info;
}

tm_email_t *tm_provider_tenmail_wangtz_get_emails(const char *email, int *count) {
    *count = -1;
    if (!email) return NULL;

    cJSON *root = cJSON_CreateObject();
    if (!root) return NULL;
    cJSON_AddStringToObject(root, "emailName", email);
    char *body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!body) return NULL;

    tm_http_response_t *resp = tm_http_request_skip_cert_verify(
        TM_HTTP_POST, TM10_ORIGIN "/api/emailList", tm10_json_headers, body, 15);
    free(body);

    if (!resp || resp->status != 200) {
        tm_http_response_free(resp);
        return NULL;
    }

    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) return NULL;

    if (!cJSON_IsArray(json)) {
        cJSON_Delete(json);
        return NULL;
    }

    int n = cJSON_GetArraySize(json);
    *count = n;
    if (n == 0) {
        cJSON_Delete(json);
        return NULL;
    }

    tm_email_t *emails = tm_emails_new(n);
    if (!emails) {
        cJSON_Delete(json);
        *count = -1;
        return NULL;
    }

    for (int i = 0; i < n; i++) {
        cJSON *raw = cJSON_GetArrayItem(json, i);
        cJSON *flat = tm10_flatten_item(raw);
        if (flat) {
            emails[i] = tm_normalize_email(flat, email);
            cJSON_Delete(flat);
        } else {
            emails[i] = tm_normalize_email(raw, email);
        }
    }
    cJSON_Delete(json);
    return emails;
}
