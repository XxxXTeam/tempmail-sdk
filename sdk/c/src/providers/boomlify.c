/**
 * Boomlify — https://v1.boomlify.com
 */
#include "tempmail_internal.h"
#include <curl/curl.h>
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

static void bl_random_local(char *buf, int len) {
    static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    for (int i = 0; i < len; i++) buf[i] = chars[rand() % (int)(sizeof(chars) - 1)];
    buf[len] = '\0';
}

static int bl_json_active(const cJSON *obj) {
    const cJSON *a = cJSON_GetObjectItemCaseSensitive(obj, "is_active");
    if (!a) a = cJSON_GetObjectItemCaseSensitive(obj, "isActive");
    if (cJSON_IsNumber(a)) return a->valueint == 1 || (int)a->valuedouble == 1;
    if (cJSON_IsBool(a)) return cJSON_IsTrue(a);
    return 0;
}

tm_email_info_t* tm_provider_boomlify_generate(void) {
    srand((unsigned)time(NULL));
    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, BL_BASE "/domains/public", bl_headers, NULL, 15);
    if (!resp || resp->status < 200 || resp->status >= 300) { tm_http_response_free(resp); return NULL; }

    cJSON *arr = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!arr || !cJSON_IsArray(arr)) { cJSON_Delete(arr); return NULL; }

    char domains[32][256];
    int nd = 0;
    int n = cJSON_GetArraySize(arr);
    for (int i = 0; i < n && nd < 32; i++) {
        cJSON *d = cJSON_GetArrayItem(arr, i);
        if (!cJSON_IsObject(d)) continue;
        if (!bl_json_active(d)) continue;
        const char *dom = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(d, "domain"));
        if (dom && dom[0]) {
            strncpy(domains[nd], dom, sizeof(domains[0]) - 1);
            domains[nd][sizeof(domains[0]) - 1] = '\0';
            nd++;
        }
    }
    cJSON_Delete(arr);
    if (nd == 0) return NULL;

    char local[16];
    bl_random_local(local, 8);
    char address[320];
    snprintf(address, sizeof(address), "%s@%s", local, domains[rand() % nd]);

    tm_email_info_t *info = tm_email_info_new();
    info->channel = CHANNEL_BOOMLIFY;
    info->email = tm_strdup(address);
    return info;
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

tm_email_t* tm_provider_boomlify_get_emails(const char *email, int *count) {
    *count = -1;
    if (!email) return NULL;

    char *esc = bl_escape_path(email);
    char url[1024];
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
