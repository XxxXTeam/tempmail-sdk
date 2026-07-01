/**
 * Neighbours -- https://neighbours.sh
 */

#include "tempmail_internal.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NEIGHBOURS_BASE "https://neighbours.sh/api/v1"

static const char *neighbours_headers[] = {
    "Accept: application/json",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
    NULL
};

static void neighbours_seed_once(void) {
    static int seeded = 0;
    if (!seeded) {
        srand((unsigned int)time(NULL) ^ (unsigned int)clock());
        seeded = 1;
    }
}

static char *neighbours_urlencode(const char *s) {
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

static char *neighbours_trim_copy(const char *s) {
    if (!s) return tm_strdup("");
    while (*s && isspace((unsigned char)*s)) s++;
    const char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) end--;
    size_t len = (size_t)(end - s);
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}

static char *neighbours_lower_copy(const char *s) {
    char *out = neighbours_trim_copy(s);
    if (!out) return NULL;
    for (char *p = out; *p; p++) {
        *p = (char)tolower((unsigned char)*p);
    }
    return out;
}

static char *neighbours_random_local(void) {
    static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    char *out = (char *)malloc(20);
    if (!out) return NULL;
    neighbours_seed_once();
    memcpy(out, "sdk", 3);
    for (int i = 3; i < 19; i++) {
        out[i] = chars[rand() % (int)(sizeof(chars) - 1)];
    }
    out[19] = '\0';
    return out;
}

static char *neighbours_json_string(const cJSON *value) {
    if (!value) return tm_strdup("");
    if (cJSON_IsString(value) && value->valuestring) {
        return neighbours_trim_copy(value->valuestring);
    }
    if (cJSON_IsNumber(value)) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%g", value->valuedouble);
        return tm_strdup(buf);
    }
    if (cJSON_IsBool(value)) {
        return tm_strdup(cJSON_IsTrue(value) ? "true" : "false");
    }
    if (cJSON_IsArray(value)) {
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, (cJSON *)value) {
            char *hit = neighbours_json_string(item);
            if (hit && hit[0]) {
                return hit;
            }
            free(hit);
        }
        return tm_strdup("");
    }
    if (cJSON_IsObject(value)) {
        const cJSON *address = cJSON_GetObjectItemCaseSensitive((cJSON *)value, "address");
        if (cJSON_IsString(address) && address->valuestring) {
            char *hit = neighbours_trim_copy(address->valuestring);
            if (hit && hit[0]) {
                return hit;
            }
            free(hit);
        }
        const cJSON *text = cJSON_GetObjectItemCaseSensitive((cJSON *)value, "text");
        if (cJSON_IsString(text) && text->valuestring) {
            char *hit = neighbours_trim_copy(text->valuestring);
            if (hit && strchr(hit, '@')) {
                return hit;
            }
            free(hit);
        }
        const cJSON *inner = cJSON_GetObjectItemCaseSensitive((cJSON *)value, "value");
        if (inner) {
            char *hit = neighbours_json_string(inner);
            if (hit && hit[0]) {
                return hit;
            }
            free(hit);
        }
        return tm_strdup("");
    }
    return tm_strdup("");
}

static cJSON *neighbours_request_json(const char *path, bool allow_not_found, bool *not_found) {
    if (not_found) *not_found = false;
    char url[1024];
    snprintf(url, sizeof(url), "%s%s", NEIGHBOURS_BASE, path);
    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, neighbours_headers, NULL, 15);
    if (!resp) return NULL;
    if (allow_not_found && resp->status == 404) {
        if (not_found) *not_found = true;
        tm_http_response_free(resp);
        return NULL;
    }
    if (resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }
    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    return root;
}

