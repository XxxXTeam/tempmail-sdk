/**
 * 1SecMail -- https://1sec-mail.com
 */

#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ONE_SEC_MAIL_BASE "https://1sec-mail.com/"

static const char *one_sec_mail_home_headers[] = {
    "Accept: text/html,application/xhtml+xml",
    "User-Agent: Mozilla/5.0",
    NULL
};

static char *one_sec_mail_cookie_value(const char *line) {
    if (!line) return NULL;
    const char *end = strchr(line, ';');
    size_t n = end ? (size_t)(end - line) : strlen(line);
    char *out = (char*)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, line, n);
    out[n] = '\0';
    return out;
}

static char *one_sec_mail_merge_cookie(const char *prev, const char *set_cookie) {
    if (!set_cookie || !set_cookie[0]) return tm_strdup(prev ? prev : "");
    char *first = one_sec_mail_cookie_value(set_cookie);
    if (!first) return tm_strdup(prev ? prev : "");
    if (!prev || !prev[0]) return first;
    size_t cap = strlen(prev) + strlen(first) + 3;
    char *out = (char*)malloc(cap);
    if (!out) {
        free(first);
        return NULL;
    }
    snprintf(out, cap, "%s; %s", prev, first);
    free(first);
    return out;
}

static char *one_sec_mail_csrf(const char *html) {
    const char *needle = "<meta name=\"csrf-token\" content=\"";
    const char *p = html ? strstr(html, needle) : NULL;
    if (!p) return tm_strdup("");
    p += strlen(needle);
    const char *end = strchr(p, '"');
    if (!end) return tm_strdup("");
    size_t n = (size_t)(end - p);
    char *out = (char*)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, p, n);
    out[n] = '\0';
    return out;
}

static char *one_sec_mail_json_escape(const char *s) {
    cJSON *item = cJSON_CreateString(s ? s : "");
    if (!item) return NULL;
    char *printed = cJSON_PrintUnformatted(item);
    cJSON_Delete(item);
    return printed;
}

static char *one_sec_mail_encode_session(const char *csrf, const char *cookie) {
    cJSON *root = cJSON_CreateObject();
    if (!root) return NULL;
    cJSON_AddStringToObject(root, "csrf", csrf ? csrf : "");
    cJSON_AddStringToObject(root, "cookie", cookie ? cookie : "");
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return out;
}

static int one_sec_mail_decode_session(const char *token, char **csrf, char **cookie) {
    *csrf = NULL;
    *cookie = NULL;
    cJSON *root = cJSON_Parse(token ? token : "");
    if (!root) return 0;
    const char *csrf_s = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "csrf"), "");
    const char *cookie_s = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "cookie"), "");
    if (!csrf_s[0] || !cookie_s[0]) {
        cJSON_Delete(root);
        return 0;
    }
    *csrf = tm_strdup(csrf_s);
    *cookie = tm_strdup(cookie_s);
    cJSON_Delete(root);
    return *csrf && *cookie;
}

static cJSON *one_sec_mail_fetch_messages(const char *csrf, const char *cookie, char **new_cookie) {
    *new_cookie = NULL;
    char *csrf_json = one_sec_mail_json_escape(csrf);
    if (!csrf_json) return NULL;
    size_t body_cap = strlen(csrf_json) + 28;
    char *body = (char*)malloc(body_cap);
    if (!body) {
        free(csrf_json);
        return NULL;
    }
    snprintf(body, body_cap, "{\"_token\":%s,\"captcha\":\"\"}", csrf_json);
    free(csrf_json);

    size_t csrf_cap = strlen(csrf) + 16;
    size_t cookie_cap = strlen(cookie) + 9;
    char *csrf_header = (char*)malloc(csrf_cap);
    char *cookie_header = (char*)malloc(cookie_cap);
    if (!csrf_header || !cookie_header) {
        free(csrf_header);
        free(cookie_header);
        free(body);
        return NULL;
    }
    snprintf(csrf_header, csrf_cap, "X-CSRF-TOKEN: %s", csrf);
    snprintf(cookie_header, cookie_cap, "Cookie: %s", cookie);
    const char *headers[] = {
        "Accept: application/json, text/plain, */*",
        "Content-Type: application/json",
        "X-Requested-With: XMLHttpRequest",
        csrf_header,
        "Referer: https://1sec-mail.com/",
        cookie_header,
        "User-Agent: Mozilla/5.0",
        NULL
    };
    tm_http_response_t *resp = tm_http_request_ipv4(TM_HTTP_POST, ONE_SEC_MAIL_BASE "get_messages", headers, body, 15);
    free(csrf_header);
    free(cookie_header);
    free(body);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }
    *new_cookie = one_sec_mail_merge_cookie(cookie, resp->cookies);
    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    return root;
}

