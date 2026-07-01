/**
 * 1SecMail -- https://tmaily.com
 */

#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ONE_SEC_MAIL_BASE "https://tmaily.com/"

static const char *one_sec_mail_generate_headers[] = {
    "Accept: application/json",
    "User-Agent: Mozilla/5.0",
    NULL
};

/**
 * 从 Set-Cookie 字符串中提取 TMaily_sid
 */
static char *one_sec_mail_extract_cookie(const char *set_cookie) {
    if (!set_cookie || !set_cookie[0]) return NULL;
    const char *needle = "TMaily_sid=";
    const char *p = strstr(set_cookie, needle);
    if (!p) return NULL;
    const char *end = strpbrk(p, "; \t\r\n");
    size_t n = end ? (size_t)(end - p) : strlen(p);
    char *out = (char*)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, p, n);
    out[n] = '\0';
    return out;
}

tm_email_info_t* tm_provider_one_sec_mail_generate(void) {
    tm_http_response_t *resp = tm_http_request_ipv4(
        TM_HTTP_GET, ONE_SEC_MAIL_BASE "generate",
        one_sec_mail_generate_headers, NULL, 15
    );
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }
    char *cookie = one_sec_mail_extract_cookie(resp->cookies);
    if (!cookie || !cookie[0]) {
        free(cookie);
        tm_http_response_free(resp);
        return NULL;
    }

    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!root) {
        free(cookie);
        return NULL;
    }
    const char *address = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "address"), "");
    if (!address[0] || !strchr(address, '@')) {
        cJSON_Delete(root);
        free(cookie);
        return NULL;
    }
    tm_email_info_t *info = tm_email_info_new();
    if (!info) {
        cJSON_Delete(root);
        free(cookie);
        return NULL;
    }
    info->channel = CHANNEL_ONE_SEC_MAIL;
    info->email = tm_strdup(address);
    info->token = cookie;
    cJSON_Delete(root);
    return info;
}

tm_email_t* tm_provider_one_sec_mail_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    if (!token || !token[0] || !email || !email[0]) {
        return NULL;
    }

    size_t url_cap = strlen(ONE_SEC_MAIL_BASE) + strlen("emails?address=") + strlen(email) + 1;
    char *url = (char*)malloc(url_cap);
    if (!url) return NULL;
    snprintf(url, url_cap, "%semails?address=%s", ONE_SEC_MAIL_BASE, email);

    size_t cookie_cap = strlen(token) + 9;
    char *cookie_header = (char*)malloc(cookie_cap);
    if (!cookie_header) {
        free(url);
        return NULL;
    }
    snprintf(cookie_header, cookie_cap, "Cookie: %s", token);

    const char *headers[] = {
        "Accept: application/json",
        "User-Agent: Mozilla/5.0",
        cookie_header,
        NULL
    };

    tm_http_response_t *resp = tm_http_request_ipv4(TM_HTTP_GET, url, headers, NULL, 15);
    free(url);
    free(cookie_header);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }

    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!cJSON_IsArray(root)) {
        cJSON_Delete(root);
        return NULL;
    }

    int n = cJSON_GetArraySize(root);
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
        cJSON *item = cJSON_GetArrayItem(root, i);
        emails[i] = tm_normalize_email(item, email);
    }

    cJSON_Delete(root);
    return emails;
}
