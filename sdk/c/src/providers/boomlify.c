/**
 * Boomlify — https://v1.boomlify.com
 * 先 POST /emails/public/create 登记收件箱，地址为 inboxId@域名。
 */
#include "tempmail_internal.h"
#include <curl/curl.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BL_BASE "https://v1.boomlify.com"

static const char *bl_headers[] = {
    "Accept: application/json, text/plain, */*",
    "Accept-Language: zh",
    "Origin: https://boomlify.com",
    "Referer: https://boomlify.com/",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    "sec-ch-ua: \"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"146\"",
    "sec-ch-ua-mobile: ?0",
    "sec-ch-ua-platform: \"Windows\"",
    "sec-fetch-dest: empty",
    "sec-fetch-mode: cors",
    "sec-fetch-site: same-site",
    "x-user-language: zh",
    NULL
};

static const char *bl_post_headers[] = {
    "Accept: application/json, text/plain, */*",
    "Content-Type: application/json",
    "Accept-Language: zh",
    "Origin: https://boomlify.com",
    "Referer: https://boomlify.com/",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    "sec-ch-ua: \"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"146\"",
    "sec-ch-ua-mobile: ?0",
    "sec-ch-ua-platform: \"Windows\"",
    "sec-fetch-dest: empty",
    "sec-fetch-mode: cors",
    "sec-fetch-site: same-site",
    "x-user-language: zh",
    NULL
};

static int bl_json_active(const cJSON *obj) {
    const cJSON *a = cJSON_GetObjectItemCaseSensitive(obj, "is_active");
    if (!a) a = cJSON_GetObjectItemCaseSensitive(obj, "isActive");
    if (cJSON_IsNumber(a)) return a->valueint == 1 || (int)a->valuedouble == 1;
    if (cJSON_IsBool(a)) return cJSON_IsTrue(a);
    return 0;
}

static int bl_hexnybble(int c) {
    if (c >= '0' && c <= '9') return 1;
    if (c >= 'a' && c <= 'f') return 1;
    if (c >= 'A' && c <= 'F') return 1;
    return 0;
}

/* RFC inbox UUID 本地部：8-4-4-4-12 */
static int bl_local_is_inbox_uuid(const char *local) {
    if (!local) return 0;
    size_t n = strlen(local);
    if (n != 36) return 0;
    if (local[8] != '-' || local[13] != '-' || local[18] != '-' || local[23] != '-') return 0;
    for (size_t i = 0; i < n; i++) {
        if (local[i] == '-') continue;
        if (!bl_hexnybble((unsigned char)local[i])) return 0;
    }
    return 1;
}

static void bl_inbox_path_segment(const char *email, char *out, size_t cap) {
    if (!email || cap < 2) {
        if (cap) out[0] = '\0';
        return;
    }
    const char *at = strrchr(email, '@');
    if (at && at > email) {
        size_t ll = (size_t)(at - email);
        size_t cpy = ll < cap - 1 ? ll : cap - 1;
        memcpy(out, email, cpy);
        out[cpy] = '\0';
        if (bl_local_is_inbox_uuid(out)) return;
    }
    strncpy(out, email, cap - 1);
    out[cap - 1] = '\0';
}

static char *bl_escape_path(const char *email) {
    CURL *c = curl_easy_init();
    if (!c) return tm_strdup(email);
    char *e = curl_easy_escape(c, email, 0);
    curl_easy_cleanup(c);
    if (!e) return tm_strdup(email);
    char *d = tm_strdup(e);
    curl_free(e);
    return d;
}

