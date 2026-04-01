/**
 * Fake Legal — https://fake.legal
 */
#include "tempmail_internal.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef _WIN32
#include <strings.h>
#endif

#define FL_BASE "https://fake.legal"

static const char *fl_headers[] = {
    "Accept: application/json, text/plain, */*",
    "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8",
    "Cache-Control: no-cache",
    "DNT: 1",
    "Pragma: no-cache",
    "Referer: https://fake.legal/",
    "sec-ch-ua: \"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"146\"",
    "sec-ch-ua-mobile: ?0",
    "sec-ch-ua-platform: \"Windows\"",
    "sec-fetch-dest: empty",
    "sec-fetch-mode: cors",
    "sec-fetch-site: same-origin",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    NULL
};

static int fl_stricmp(const char *a, const char *b) {
#ifdef _WIN32
    return _stricmp(a ? a : "", b ? b : "");
#else
    return strcasecmp(a ? a : "", b ? b : "");
#endif
}

static char *fl_curl_escape(const char *s) {
    if (!s) return tm_strdup("");
    CURL *c = curl_easy_init();
    if (!c) return tm_strdup(s);
    char *e = curl_easy_escape(c, s, 0);
    curl_easy_cleanup(c);
    if (!e) return tm_strdup(s);
    char *d = tm_strdup(e);
    curl_free(e);
    return d;
}

static void fl_pick_domain(char domains[][256], int nd, const char *preferred, char *out, size_t cap) {
    if (!out || cap < 2) return;
    out[0] = '\0';
    if (nd <= 0) return;
    if (preferred && preferred[0]) {
        for (int i = 0; i < nd; i++) {
            if (fl_stricmp(domains[i], preferred) == 0) {
                strncpy(out, domains[i], cap - 1);
                out[cap - 1] = '\0';
                return;
            }
        }
    }
    int pick = rand() % nd;
    strncpy(out, domains[pick], cap - 1);
    out[cap - 1] = '\0';
}

tm_email_info_t* tm_provider_fake_legal_generate(const char *domain) {
    srand((unsigned)time(NULL));
    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, FL_BASE "/api/domains", fl_headers, NULL, 15);
    if (!resp || resp->status < 200 || resp->status >= 300) { tm_http_response_free(resp); return NULL; }

    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!root) return NULL;

    cJSON *darr = cJSON_GetObjectItemCaseSensitive(root, "domains");
    char domains[64][256];
    int nd = 0;
    if (cJSON_IsArray(darr)) {
        int n = cJSON_GetArraySize(darr);
        for (int i = 0; i < n && nd < 64; i++) {
            cJSON *it = cJSON_GetArrayItem(darr, i);
            const char *s = cJSON_GetStringValue(it);
            if (s && s[0]) {
                strncpy(domains[nd], s, sizeof(domains[0]) - 1);
                domains[nd][sizeof(domains[0]) - 1] = '\0';
                nd++;
            }
        }
    }
    cJSON_Delete(root);
    if (nd == 0) return NULL;

    char pickdom[256];
    fl_pick_domain(domains, nd, domain, pickdom, sizeof(pickdom));
    if (!pickdom[0]) return NULL;

    char *enc = fl_curl_escape(pickdom);
    char url[768];
    snprintf(url, sizeof(url), "%s/api/inbox/new?domain=%s", FL_BASE, enc);
    free(enc);

    tm_http_response_t *r2 = tm_http_request(TM_HTTP_GET, url, fl_headers, NULL, 15);
    if (!r2 || r2->status < 200 || r2->status >= 300) { tm_http_response_free(r2); return NULL; }

    cJSON *cj = cJSON_Parse(r2->body);
    tm_http_response_free(r2);
    if (!cj) return NULL;

    const cJSON *ok = cJSON_GetObjectItemCaseSensitive(cj, "success");
    if (!cJSON_IsBool(ok) || !cJSON_IsTrue(ok)) { cJSON_Delete(cj); return NULL; }
    const char *addr = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(cj, "address"));
    if (!addr || !addr[0]) { cJSON_Delete(cj); return NULL; }

    tm_email_info_t *info = tm_email_info_new();
    info->channel = CHANNEL_FAKE_LEGAL;
    info->email = tm_strdup(addr);
    info->token = NULL;
    cJSON_Delete(cj);
    return info;
}

tm_email_t* tm_provider_fake_legal_get_emails(const char *email, int *count) {
    *count = -1;
    if (!email || !email[0]) return NULL;

    char *esc = fl_curl_escape(email);
    char url[1200];
    snprintf(url, sizeof(url), "%s/api/inbox/%s", FL_BASE, esc);
    free(esc);

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, fl_headers, NULL, 15);
    if (!resp || resp->status < 200 || resp->status >= 300) { tm_http_response_free(resp); return NULL; }

    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!root) return NULL;

    const cJSON *ok = cJSON_GetObjectItemCaseSensitive(root, "success");
    if (!cJSON_IsBool(ok) || !cJSON_IsTrue(ok)) {
        *count = 0;
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *arr = cJSON_GetObjectItemCaseSensitive(root, "emails");
    if (!arr || !cJSON_IsArray(arr)) {
        *count = 0;
        cJSON_Delete(root);
        return NULL;
    }

    int n = cJSON_GetArraySize(arr);
    *count = n;
    if (n == 0) {
        cJSON_Delete(root);
        return NULL;
    }

    tm_email_t *emails = tm_emails_new(n);
    if (!emails) {
        cJSON_Delete(root);
        *count = -1;
        return NULL;
    }
    for (int i = 0; i < n; i++) {
        cJSON *raw = cJSON_GetArrayItem(arr, i);
        emails[i] = tm_normalize_email(raw, email);
    }
    cJSON_Delete(root);
    return emails;
}
