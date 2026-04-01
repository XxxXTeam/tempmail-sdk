/**
 * mail.cx 渠道实现（公开 REST API，见 https://docs.mail.cx）
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <time.h>
#include <string.h>

#ifdef _WIN32
#define MCX_STRICMP _stricmp
#else
#include <strings.h>
#define MCX_STRICMP strcasecmp
#endif

#define MCX_BASE "https://api.mail.cx"

static const char *mcx_headers_get[] = {
    "Accept: application/json",
    "Origin: https://mail.cx",
    "Referer: https://mail.cx/",
    NULL
};

static void random_string(char *buf, int len) {
    const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    for (int i = 0; i < len; i++) {
        buf[i] = chars[rand() % (int)(sizeof(chars) - 1)];
    }
    buf[len] = '\0';
}

/* 释放 cJSON 字符串数组 */
static void free_domain_list(char **domains, int n) {
    if (!domains) return;
    for (int i = 0; i < n; i++) free(domains[i]);
    free(domains);
}

/* 返回 0 成功，*domains / *dn 由调用者 free_domain_list 释放 */
static int mcx_fetch_domains(char ***out_domains, int *out_n) {
    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, MCX_BASE "/api/domains", mcx_headers_get, NULL, 15);
    if (!resp || resp->status != 200) {
        tm_http_response_free(resp);
        return -1;
    }
    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) return -1;

    cJSON *arr = cJSON_GetObjectItemCaseSensitive(json, "domains");
    if (!cJSON_IsArray(arr)) {
        cJSON_Delete(json);
        return -1;
    }

    int n = cJSON_GetArraySize(arr);
    char **domains = (char **)calloc((size_t)(n > 0 ? n : 1), sizeof(char *));
    if (!domains) {
        cJSON_Delete(json);
        return -1;
    }
    int k = 0;
    for (int i = 0; i < n; i++) {
        cJSON *d = cJSON_GetArrayItem(arr, i);
        cJSON *dom = cJSON_GetObjectItemCaseSensitive(d, "domain");
        if (cJSON_IsString(dom) && dom->valuestring && dom->valuestring[0]) {
            domains[k++] = tm_strdup(dom->valuestring);
        }
    }
    cJSON_Delete(json);
    if (k == 0) {
        free(domains);
        return -1;
    }
    *out_domains = domains;
    *out_n = k;
    return 0;
}

static int domain_in_list(const char *want, char **domains, int n) {
    if (!want || !want[0]) return 0;
    for (int i = 0; i < n; i++) {
        if (domains[i] && MCX_STRICMP(domains[i], want) == 0) return 1;
    }
    return 0;
}

tm_email_info_t *tm_provider_mail_cx_generate(const char *preferred_domain) {
    srand((unsigned)time(NULL));

    char **domains = NULL;
    int dn = 0;
    if (mcx_fetch_domains(&domains, &dn) != 0) return NULL;

    /* 可选：仅使用 preferred_domain */
    if (preferred_domain && preferred_domain[0]) {
        char want[256];
        const char *p = preferred_domain;
        if (*p == '@') p++;
        snprintf(want, sizeof(want), "%s", p);
        if (!domain_in_list(want, domains, dn)) {
            /* 不在列表则忽略，仍用全部域名 */
        } else {
            for (int i = 0; i < dn; i++) {
                if (domains[i] && MCX_STRICMP(domains[i], want) == 0) {
                    char *one = tm_strdup(domains[i]);
                    free_domain_list(domains, dn);
                    domains = (char **)malloc(sizeof(char *));
                    if (!domains) return NULL;
                    domains[0] = one;
                    dn = 1;
                    break;
                }
            }
        }
    }

    tm_email_info_t *result = NULL;
    for (int attempt = 0; attempt < 8 && !result; attempt++) {
        char username[13], password[17];
        random_string(username, 12);
        random_string(password, 16);
        char *dom = domains[rand() % dn];
        char address[320];
        snprintf(address, sizeof(address), "%s@%s", username, dom);

        char body[512];
        snprintf(body, sizeof(body), "{\"address\":\"%s\",\"password\":\"%s\"}", address, password);

        const char *post_headers[] = {
            "Content-Type: application/json",
            "Accept: application/json",
            "Origin: https://mail.cx",
            "Referer: https://mail.cx/",
            NULL
        };

        tm_http_response_t *resp = tm_http_request(TM_HTTP_POST, MCX_BASE "/api/accounts", post_headers, body, 15);
        if (!resp) continue;

        if (resp->status == 409) {
            tm_http_response_free(resp);
            continue;
        }
        if (resp->status != 201) {
            tm_http_response_free(resp);
            if (attempt == 7) break;
            continue;
        }

        cJSON *acc = cJSON_Parse(resp->body);
        tm_http_response_free(resp);
        if (!acc) continue;

        const char *tok = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(acc, "token"));
        const char *addr = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(acc, "address"));
        if (tok && addr) {
            result = tm_email_info_new();
            result->channel = CHANNEL_MAIL_CX;
            result->email = tm_strdup(addr);
            result->token = tm_strdup(tok);
        }
        cJSON_Delete(acc);
    }

    free_domain_list(domains, dn);
    return result;
}

tm_email_t *tm_provider_mail_cx_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    char auth[600];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", token);
    const char *headers[] = {
        auth,
        "Accept: application/json",
        "Origin: https://mail.cx",
        "Referer: https://mail.cx/",
        NULL
    };

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, MCX_BASE "/api/messages?page=1", headers, NULL, 15);
    if (!resp || resp->status != 200) {
        tm_http_response_free(resp);
        return NULL;
    }

    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) return NULL;

    cJSON *messages = cJSON_GetObjectItemCaseSensitive(json, "messages");
    if (!cJSON_IsArray(messages)) {
        cJSON_Delete(json);
        return NULL;
    }

    int n = cJSON_GetArraySize(messages);
    *count = n;
    if (n == 0) {
        cJSON_Delete(json);
        return NULL;
    }

    tm_email_t *emails = tm_emails_new(n);
    for (int i = 0; i < n; i++) {
        cJSON *msg = cJSON_GetArrayItem(messages, i);
        const char *mid = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(msg, "id"));
        if (mid && mid[0]) {
            char detail_url[384];
            snprintf(detail_url, sizeof(detail_url), MCX_BASE "/api/messages/%s", mid);
            tm_http_response_t *dr = tm_http_request(TM_HTTP_GET, detail_url, headers, NULL, 15);
            if (dr && dr->status == 200) {
                cJSON *detail = cJSON_Parse(dr->body);
                if (detail) {
                    /* 附件 URL */
                    cJSON *atts = cJSON_GetObjectItemCaseSensitive(detail, "attachments");
                    if (cJSON_IsArray(atts)) {
                        cJSON *a;
                        cJSON_ArrayForEach(a, atts) {
                            cJSON *idx = cJSON_GetObjectItemCaseSensitive(a, "index");
                            char urlbuf[512];
                            if (cJSON_IsNumber(idx)) {
                                snprintf(urlbuf, sizeof(urlbuf), MCX_BASE "/api/messages/%s/attachments/%d", mid, idx->valueint);
                                cJSON_AddStringToObject(a, "url", urlbuf);
                            }
                        }
                    }
                    emails[i] = tm_normalize_email(detail, email);
                    cJSON_Delete(detail);
                    tm_http_response_free(dr);
                    continue;
                }
            }
            tm_http_response_free(dr);
        }
        emails[i] = tm_normalize_email(msg, email);
    }
    cJSON_Delete(json);
    return emails;
}
