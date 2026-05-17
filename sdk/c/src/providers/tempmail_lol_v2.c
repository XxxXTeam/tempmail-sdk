/**
 * tempmail.lol V2 渠道实现
 * API: https://api.tempmail.lol
 */
#include "tempmail_internal.h"

#define TLV2_BASE "https://api.tempmail.lol"

static const char* tlv2_headers[] = {
    "Accept: application/json",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
    NULL
};

tm_email_info_t* tm_provider_tempmail_lol_v2_generate(void) {
    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, TLV2_BASE "/generate", tlv2_headers, NULL, 15);
    if (!resp || resp->status != 200) { tm_http_response_free(resp); return NULL; }

    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) return NULL;

    const char *addr = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(json, "address"));
    const char *token = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(json, "token"));
    if (!addr || !token) { cJSON_Delete(json); return NULL; }

    tm_email_info_t *info = tm_email_info_new();
    info->channel = CHANNEL_TEMPMAIL_LOL_V2;
    info->email = tm_strdup(addr);
    info->token = tm_strdup(token);

    cJSON_Delete(json);
    return info;
}

tm_email_t* tm_provider_tempmail_lol_v2_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    char url[512];
    snprintf(url, sizeof(url), TLV2_BASE "/auth/%s", token);

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, tlv2_headers, NULL, 15);
    if (!resp || resp->status != 200) { tm_http_response_free(resp); return NULL; }

    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) return NULL;

    cJSON *list = cJSON_GetObjectItemCaseSensitive(json, "email");
    int n = cJSON_IsArray(list) ? cJSON_GetArraySize(list) : 0;
    *count = n;
    if (n == 0) { cJSON_Delete(json); return NULL; }

    tm_email_t *emails = tm_emails_new(n);
    for (int i = 0; i < n; i++) {
        cJSON *item = cJSON_GetArrayItem(list, i);
        cJSON *flat = cJSON_CreateObject();

        const char *id = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(item, "id"));
        if (!id) id = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(item, "_id"));
        cJSON_AddStringToObject(flat, "id", id ? id : "");

        const char *from = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(item, "from"));
        if (!from) from = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(item, "sender"));
        cJSON_AddStringToObject(flat, "from", from ? from : "");
        cJSON_AddStringToObject(flat, "to", email);

        cJSON_AddStringToObject(flat, "subject",
            TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(item, "subject"), ""));

        const char *body_str = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(item, "body"));
        const char *text_str = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(item, "text"));
        const char *html_str = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(item, "html"));

        cJSON_AddStringToObject(flat, "text", body_str ? body_str : (text_str ? text_str : ""));
        cJSON_AddStringToObject(flat, "html", html_str ? html_str : (body_str ? body_str : ""));

        const char *date = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(item, "date"));
        if (!date) date = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(item, "receivedAt"));
        cJSON_AddStringToObject(flat, "date", date ? date : "");

        cJSON_AddBoolToObject(flat, "isRead", 0);

        emails[i] = tm_normalize_email(flat, email);
        cJSON_Delete(flat);
    }
    cJSON_Delete(json);
    return emails;
}
