/**
 * maildrop.cc 渠道实现 (GraphQL)
 */
#include "tempmail_internal.h"
#include <time.h>

#define MD_GQL "https://api.maildrop.cc/graphql"

static const char* md_headers[] = {
    "Content-Type: application/json",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
    NULL
};

static void md_random_username(char *buf, int len) {
    const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    srand((unsigned)time(NULL));
    for (int i = 0; i < len; i++) {
        buf[i] = chars[rand() % (sizeof(chars) - 1)];
    }
    buf[len] = '\0';
}

/* 发送 GraphQL 请求 */
static cJSON* md_graphql(const char *query) {
    cJSON *payload = cJSON_CreateObject();
    cJSON_AddStringToObject(payload, "query", query);
    char *body = cJSON_PrintUnformatted(payload);
    cJSON_Delete(payload);

    tm_http_response_t *resp = tm_http_request(TM_HTTP_POST, MD_GQL, md_headers, body, 15);
    free(body);
    if (!resp || resp->status != 200) { tm_http_response_free(resp); return NULL; }

    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) return NULL;

    cJSON *errors = cJSON_GetObjectItemCaseSensitive(json, "errors");
    if (cJSON_IsArray(errors) && cJSON_GetArraySize(errors) > 0) {
        cJSON_Delete(json);
        return NULL;
    }

    cJSON *data = cJSON_DetachItemFromObjectCaseSensitive(json, "data");
    cJSON_Delete(json);
    return data;
}

tm_email_info_t* tm_provider_maildrop_generate(void) {
    char username[11];
    md_random_username(username, 10);
    char email[64];
    snprintf(email, sizeof(email), "%s@maildrop.cc", username);

    /* 验证 API 可用 */
    char query[256];
    snprintf(query, sizeof(query), "{ inbox(mailbox: \"%s\") { id } }", username);
    cJSON *data = md_graphql(query);
    if (!data) return NULL;
    cJSON_Delete(data);

    tm_email_info_t *info = tm_email_info_new();
    info->channel = CHANNEL_MAILDROP;
    info->email = tm_strdup(email);
    info->token = tm_strdup(username);
    return info;
}

tm_email_t* tm_provider_maildrop_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    char query[512];
    snprintf(query, sizeof(query),
        "{ inbox(mailbox: \"%s\") { id headerfrom subject date } }", token);

    cJSON *data = md_graphql(query);
    if (!data) return NULL;

    cJSON *inbox = cJSON_GetObjectItemCaseSensitive(data, "inbox");
    int n = cJSON_IsArray(inbox) ? cJSON_GetArraySize(inbox) : 0;
    *count = 0;
    if (n == 0) { cJSON_Delete(data); return NULL; }

    tm_email_t *emails = tm_emails_new(n);
    int actual = 0;

    for (int i = 0; i < n; i++) {
        cJSON *item = cJSON_GetArrayItem(inbox, i);
        const char *id = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(item, "id"));
        if (!id) continue;

        /* 获取完整内容 */
        char msg_query[512];
        snprintf(msg_query, sizeof(msg_query),
            "{ message(mailbox: \"%s\", id: \"%s\") { id headerfrom subject date body html } }", token, id);
        cJSON *msg_data = md_graphql(msg_query);
        if (!msg_data) continue;

        cJSON *msg = cJSON_GetObjectItemCaseSensitive(msg_data, "message");
        if (msg) {
            /* 构建标准化邮件 */
            emails[actual].id = tm_strdup(cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(msg, "id")) ?: id);
            emails[actual].from_addr = tm_strdup(cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(msg, "headerfrom")) ?: "");
            emails[actual].to = tm_strdup(email);
            emails[actual].subject = tm_strdup(cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(msg, "subject")) ?: "");
            emails[actual].text = tm_strdup(cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(msg, "body")) ?: "");
            emails[actual].html = tm_strdup(cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(msg, "html")) ?: "");
            emails[actual].date = tm_strdup(cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(msg, "date")) ?: "");
            emails[actual].is_read = false;
            emails[actual].attachments = NULL;
            emails[actual].attachment_count = 0;
            actual++;
        }
        cJSON_Delete(msg_data);
    }

    cJSON_Delete(data);
    *count = actual;
    return emails;
}
