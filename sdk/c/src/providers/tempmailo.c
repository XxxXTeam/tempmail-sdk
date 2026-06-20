/**
 * Tempmailo -- https://tempmailo.com
 */

#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TMLO_BASE "https://tempmailo.com"

static const char *tmlo_home_headers[] = {
    "Accept: text/html,application/json,*/*;q=0.8",
    "Referer: https://tempmailo.com/",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    NULL
};

static char *tmlo_attr_value(const char *start, const char *end, const char *attr) {
    const char *p = start;
    size_t attr_len = strlen(attr);
    while (p && p < end) {
        p = strstr(p, attr);
        if (!p || p >= end) break;
        const char *v = p + attr_len;
        const char *q = strchr(v, '"');
        if (!q || q > end) return NULL;
        size_t len = (size_t)(q - v);
        char *out = (char*)malloc(len + 1);
        if (!out) return NULL;
        memcpy(out, v, len);
        out[len] = '\0';
        return out;
    }
    return NULL;
}

static char *tmlo_verification_token(const char *html) {
    const char *name = html ? strstr(html, "name=\"__RequestVerificationToken\"") : NULL;
    if (!name) return NULL;

    const char *start = name;
    while (start > html && *start != '<') start--;
    const char *end = strchr(name, '>');
    if (!end) end = name + strlen(name);

    return tmlo_attr_value(start, end, "value=\"");
}

static char *tmlo_encode_token(const char *verification_token, const char *cookie) {
    cJSON *root = cJSON_CreateObject();
    if (!root) return NULL;
    cJSON_AddStringToObject(root, "verificationToken", verification_token ? verification_token : "");
    cJSON_AddStringToObject(root, "cookie", cookie ? cookie : "");
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return out;
}

static int tmlo_decode_token(const char *token, char **verification_token, char **cookie) {
    *verification_token = NULL;
    *cookie = NULL;
    cJSON *root = cJSON_Parse(token);
    if (!root) return 0;
    const char *vt = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "verificationToken"), "");
    const char *ck = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "cookie"), "");
    if (!vt[0] || !ck[0]) {
        cJSON_Delete(root);
        return 0;
    }
    *verification_token = tm_strdup(vt);
    *cookie = tm_strdup(ck);
    cJSON_Delete(root);
    return *verification_token && *cookie;
}

tm_email_info_t* tm_provider_tempmailo_generate(void) {
    tm_http_response_t *home = tm_http_request(TM_HTTP_GET, TMLO_BASE "/", tmlo_home_headers, NULL, 15);
    if (!home || home->status < 200 || home->status >= 300) {
        tm_http_response_free(home);
        return NULL;
    }

    char *verification_token = tmlo_verification_token(home->body);
    char *cookie = tm_strdup(home->cookies ? home->cookies : "");
    tm_http_response_free(home);
    if (!verification_token || !cookie[0]) {
        free(verification_token);
        free(cookie);
        return NULL;
    }

    char cookie_line[4096];
    char token_line[512];
    snprintf(cookie_line, sizeof(cookie_line), "Cookie: %s", cookie);
    snprintf(token_line, sizeof(token_line), "RequestVerificationToken: %s", verification_token);
    const char *change_headers[] = {
        "Accept: text/plain,*/*;q=0.8",
        "Referer: https://tempmailo.com/",
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
        cookie_line,
        token_line,
        NULL
    };

    tm_http_response_t *changed = tm_http_request(TM_HTTP_GET, TMLO_BASE "/changemail?_r=1", change_headers, NULL, 15);
    if (!changed || changed->status < 200 || changed->status >= 300 || !changed->body || !strchr(changed->body, '@')) {
        tm_http_response_free(changed);
        free(verification_token);
        free(cookie);
        return NULL;
    }
    if (changed->cookies && changed->cookies[0]) {
        char *merged = (char*)malloc(strlen(cookie) + strlen(changed->cookies) + 3);
        if (merged) {
            sprintf(merged, "%s; %s", cookie, changed->cookies);
            free(cookie);
            cookie = merged;
        }
    }

    char *token = tmlo_encode_token(verification_token, cookie);
    tm_email_info_t *info = tm_email_info_new();
    if (!info || !token) {
        tm_http_response_free(changed);
        free(verification_token);
        free(cookie);
        free(token);
        if (info) tm_free_email_info(info);
        return NULL;
    }
    info->channel = CHANNEL_TEMPMAILO;
    info->email = tm_strdup(changed->body);
    info->token = token;

    tm_http_response_free(changed);
    free(verification_token);
    free(cookie);
    return info;
}

tm_email_t* tm_provider_tempmailo_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    if (!token || !email || !email[0]) return NULL;

    char *verification_token = NULL;
    char *cookie = NULL;
    if (!tmlo_decode_token(token, &verification_token, &cookie)) {
        free(verification_token);
        free(cookie);
        return NULL;
    }

    cJSON *body_json = cJSON_CreateObject();
    if (!body_json) {
        free(verification_token);
        free(cookie);
        return NULL;
    }
    cJSON_AddStringToObject(body_json, "mail", email);
    char *body = cJSON_PrintUnformatted(body_json);
    cJSON_Delete(body_json);
    if (!body) {
        free(verification_token);
        free(cookie);
        return NULL;
    }

    char cookie_line[4096];
    char token_line[512];
    snprintf(cookie_line, sizeof(cookie_line), "Cookie: %s", cookie);
    snprintf(token_line, sizeof(token_line), "RequestVerificationToken: %s", verification_token);
    const char *headers[] = {
        "Accept: application/json,*/*;q=0.8",
        "Content-Type: application/json",
        "Referer: https://tempmailo.com/",
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
        cookie_line,
        token_line,
        NULL
    };

    tm_http_response_t *resp = tm_http_request(TM_HTTP_POST, TMLO_BASE "/", headers, body, 15);
    free(body);
    free(verification_token);
    free(cookie);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }

    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!root) return NULL;
    if (!cJSON_IsArray(root)) {
        *count = 0;
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
        cJSON *raw = cJSON_GetArrayItem(root, i);
        emails[i] = tm_normalize_email(raw, email);
    }

    cJSON_Delete(root);
    return emails;
}