static char **neighbours_domains(int *count) {
    *count = 0;
    cJSON *root = neighbours_request_json("/config/domains", false, NULL);
    if (!root) return NULL;

    cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
    cJSON *domains = data ? cJSON_GetObjectItemCaseSensitive(data, "domains") : NULL;
    int n = cJSON_IsArray(domains) ? cJSON_GetArraySize(domains) : 0;
    if (n <= 0) {
        cJSON_Delete(root);
        return NULL;
    }

    char **out = (char **)calloc((size_t)n, sizeof(char *));
    if (!out) {
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *item = NULL;
    cJSON_ArrayForEach(item, domains) {
        char *raw = neighbours_json_string(item);
        char *domain = neighbours_lower_copy(raw);
        free(raw);
        if (domain && domain[0]) {
            out[*count] = domain;
            (*count)++;
        } else {
            free(domain);
        }
    }

    cJSON_Delete(root);
    if (*count == 0) {
        free(out);
        return NULL;
    }
    return out;
}

static void neighbours_free_domains(char **domains, int count) {
    if (!domains) return;
    for (int i = 0; i < count; i++) {
        free(domains[i]);
    }
    free(domains);
}

static cJSON *neighbours_detail(const char *address, const char *uid) {
    char *addr_enc = neighbours_urlencode(address);
    char *uid_enc = neighbours_urlencode(uid);
    if (!addr_enc || !uid_enc) {
        free(addr_enc);
        free(uid_enc);
        return NULL;
    }

    char path[1024];
    snprintf(path, sizeof(path), "/inbox/%s/%s", addr_enc, uid_enc);
    free(addr_enc);
    free(uid_enc);

    bool not_found = false;
    cJSON *root = neighbours_request_json(path, true, &not_found);
    if (not_found || !root) {
        return NULL;
    }

    cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
    cJSON *dup = cJSON_IsObject(data) ? cJSON_Duplicate(data, 1) : NULL;
    cJSON_Delete(root);
    return dup;
}

static cJSON *neighbours_flatten_message(const cJSON *raw, const char *recipient) {
    cJSON *flat = cJSON_CreateObject();
    if (!flat) return NULL;

    char *id = neighbours_json_string(cJSON_GetObjectItemCaseSensitive((cJSON *)raw, "uid"));
    char *from = neighbours_json_string(cJSON_GetObjectItemCaseSensitive((cJSON *)raw, "from"));
    char *to = neighbours_json_string(cJSON_GetObjectItemCaseSensitive((cJSON *)raw, "to"));
    char *subject = neighbours_json_string(cJSON_GetObjectItemCaseSensitive((cJSON *)raw, "subject"));
    char *text = neighbours_json_string(cJSON_GetObjectItemCaseSensitive((cJSON *)raw, "text"));
    if (!text || !text[0]) {
        free(text);
        text = neighbours_json_string(cJSON_GetObjectItemCaseSensitive((cJSON *)raw, "snippet"));
    }
    char *html = neighbours_json_string(cJSON_GetObjectItemCaseSensitive((cJSON *)raw, "html"));
    char *date = neighbours_json_string(cJSON_GetObjectItemCaseSensitive((cJSON *)raw, "date"));
    if (!date || !date[0]) {
        free(date);
        date = neighbours_json_string(cJSON_GetObjectItemCaseSensitive((cJSON *)raw, "received_at"));
    }

    cJSON_AddStringToObject(flat, "id", id ? id : "");
    cJSON_AddStringToObject(flat, "from", from ? from : "");
    cJSON_AddStringToObject(flat, "to", (to && to[0]) ? to : (recipient ? recipient : ""));
    cJSON_AddStringToObject(flat, "subject", subject ? subject : "");
    cJSON_AddStringToObject(flat, "text", text ? text : "");
    cJSON_AddStringToObject(flat, "html", html ? html : "");
    cJSON_AddStringToObject(flat, "date", date ? date : "");

    const cJSON *attachments = cJSON_GetObjectItemCaseSensitive((cJSON *)raw, "attachments");
    if (cJSON_IsArray(attachments)) {
        cJSON_AddItemToObject(flat, "attachments", cJSON_Duplicate((cJSON *)attachments, 1));
    } else {
        cJSON_AddItemToObject(flat, "attachments", cJSON_CreateArray());
    }

    free(id);
    free(from);
    free(to);
    free(subject);
    free(text);
    free(html);
    free(date);
    return flat;
}

tm_email_info_t* tm_provider_neighbours_generate(const char *preferred_domain) {
    int domain_count = 0;
    char **domains = neighbours_domains(&domain_count);
    if (!domains || domain_count <= 0) {
        return NULL;
    }

    neighbours_seed_once();
    const char *selected = domains[rand() % domain_count];
    if (preferred_domain && preferred_domain[0]) {
        char *wanted = neighbours_lower_copy(preferred_domain);
        selected = NULL;
        if (wanted && wanted[0]) {
            for (int i = 0; i < domain_count; i++) {
                if (strcmp(domains[i], wanted) == 0) {
                    selected = domains[i];
                    break;
                }
            }
        }
        free(wanted);
        if (!selected) {
            neighbours_free_domains(domains, domain_count);
            return NULL;
        }
    }

    char *local = neighbours_random_local();
    if (!local) {
        neighbours_free_domains(domains, domain_count);
        return NULL;
    }

    size_t need = strlen(local) + 1 + strlen(selected) + 1;
    char *email = (char *)malloc(need);
    if (!email) {
        free(local);
        neighbours_free_domains(domains, domain_count);
        return NULL;
    }
    snprintf(email, need, "%s@%s", local, selected);
    free(local);
    neighbours_free_domains(domains, domain_count);

    tm_email_info_t *info = tm_email_info_new();
    if (!info) {
        free(email);
        return NULL;
    }
    info->channel = CHANNEL_NEIGHBOURS;
    info->email = email;
    info->token = tm_strdup(email);
    return info;
}

tm_email_t* tm_provider_neighbours_get_emails(const char *email, int *count) {
    *count = -1;
    if (!email || !email[0]) {
        return NULL;
    }

    char *enc = neighbours_urlencode(email);
    if (!enc) {
        return NULL;
    }

    char path[1024];
    snprintf(path, sizeof(path), "/inbox/%s", enc);
    free(enc);

    bool not_found = false;
    cJSON *root = neighbours_request_json(path, true, &not_found);
    if (not_found) {
        *count = 0;
        return NULL;
    }
    if (!root) {
        return NULL;
    }

    cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
    int n = cJSON_IsArray(data) ? cJSON_GetArraySize(data) : 0;
    *count = n;
    if (n <= 0) {
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
        cJSON *row = cJSON_GetArrayItem(data, i);
        cJSON *raw = row;
        cJSON *detail = NULL;

        const cJSON *uid_item = cJSON_GetObjectItemCaseSensitive(row, "uid");
        char *uid = neighbours_json_string(uid_item);
        if (uid && uid[0]) {
            detail = neighbours_detail(email, uid);
            if (detail) {
                raw = detail;
            }
        }
        free(uid);

        cJSON *flat = neighbours_flatten_message(raw, email);
        emails[i] = tm_normalize_email(flat ? flat : raw, email);
        cJSON_Delete(flat);
        cJSON_Delete(detail);
    }

    cJSON_Delete(root);
    return emails;
}
