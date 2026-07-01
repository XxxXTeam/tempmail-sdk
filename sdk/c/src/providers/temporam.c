/**
 * Temporam -- https://temporam.com
 */

#include "tempmail_internal.h"
#include <time.h>

#define TEMPORAM_BASE "https://temporam.com"

static const char *temporam_headers[] = {
    "Accept: application/json",
    "Accept-Encoding: gzip, deflate, br",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
    NULL
};

static void temporam_seed_once(void) {
    static int seeded = 0;
    if (!seeded) {
        srand((unsigned int)time(NULL) ^ (unsigned int)clock());
        seeded = 1;
    }
}

static char *temporam_urlencode(const char *s) {
    if (!s) return tm_strdup("");
    size_t len = strlen(s);
    char *out = (char *)malloc(len * 3 + 1);
    if (!out) return NULL;
    char *p = out;
    static const char hex[] = "0123456789ABCDEF";
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            *p++ = (char)c;
        } else {
            *p++ = '%';
            *p++ = hex[c >> 4];
            *p++ = hex[c & 15];
        }
    }
    *p = '\0';
    return out;
}

static void temporam_random_local(char *out, size_t cap) {
    static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    temporam_seed_once();
    if (cap == 0) return;
    size_t pos = 0;
    const char *prefix = "sdk";
    while (prefix[pos] && pos + 1 < cap) {
        out[pos] = prefix[pos];
        pos++;
    }
    for (int i = 0; i < 18 && pos + 1 < cap; i++) {
        out[pos++] = chars[rand() % (int)(sizeof(chars) - 1)];
    }
    out[pos] = '\0';
}

static cJSON *temporam_get_json(const char *path) {
    char url[1024];
    snprintf(url, sizeof(url), TEMPORAM_BASE "%s", path);
    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, temporam_headers, NULL, 15);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }
    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    return json;
}

static char **temporam_domains(int *count) {
    *count = 0;
    cJSON *root = temporam_get_json("/api/domains");
    if (!root) return NULL;
    cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
    int n = cJSON_IsArray(data) ? cJSON_GetArraySize(data) : 0;
    if (n <= 0) {
        cJSON_Delete(root);
        return NULL;
    }
    char **domains = (char **)calloc((size_t)n, sizeof(char *));
    if (!domains) {
        cJSON_Delete(root);
        return NULL;
    }
    for (int i = 0; i < n; i++) {
        cJSON *item = cJSON_GetArrayItem(data, i);
        const char *domain = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(item, "domain"), "");
        if (domain[0]) {
            domains[*count] = tm_strdup(domain);
            if (domains[*count]) (*count)++;
        }
    }
    cJSON_Delete(root);
    if (*count == 0) {
        free(domains);
        return NULL;
    }
    return domains;
}

static void temporam_free_domains(char **domains, int count) {
    if (!domains) return;
    for (int i = 0; i < count; i++) free(domains[i]);
    free(domains);
}

tm_email_info_t *tm_provider_temporam_generate(const char *preferred_domain) {
    int domain_count = 0;
    char **domains = temporam_domains(&domain_count);
    if (!domains || domain_count <= 0) return NULL;
    temporam_seed_once();
    const char *selected = domains[rand() % domain_count];
    if (preferred_domain && preferred_domain[0]) {
        selected = NULL;
        for (int i = 0; i < domain_count; i++) {
            if (strcmp(domains[i], preferred_domain) == 0) {
                selected = domains[i];
                break;
            }
        }
        if (!selected) {
            temporam_free_domains(domains, domain_count);
            return NULL;
        }
    }

    char local[32];
    char email[512];
    temporam_random_local(local, sizeof(local));
    snprintf(email, sizeof(email), "%s@%s", local, selected);

    tm_email_info_t *info = tm_email_info_new();
    if (info) {
        info->channel = CHANNEL_TEMPORAM;
        info->email = tm_strdup(email);
        info->token = tm_strdup(email);
    }
    temporam_free_domains(domains, domain_count);
    return info;
}

static cJSON *temporam_flatten(cJSON *raw, const char *recipient) {
    cJSON *flat = cJSON_CreateObject();
    if (!flat) return NULL;
    const char *id = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(raw, "id"),
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(raw, "uuid"), ""));
    cJSON_AddStringToObject(flat, "id", id);
    cJSON_AddStringToObject(flat, "from", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(raw, "fromEmail"), ""));
    cJSON_AddStringToObject(flat, "to", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(raw, "toEmail"), recipient));
    cJSON_AddStringToObject(flat, "subject", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(raw, "subject"), ""));
    cJSON_AddStringToObject(flat, "content", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(raw, "content"),
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(raw, "summary"), "")));
    cJSON_AddStringToObject(flat, "date", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(raw, "createdAt"), ""));
    return flat;
}

tm_email_t *tm_provider_temporam_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    const char *recipient = (token && token[0]) ? token : email;
    if (!recipient || !recipient[0]) return NULL;

    char *enc = temporam_urlencode(recipient);
    if (!enc) return NULL;
    char path[1024];
    snprintf(path, sizeof(path), "/api/emails?email=%s&limit=50", enc);
    free(enc);

    cJSON *root = temporam_get_json(path);
    if (!root) return NULL;
    cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
    int n = cJSON_IsArray(data) ? cJSON_GetArraySize(data) : 0;
    *count = n;
    if (n <= 0) {
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
        cJSON *row = cJSON_GetArrayItem(data, i);
        cJSON *raw = row;
        cJSON *detail_root = NULL;
        const char *id = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(row, "id"), "");
        if (id[0]) {
            char *id_enc = temporam_urlencode(id);
            if (id_enc) {
                char detail_path[512];
                snprintf(detail_path, sizeof(detail_path), "/api/emails/%s", id_enc);
                free(id_enc);
                detail_root = temporam_get_json(detail_path);
                cJSON *detail_data = detail_root ? cJSON_GetObjectItemCaseSensitive(detail_root, "data") : NULL;
                if (cJSON_IsObject(detail_data)) raw = detail_data;
            }
        }
        cJSON *flat = temporam_flatten(raw, recipient);
        emails[i] = tm_normalize_email(flat ? flat : raw, recipient);
        cJSON_Delete(flat);
        cJSON_Delete(detail_root);
    }
    cJSON_Delete(root);
    return emails;
}
