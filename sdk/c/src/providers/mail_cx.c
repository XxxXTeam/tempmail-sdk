/**
 * mail.cx -- https://mail.cx
 * Anonymous Web API: https://mail.cx/v1
 */
#include "tempmail_internal.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAILCX_BASE "https://mail.cx"

static void mailcx_random_string(char *buf, size_t len) {
    static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    for (size_t i = 0; i < len; i++) {
        buf[i] = chars[rand() % (int)(sizeof(chars) - 1)];
    }
    buf[len] = '\0';
}

static char *mailcx_url_encode(const char *s) {
    if (!s) return tm_strdup("");
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char*)s; *p; p++) {
        if (isalnum(*p) || *p == '-' || *p == '_' || *p == '.' || *p == '~') n += 1;
        else n += 3;
    }
    char *out = (char*)malloc(n + 1);
    if (!out) return NULL;
    char *w = out;
    for (const unsigned char *p = (const unsigned char*)s; *p; p++) {
        if (isalnum(*p) || *p == '-' || *p == '_' || *p == '.' || *p == '~') {
            *w++ = (char)*p;
        } else {
            snprintf(w, 4, "%%%02X", *p);
            w += 3;
        }
    }
    *w = '\0';
    return out;
}

static void mailcx_headers(const char **headers, char *client_line, size_t client_line_cap, const char *client_id) {
    snprintf(client_line, client_line_cap, "X-Client-ID: %s", client_id);
    headers[0] = "Accept: application/json";
    headers[1] = "Origin: https://mail.cx";
    headers[2] = "Referer: https://mail.cx/";
    headers[3] = "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";
    headers[4] = client_line;
    headers[5] = NULL;
}

static cJSON *mailcx_get_config(const char *client_id) {
    char client_line[128];
    const char *headers[6];
    mailcx_headers(headers, client_line, sizeof(client_line), client_id);

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, MAILCX_BASE "/v1/config", headers, NULL, 15);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }
    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    return root;
}

static char *mailcx_pick_domain(cJSON *config, const char *preferred) {
    cJSON *domains = cJSON_GetObjectItemCaseSensitive(config, "system_domains");
    if (!cJSON_IsArray(domains) || cJSON_GetArraySize(domains) == 0) return NULL;

    char want[256] = "";
    if (preferred && preferred[0]) {
        const char *p = preferred[0] == '@' ? preferred + 1 : preferred;
        size_t len = strlen(p);
        if (len >= sizeof(want)) len = sizeof(want) - 1;
        for (size_t i = 0; i < len; i++) want[i] = (char)tolower((unsigned char)p[i]);
        want[len] = '\0';
    }

    cJSON *item = NULL;
    cJSON_ArrayForEach(item, domains) {
        cJSON *domain = cJSON_GetObjectItemCaseSensitive(item, "domain");
        if (!cJSON_IsString(domain) || !domain->valuestring || !domain->valuestring[0]) continue;
        if (want[0]) {
            char lower[256];
            size_t len = strlen(domain->valuestring);
            if (len >= sizeof(lower)) len = sizeof(lower) - 1;
            for (size_t i = 0; i < len; i++) lower[i] = (char)tolower((unsigned char)domain->valuestring[i]);
            lower[len] = '\0';
            if (strcmp(lower, want) == 0) return tm_strdup(domain->valuestring);
        }
    }

    cJSON_ArrayForEach(item, domains) {
        cJSON *is_default = cJSON_GetObjectItemCaseSensitive(item, "default");
        cJSON *domain = cJSON_GetObjectItemCaseSensitive(item, "domain");
        if (cJSON_IsTrue(is_default) && cJSON_IsString(domain) && domain->valuestring && domain->valuestring[0]) {
            return tm_strdup(domain->valuestring);
        }
    }

    cJSON_ArrayForEach(item, domains) {
        cJSON *domain = cJSON_GetObjectItemCaseSensitive(item, "domain");
        if (cJSON_IsString(domain) && domain->valuestring && domain->valuestring[0]) {
            return tm_strdup(domain->valuestring);
        }
    }

    return NULL;
}

static const char *mailcx_first_str(cJSON *obj, const char *a, const char *b) {
    cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, a);
    if (cJSON_IsString(v) && v->valuestring && v->valuestring[0]) return v->valuestring;
    v = cJSON_GetObjectItemCaseSensitive(obj, b);
    if (cJSON_IsString(v) && v->valuestring && v->valuestring[0]) return v->valuestring;
    return "";
}

static void mailcx_add_dup(cJSON *dst, const char *dst_key, cJSON *src, const char *src_key) {
    cJSON *item = cJSON_GetObjectItemCaseSensitive(src, src_key);
    if (item) cJSON_AddItemToObject(dst, dst_key, cJSON_Duplicate(item, 1));
}

static cJSON *mailcx_flatten_list(cJSON *raw, const char *recipient) {
    cJSON *obj = cJSON_CreateObject();
    if (!obj) return NULL;
    mailcx_add_dup(obj, "id", raw, "id");
    cJSON_AddStringToObject(obj, "from", mailcx_first_str(raw, "from_email", "from_name"));
    cJSON_AddStringToObject(obj, "to", recipient ? recipient : "");
    mailcx_add_dup(obj, "subject", raw, "subject");
    mailcx_add_dup(obj, "text", raw, "preview_text");
    mailcx_add_dup(obj, "created_at", raw, "created_at");
    mailcx_add_dup(obj, "attachments", raw, "attachments");
    return obj;
}

