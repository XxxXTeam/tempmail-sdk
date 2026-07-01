/**
 * fmail -- https://fmail.men
 */

#include "tempmail_internal.h"
#include <ctype.h>

#define FMAIL_BASE "https://fmail.men"

static const char *fmail_headers[] = {
    "Accept: application/json",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
    NULL
};

static char *fmail_trim_copy(const char *s) {
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

static char *fmail_urlencode(const char *s) {
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

static cJSON *fmail_request_json(const char *path) {
    char url[1024];
    snprintf(url, sizeof(url), FMAIL_BASE "%s", path);
    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, fmail_headers, NULL, 15);
    if (!resp) return NULL;
    if (resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }
    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    return root;
}

static char *fmail_any_str(const cJSON *obj, const char **keys, int key_count) {
    char *raw = tm_json_get_str(obj, keys, key_count);
    char *trimmed = fmail_trim_copy(raw);
    free(raw);
    return trimmed;
}

static cJSON *fmail_flatten_message(const cJSON *raw, const char *recipient) {
    cJSON *flat = cJSON_CreateObject();
    if (!flat) return NULL;

    char *id = fmail_any_str(raw, (const char *[]){"id", "uid", "token"}, 3);
    char *from = fmail_any_str(raw, (const char *[]){"sender", "from"}, 2);
    char *to = fmail_any_str(raw, (const char *[]){"recipient", "to"}, 2);
    char *subject = fmail_any_str(raw, (const char *[]){"subject"}, 1);
    char *text = fmail_any_str(raw, (const char *[]){"body_text", "text", "snippet"}, 3);
    char *html = fmail_any_str(raw, (const char *[]){"body_html", "html"}, 2);
    char *date = fmail_any_str(raw, (const char *[]){"received_at", "timestamp", "date"}, 3);

    cJSON_AddStringToObject(flat, "id", id ? id : "");
    cJSON_AddStringToObject(flat, "from", from ? from : "");
    cJSON_AddStringToObject(flat, "to", (to && to[0]) ? to : recipient);
    cJSON_AddStringToObject(flat, "subject", subject ? subject : "");
    cJSON_AddStringToObject(flat, "text", text ? text : "");
    cJSON_AddStringToObject(flat, "html", html ? html : "");
    cJSON_AddStringToObject(flat, "date", date ? date : "");
    cJSON_AddBoolToObject(flat, "is_read", cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive((cJSON *)raw, "is_read")));

    cJSON *atts = cJSON_GetObjectItemCaseSensitive((cJSON *)raw, "attachments");
    if (atts) {
        cJSON_AddItemToObject(flat, "attachments", cJSON_Duplicate(atts, 1));
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

tm_email_info_t* tm_provider_fmail_generate(const char *domain) {
    char url[1024];
    char *selected = fmail_trim_copy(domain);
    if (selected && selected[0]) {
        char *encoded = fmail_urlencode(selected);
        snprintf(url, sizeof(url), FMAIL_BASE "/v1/random?domain=%s", encoded ? encoded : "");
        free(encoded);
    } else {
        snprintf(url, sizeof(url), FMAIL_BASE "/v1/random");
    }
    free(selected);

    cJSON *json = fmail_request_json(strstr(url, FMAIL_BASE) + strlen(FMAIL_BASE));
    if (!json) return NULL;

    char *email = fmail_any_str(json, (const char *[]){"address"}, 1);
    if (!email || !email[0]) {
        char *username = fmail_any_str(json, (const char *[]){"username"}, 1);
        char *dom = fmail_any_str(json, (const char *[]){"domain"}, 1);
        if (username && username[0] && dom && dom[0]) {
            size_t len = strlen(username) + strlen(dom) + 2;
            email = (char *)realloc(email, len);
            if (email) {
                snprintf(email, len, "%s@%s", username, dom);
            }
        }
        free(username);
        free(dom);
    }
    if (!email || !strchr(email, '@')) {
        free(email);
        cJSON_Delete(json);
        return NULL;
    }

    tm_email_info_t *info = tm_email_info_new();
    if (!info) {
        free(email);
        cJSON_Delete(json);
        return NULL;
    }
    info->channel = CHANNEL_FMAIL;
    info->email = tm_strdup(email);
    free(email);
    cJSON_Delete(json);
    return info;
}

tm_email_t* tm_provider_fmail_get_emails(const char *email, int *count) {
    *count = -1;
    if (!email || !email[0]) return NULL;

    const char *at = strchr(email, '@');
    if (!at || at == email || !at[1]) return NULL;
    size_t local_len = (size_t)(at - email);
    char *local = (char *)malloc(local_len + 1);
    if (!local) return NULL;
    memcpy(local, email, local_len);
    local[local_len] = '\0';

    char *enc_local = fmail_urlencode(local);
    char *enc_domain = fmail_urlencode(at + 1);
    free(local);
    if (!enc_local || !enc_domain) {
        free(enc_local);
        free(enc_domain);
        return NULL;
    }

    char path[1024];
    snprintf(path, sizeof(path), "/v1/inbox/%s?domain=%s&limit=50", enc_local, enc_domain);
    free(enc_local);
    free(enc_domain);

    cJSON *json = fmail_request_json(path);
    if (!json) return NULL;

    cJSON *emails = cJSON_GetObjectItemCaseSensitive(json, "emails");
    int n = cJSON_IsArray(emails) ? cJSON_GetArraySize(emails) : 0;
    *count = n;
    if (n == 0) {
        cJSON_Delete(json);
        return NULL;
    }

    tm_email_t *out = tm_emails_new(n);
    if (!out) {
        cJSON_Delete(json);
        return NULL;
    }

    for (int i = 0; i < n; i++) {
        cJSON *row = cJSON_GetArrayItem(emails, i);
        cJSON *flat = NULL;
        char *token = fmail_any_str(row, (const char *[]){"token", "id"}, 2);
        if (token && token[0]) {
            char *enc_token = fmail_urlencode(token);
            char detail_path[1024];
            snprintf(detail_path, sizeof(detail_path), "/v1/email/%s", enc_token ? enc_token : "");
            free(enc_token);
            cJSON *detail = fmail_request_json(detail_path);
            if (detail) {
                cJSON *nested = cJSON_GetObjectItemCaseSensitive(detail, "email");
                if (cJSON_IsObject(nested)) {
                    flat = fmail_flatten_message(nested, email);
                } else {
                    flat = fmail_flatten_message(detail, email);
                }
                cJSON_Delete(detail);
            }
        }
        if (!flat) {
            flat = fmail_flatten_message(row, email);
        }
        if (!flat) {
            out[i] = tm_normalize_email(cJSON_Parse("{}"), email);
        } else {
            out[i] = tm_normalize_email(flat, email);
            cJSON_Delete(flat);
        }
        free(token);
    }

    cJSON_Delete(json);
    return out;
}