tm_email_info_t* tm_provider_one_sec_mail_generate(void) {
    tm_http_response_t *home = tm_http_request_ipv4(TM_HTTP_GET, ONE_SEC_MAIL_BASE, one_sec_mail_home_headers, NULL, 15);
    if (!home || home->status < 200 || home->status >= 300) {
        tm_http_response_free(home);
        return NULL;
    }
    char *csrf = one_sec_mail_csrf(home->body);
    char *cookie = one_sec_mail_merge_cookie("", home->cookies);
    tm_http_response_free(home);
    if (!csrf || !csrf[0] || !cookie || !cookie[0]) {
        free(csrf);
        free(cookie);
        return NULL;
    }

    char *new_cookie = NULL;
    cJSON *root = one_sec_mail_fetch_messages(csrf, cookie, &new_cookie);
    if (!root) {
        free(csrf);
        free(cookie);
        return NULL;
    }
    const char *mailbox = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "mailbox"), "");
    if (!mailbox[0] || !strchr(mailbox, '@')) {
        cJSON_Delete(root);
        free(csrf);
        free(cookie);
        free(new_cookie);
        return NULL;
    }
    tm_email_info_t *info = tm_email_info_new();
    if (!info) {
        cJSON_Delete(root);
        free(csrf);
        free(cookie);
        free(new_cookie);
        return NULL;
    }
    info->channel = CHANNEL_ONE_SEC_MAIL;
    info->email = tm_strdup(mailbox);
    info->token = one_sec_mail_encode_session(csrf, new_cookie ? new_cookie : cookie);
    cJSON_Delete(root);
    free(csrf);
    free(cookie);
    free(new_cookie);
    return info;
}

static cJSON *one_sec_mail_message_raw(const cJSON *msg, const char *recipient) {
    cJSON *raw = cJSON_Duplicate((cJSON*)msg, 1);
    if (!raw) raw = cJSON_CreateObject();
    if (!raw) return NULL;
    const cJSON *content = cJSON_GetObjectItemCaseSensitive((cJSON*)msg, "content");
    const cJSON *html_flag = cJSON_GetObjectItemCaseSensitive((cJSON*)msg, "html");
    const char *content_s = TM_JSON_STR(content, "");
    cJSON_ReplaceItemInObject(raw, "id", cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive((cJSON*)msg, "id"), 1));
    cJSON_ReplaceItemInObject(raw, "from", cJSON_CreateString(TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON*)msg, "from_email"), TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON*)msg, "from"), ""))));
    if (!cJSON_GetObjectItemCaseSensitive(raw, "to")) cJSON_AddStringToObject(raw, "to", recipient);
    if (cJSON_IsBool(html_flag) && cJSON_IsTrue(html_flag)) {
        cJSON_ReplaceItemInObject(raw, "text", cJSON_CreateString(""));
        cJSON_ReplaceItemInObject(raw, "html", cJSON_CreateString(content_s));
    } else {
        cJSON_ReplaceItemInObject(raw, "text", cJSON_CreateString(content_s));
        if (cJSON_IsString(html_flag)) cJSON_ReplaceItemInObject(raw, "html", cJSON_Duplicate((cJSON*)html_flag, 1));
    }
    cJSON_ReplaceItemInObject(raw, "date", cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive((cJSON*)msg, "receivedAt"), 1));
    cJSON_ReplaceItemInObject(raw, "isRead", cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive((cJSON*)msg, "is_seen"), 1));
    return raw;
}

tm_email_t* tm_provider_one_sec_mail_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    char *csrf = NULL;
    char *cookie = NULL;
    if (!one_sec_mail_decode_session(token, &csrf, &cookie)) {
        free(csrf);
        free(cookie);
        return NULL;
    }
    char *new_cookie = NULL;
    cJSON *root = one_sec_mail_fetch_messages(csrf, cookie, &new_cookie);
    free(csrf);
    free(cookie);
    free(new_cookie);
    if (!root) return NULL;
    cJSON *messages = cJSON_GetObjectItemCaseSensitive(root, "messages");
    if (!cJSON_IsArray(messages)) {
        cJSON_Delete(root);
        return NULL;
    }
    int n = cJSON_GetArraySize(messages);
    *count = n;
    if (n == 0) {
        cJSON_Delete(root);
        return NULL;
    }
    tm_email_t *emails = tm_emails_new(n);
    if (!emails) {
        *count = -1;
        cJSON_Delete(root);
        return NULL;
    }
    for (int i = 0; i < n; i++) {
        cJSON *item = cJSON_GetArrayItem(messages, i);
        cJSON *raw = one_sec_mail_message_raw(item, email);
        emails[i] = tm_normalize_email(raw ? raw : item, email);
        if (raw) cJSON_Delete(raw);
    }
    cJSON_Delete(root);
    return emails;
}