tm_email_info_t* tm_provider_boomlify_generate(void) {
    srand((unsigned)time(NULL));
    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, BL_BASE "/domains/public", bl_headers, NULL, 15);
    if (!resp || resp->status < 200 || resp->status >= 300) { tm_http_response_free(resp); return NULL; }

    cJSON *arr = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!arr || !cJSON_IsArray(arr)) { cJSON_Delete(arr); return NULL; }

    char did[64][48];
    char domains[64][256];
    int nd = 0;
    int n = cJSON_GetArraySize(arr);
    for (int i = 0; i < n && nd < 64; i++) {
        cJSON *d = cJSON_GetArrayItem(arr, i);
        if (!cJSON_IsObject(d)) continue;
        if (!bl_json_active(d)) continue;
        const char *id = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(d, "id"));
        const char *dom = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(d, "domain"));
        if (id && id[0] && dom && dom[0]) {
            strncpy(did[nd], id, sizeof(did[0]) - 1);
            did[nd][sizeof(did[0]) - 1] = '\0';
            strncpy(domains[nd], dom, sizeof(domains[0]) - 1);
            domains[nd][sizeof(domains[0]) - 1] = '\0';
            nd++;
        }
    }
    cJSON_Delete(arr);
    if (nd == 0) return NULL;

    int pick = rand() % nd;
    cJSON *payload = cJSON_CreateObject();
    cJSON_AddStringToObject(payload, "domainId", did[pick]);
    char *body = cJSON_PrintUnformatted(payload);
    cJSON_Delete(payload);
    if (!body) return NULL;

    tm_http_response_t *r2 = tm_http_request(TM_HTTP_POST, BL_BASE "/emails/public/create", bl_post_headers, body, 15);
    cJSON_free(body);
    if (!r2 || r2->status < 200 || r2->status >= 300) { tm_http_response_free(r2); return NULL; }

    cJSON *cj = cJSON_Parse(r2->body);
    tm_http_response_free(r2);
    if (!cj) return NULL;
    const char *err = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(cj, "error"));
    if (err && err[0]) {
        cJSON_Delete(cj);
        return NULL;
    }
    const char *box = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(cj, "id"));
    if (!box || !box[0]) { cJSON_Delete(cj); return NULL; }

    char address[400];
    snprintf(address, sizeof(address), "%s@%s", box, domains[pick]);

    tm_email_info_t *info = tm_email_info_new();
    info->channel = CHANNEL_BOOMLIFY;
    info->email = tm_strdup(address);
    info->token = tm_strdup(box);
    cJSON_Delete(cj);
    return info;
}

tm_email_t* tm_provider_boomlify_get_emails(const char *email, int *count) {
    *count = -1;
    if (!email) return NULL;

    char seg[384];
    bl_inbox_path_segment(email, seg, sizeof(seg));
    char *esc = bl_escape_path(seg);
    char url[1200];
    snprintf(url, sizeof(url), "%s/emails/public/%s", BL_BASE, esc);
    free(esc);

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, bl_headers, NULL, 15);
    if (!resp || resp->status < 200 || resp->status >= 300) { tm_http_response_free(resp); return NULL; }

    cJSON *arr = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!arr || !cJSON_IsArray(arr)) { cJSON_Delete(arr); return NULL; }

    int n = cJSON_GetArraySize(arr);
    *count = n;
    if (n == 0) { cJSON_Delete(arr); return NULL; }

    tm_email_t *emails = tm_emails_new(n);
    for (int i = 0; i < n; i++) {
        cJSON *raw = cJSON_GetArrayItem(arr, i);
        const char *from = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(raw, "from_email"));
        if (!from || !from[0])
            from = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(raw, "from_name"));
        cJSON *one = cJSON_CreateObject();
#define BL_S(f) (cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(raw, f)))
        cJSON_AddStringToObject(one, "id", BL_S("id") ? BL_S("id") : "");
        cJSON_AddStringToObject(one, "from", from ? from : "");
        cJSON_AddStringToObject(one, "to", email);
        cJSON_AddStringToObject(one, "subject", BL_S("subject") ? BL_S("subject") : "");
        cJSON_AddStringToObject(one, "text", BL_S("body_text") ? BL_S("body_text") : "");
        cJSON_AddStringToObject(one, "html", BL_S("body_html") ? BL_S("body_html") : "");
        cJSON_AddStringToObject(one, "received_at", BL_S("received_at") ? BL_S("received_at") : "");
#undef BL_S
        cJSON_AddBoolToObject(one, "isRead", 0);
        emails[i] = tm_normalize_email(one, email);
        cJSON_Delete(one);
    }
    cJSON_Delete(arr);
    return emails;
}
