/**
 * Maildrop — https://maildrop.cx
 */
#include "tempmail_internal.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define MD_STRICMP _stricmp
#else
#include <strings.h>
#define MD_STRICMP strcasecmp
#endif

#define MD_BASE "https://maildrop.cx"
#define MD_EXCLUDED "transformer.edu.kg"

static const char *md_headers[] = {
    "Accept: application/json, text/plain, */*",
    "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
    "Cache-Control: no-cache",
    "DNT: 1",
    "Pragma: no-cache",
    "Referer: https://maildrop.cx/zh-cn/app",
    "sec-ch-ua: \"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"146\"",
    "sec-ch-ua-mobile: ?0",
    "sec-ch-ua-platform: \"Windows\"",
    "sec-fetch-dest: empty",
    "sec-fetch-mode: cors",
    "sec-fetch-site: same-origin",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    "x-requested-with: XMLHttpRequest",
    NULL
};

static void md_random_local(char *buf, int len) {
    const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    srand((unsigned)time(NULL));
    for (int i = 0; i < len; i++)
        buf[i] = chars[rand() % (int)(sizeof(chars) - 1)];
    buf[len] = '\0';
}

static char *md_curl_escape(const char *s) {
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

static void md_pick_domain(char domains[][256], int nd, const char *preferred, char *out, size_t cap) {
    if (!out || cap < 2) return;
    out[0] = '\0';
    if (nd <= 0) return;
    if (preferred && preferred[0]) {
        for (int i = 0; i < nd; i++) {
            if (MD_STRICMP(domains[i], preferred) == 0) {
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

tm_email_info_t *tm_provider_maildrop_generate(const char *domain) {
    srand((unsigned)time(NULL));
    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, MD_BASE "/api/suffixes.php", md_headers, NULL, 15);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }

    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!root || !cJSON_IsArray(root)) {
        cJSON_Delete(root);
        return NULL;
    }

    char domains[64][256];
    int nd = 0;
    int n = cJSON_GetArraySize(root);
    for (int i = 0; i < n && nd < 64; i++) {
        cJSON *it = cJSON_GetArrayItem(root, i);
        const char *s = cJSON_GetStringValue(it);
        if (s && s[0] && MD_STRICMP(s, MD_EXCLUDED) != 0) {
            strncpy(domains[nd], s, sizeof(domains[0]) - 1);
            domains[nd][sizeof(domains[0]) - 1] = '\0';
            nd++;
        }
    }
    cJSON_Delete(root);
    if (nd == 0) return NULL;

    char pickdom[256];
    md_pick_domain(domains, nd, domain, pickdom, sizeof(pickdom));
    if (!pickdom[0]) return NULL;

    char local[16];
    md_random_local(local, 10);

    char email[320];
    snprintf(email, sizeof(email), "%s@%s", local, pickdom);

    tm_email_info_t *info = tm_email_info_new();
    info->channel = CHANNEL_MAILDROP;
    info->email = tm_strdup(email);
    info->token = tm_strdup(email);
    return info;
}

tm_email_t *tm_provider_maildrop_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    const char *addr = (email && email[0]) ? email : token;
    if (!addr || !addr[0]) return NULL;

    char *esc = md_curl_escape(addr);
    char url[800];
    snprintf(url, sizeof(url), "%s/api/emails.php?addr=%s&page=1&limit=20", MD_BASE, esc);
    free(esc);

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, md_headers, NULL, 15);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }

    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!root) return NULL;

    cJSON *arr = cJSON_GetObjectItemCaseSensitive(root, "emails");
    int n = (arr && cJSON_IsArray(arr)) ? cJSON_GetArraySize(arr) : 0;
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
        emails[i] = tm_normalize_email(raw, addr);
    }
    cJSON_Delete(root);
    return emails;
}
