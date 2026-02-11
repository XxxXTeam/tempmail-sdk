/**
 * dropmail.me 渠道实现 (GraphQL)
 */
#include "tempmail_internal.h"

#define DM_BASE "https://dropmail.me/api/graphql/MY_TOKEN"

static const char* dm_headers[] = {
    "Content-Type: application/x-www-form-urlencoded",
    NULL
};

tm_email_info_t* tm_provider_dropmail_generate(void) {
    const char *body = "query=mutation%20%7BintroduceSession%20%7Bid%2C%20expiresAt%2C%20addresses%7Bid%2C%20address%7D%7D%7D";
    tm_http_response_t *resp = tm_http_request(TM_HTTP_POST, DM_BASE, dm_headers, body, 15);
    if (!resp || resp->status != 200) { tm_http_response_free(resp); return NULL; }

    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) return NULL;

    cJSON *data = cJSON_GetObjectItemCaseSensitive(json, "data");
    cJSON *session = cJSON_GetObjectItemCaseSensitive(data, "introduceSession");
    cJSON *addrs = cJSON_GetObjectItemCaseSensitive(session, "addresses");
    if (!cJSON_IsArray(addrs) || cJSON_GetArraySize(addrs) == 0) { cJSON_Delete(json); return NULL; }

    cJSON *first = cJSON_GetArrayItem(addrs, 0);
    tm_email_info_t *info = tm_email_info_new();
    info->channel = CHANNEL_DROPMAIL;
    info->email = tm_strdup(cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(first, "address")));
    info->token = tm_strdup(cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(session, "id")));
    cJSON_Delete(json);
    return info;
}

tm_email_t* tm_provider_dropmail_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    char body[1024];
    snprintf(body, sizeof(body),
        "query=query%%20(%%24id%%3A%%20ID!)%%20%%7Bsession(id%%3A%%24id)%%20%%7Bmails%%20%%7Bid%%2C%%20fromAddr%%2C%%20toAddr%%2C%%20receivedAt%%2C%%20text%%2C%%20headerSubject%%2C%%20html%%7D%%7D%%7D&variables=%%7B%%22id%%22%%3A%%22%s%%22%%7D",
        token);

    tm_http_response_t *resp = tm_http_request(TM_HTTP_POST, DM_BASE, dm_headers, body, 15);
    if (!resp || resp->status != 200) { tm_http_response_free(resp); return NULL; }

    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) return NULL;

    cJSON *data = cJSON_GetObjectItemCaseSensitive(json, "data");
    cJSON *session = cJSON_GetObjectItemCaseSensitive(data, "session");
    cJSON *mails = cJSON_GetObjectItemCaseSensitive(session, "mails");
    int n = cJSON_IsArray(mails) ? cJSON_GetArraySize(mails) : 0;
    *count = n;
    if (n == 0) { cJSON_Delete(json); return NULL; }

    tm_email_t *emails = tm_emails_new(n);
    for (int i = 0; i < n; i++) {
        cJSON *m = cJSON_GetArrayItem(mails, i);
        /* 扁平化字段名 */
        cJSON *flat = cJSON_CreateObject();
        cJSON_AddStringToObject(flat, "id", cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(m, "id")) ?: "");
        cJSON_AddStringToObject(flat, "from", cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(m, "fromAddr")) ?: "");
        cJSON_AddStringToObject(flat, "to", cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(m, "toAddr")) ?: email);
        cJSON_AddStringToObject(flat, "subject", cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(m, "headerSubject")) ?: "");
        cJSON_AddStringToObject(flat, "text", cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(m, "text")) ?: "");
        cJSON_AddStringToObject(flat, "html", cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(m, "html")) ?: "");
        cJSON_AddStringToObject(flat, "received_at", cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(m, "receivedAt")) ?: "");
        emails[i] = tm_normalize_email(flat, email);
        cJSON_Delete(flat);
    }
    cJSON_Delete(json);
    return emails;
}