static cJSON *mailcx_flatten_detail(cJSON *raw, const char *recipient) {
    cJSON *obj = cJSON_CreateObject();
    if (!obj) return NULL;
    mailcx_add_dup(obj, "id", raw, "id");
    cJSON_AddStringToObject(obj, "from", mailcx_first_str(raw, "from_email", "from_name"));
    cJSON_AddStringToObject(obj, "to", recipient ? recipient : "");
    mailcx_add_dup(obj, "subject", raw, "subject");
    mailcx_add_dup(obj, "text_body", raw, "text_body");
    mailcx_add_dup(obj, "html_body", raw, "html_body");
    cJSON_AddStringToObject(obj, "text", mailcx_first_str(raw, "text_body", "preview_text"));
    mailcx_add_dup(obj, "html", raw, "html_body");
    mailcx_add_dup(obj, "created_at", raw, "created_at");
    mailcx_add_dup(obj, "attachments", raw, "attachments");
    return obj;
}

static cJSON *mailcx_get_detail(const char *client_id, const char *id) {
    char *enc = mailcx_url_encode(id);
    if (!enc) return NULL;
    size_t url_len = strlen(MAILCX_BASE) + strlen("/v1/email/") + strlen(enc) + 1;
    char *url = (char*)malloc(url_len);
    if (!url) {
        free(enc);
        return NULL;
    }
    snprintf(url, url_len, "%s/v1/email/%s", MAILCX_BASE, enc);
    free(enc);

    char client_line[128];
    const char *headers[6];
    mailcx_headers(headers, client_line, sizeof(client_line), client_id);

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, headers, NULL, 15);
    free(url);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }
    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    return root;
}

tm_email_info_t* tm_provider_mail_cx_generate(const char *domain) {
    srand((unsigned)time(NULL));

    char cid_rand[17];
    mailcx_random_string(cid_rand, 16);
    char client_id[64];
    snprintf(client_id, sizeof(client_id), "tempmail-sdk-%s", cid_rand);

    cJSON *config = mailcx_get_config(client_id);
    if (!config) return NULL;

    char *picked_domain = mailcx_pick_domain(config, domain);
    if (!picked_domain) {
        cJSON_Delete(config);
        return NULL;
    }

    char local[13];
    mailcx_random_string(local, 12);

    size_t email_len = strlen(local) + strlen("@") + strlen(picked_domain) + 1;
    char *email = (char*)malloc(email_len);
    if (!email) {
        free(picked_domain);
        cJSON_Delete(config);
        return NULL;
    }
    snprintf(email, email_len, "%s@%s", local, picked_domain);
    free(picked_domain);

    tm_email_info_t *info = tm_email_info_new();
    if (!info) {
        free(email);
        cJSON_Delete(config);
        return NULL;
    }
    info->channel = CHANNEL_MAIL_CX;
    info->email = email;
    info->token = tm_strdup(client_id);

    time_t now = time(NULL);
    char created[48];
    struct tm tmu;
#ifdef _WIN32
    if (gmtime_s(&tmu, &now) != 0) memset(&tmu, 0, sizeof(tmu));
#else
    gmtime_r(&now, &tmu);
#endif
    strftime(created, sizeof(created), "%Y-%m-%dT%H:%M:%SZ", &tmu);
    info->created_at = tm_strdup(created);

    cJSON *ttl = cJSON_GetObjectItemCaseSensitive(config, "ttl_seconds");
    if (cJSON_IsNumber(ttl) && ttl->valuedouble > 0) {
        info->expires_at = ((long long)now + (long long)ttl->valuedouble) * 1000LL;
    }

    cJSON_Delete(config);
    return info;
}

tm_email_t* tm_provider_mail_cx_get_emails(const char *client_id, const char *email, int *count) {
    *count = -1;
    if (!client_id || !client_id[0] || !email || !email[0]) return NULL;

    char *enc = mailcx_url_encode(email);
    if (!enc) return NULL;
    size_t url_len = strlen(MAILCX_BASE) + strlen("/v1/inbox/") + strlen(enc) + 1;
    char *url = (char*)malloc(url_len);
    if (!url) {
        free(enc);
        return NULL;
    }
    snprintf(url, url_len, "%s/v1/inbox/%s", MAILCX_BASE, enc);
    free(enc);

    char client_line[128];
    const char *headers[6];
    mailcx_headers(headers, client_line, sizeof(client_line), client_id);

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, headers, NULL, 30);
    free(url);
    if (!resp) return NULL;
    if (resp->status == 204) {
        *count = 0;
        tm_http_response_free(resp);
        return NULL;
    }
    if (resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }

    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!root) return NULL;

    cJSON *arr = cJSON_GetObjectItemCaseSensitive(root, "emails");
    if (!cJSON_IsArray(arr)) {
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
        *count = -1;
        cJSON_Delete(root);
        return NULL;
    }

    for (int i = 0; i < n; i++) {
        cJSON *row = cJSON_GetArrayItem(arr, i);
        cJSON *flat = mailcx_flatten_list(row, email);
        cJSON *id = cJSON_GetObjectItemCaseSensitive(row, "id");
        if (cJSON_IsString(id) && id->valuestring && id->valuestring[0]) {
            cJSON *detail = mailcx_get_detail(client_id, id->valuestring);
            if (detail) {
                cJSON_Delete(flat);
                flat = mailcx_flatten_detail(detail, email);
                cJSON_Delete(detail);
            }
        }
        emails[i] = tm_normalize_email(flat ? flat : row, email);
        cJSON_Delete(flat);
    }

    cJSON_Delete(root);
    return emails;
}
