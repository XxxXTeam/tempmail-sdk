/**
 * dropmail.me 渠道实现 (GraphQL)
 * 自动 af_ 令牌：/api/token/generate，将过期时 /api/token/renew
 * 配置：环境变量 DROPMAIL_AUTH_TOKEN、DROPMAIL_API_TOKEN、DROPMAIL_NO_AUTO_TOKEN、DROPMAIL_RENEW_LIFETIME
 */
#include "tempmail_internal.h"
#include <curl/curl.h>
#include <time.h>

#ifdef _WIN32
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

static const char *dm_json_headers[] = {
    "Accept: application/json",
    "Content-Type: application/json",
    "Origin: https://dropmail.me",
    "Referer: https://dropmail.me/api/",
    NULL
};

static const char *dm_form_headers[] = {
    "Content-Type: application/x-www-form-urlencoded",
    NULL
};

static const char DM_GEN_URL[] = "https://dropmail.me/api/token/generate";
static const char DM_REN_URL[] = "https://dropmail.me/api/token/renew";

static char *dm_af_cached = NULL;
static time_t dm_af_deadline = 0;

#define DM_RENEW_BEFORE_SEC 600

static const char *dm_explicit_auth(void) {
    const char *a = getenv("DROPMAIL_AUTH_TOKEN");
    if (a && *a) return a;
    a = getenv("DROPMAIL_API_TOKEN");
    if (a && *a) return a;
    return NULL;
}

static int dm_auto_disabled(void) {
    const char *v = getenv("DROPMAIL_NO_AUTO_TOKEN");
    if (!v || !*v) return 0;
    return (strcasecmp(v, "1") == 0 || strcasecmp(v, "true") == 0 || strcasecmp(v, "yes") == 0);
}

static const char *dm_renew_lifetime_env(void) {
    const char *v = getenv("DROPMAIL_RENEW_LIFETIME");
    if (v && *v) return v;
    return "1d";
}

static time_t dm_deadline_for_lifetime(const char *life) {
    time_t now = time(NULL);
    if (!life || !*life) return now + 50 * 60;
    if (strcasecmp(life, "1h") == 0) return now + 50 * 60;
    if (strcasecmp(life, "1d") == 0) return now + 23 * 3600;
    {
        size_t n = strlen(life);
        if (n > 1 && (life[n - 1] == 'd' || life[n - 1] == 'D')) {
            int days = atoi(life);
            if (days > 1) return now + (time_t)(days - 1) * 24 * 3600;
        }
    }
    return now + 50 * 60;
}

static void dm_af_cache_clear(void) {
    if (dm_af_cached) {
        free(dm_af_cached);
        dm_af_cached = NULL;
    }
    dm_af_deadline = 0;
}

static int dm_af_cache_fresh(void) {
    if (!dm_af_cached || dm_af_deadline == 0) return 0;
    time_t now = time(NULL);
    return now < dm_af_deadline - DM_RENEW_BEFORE_SEC;
}

/* 解析 {"token":"af_..."} */
static int dm_parse_af_json(const char *body, char *out, size_t cap) {
    if (!body) return -1;
    cJSON *j = cJSON_Parse(body);
    if (!j) return -1;
    cJSON *t = cJSON_GetObjectItemCaseSensitive(j, "token");
    const char *sv = cJSON_GetStringValue(t);
    if (!sv || strncmp(sv, "af_", 3) != 0) {
        cJSON_Delete(j);
        return -1;
    }
    snprintf(out, cap, "%s", sv);
    cJSON_Delete(j);
    return 0;
}

static int dm_resolve_auto_af(char *tok_out, size_t cap) {
    if (dm_auto_disabled()) return -1;

    if (dm_af_cache_fresh()) {
        snprintf(tok_out, cap, "%s", dm_af_cached);
        return 0;
    }

    const char *rlife = dm_renew_lifetime_env();

    if (dm_af_cached) {
        cJSON *o = cJSON_CreateObject();
        if (!o) return -1;
        cJSON_AddStringToObject(o, "token", dm_af_cached);
        cJSON_AddStringToObject(o, "lifetime", rlife);
        char *jb = cJSON_PrintUnformatted(o);
        cJSON_Delete(o);
        if (!jb) return -1;

        tm_http_response_t *resp = tm_http_request(TM_HTTP_POST, DM_REN_URL, dm_json_headers, jb, 15);
        free(jb);

        if (resp && resp->status == 200 && resp->body && dm_parse_af_json(resp->body, tok_out, cap) == 0) {
            dm_af_cache_clear();
            dm_af_cached = tm_strdup(tok_out);
            if (dm_af_cached) dm_af_deadline = dm_deadline_for_lifetime(rlife);
            tm_http_response_free(resp);
            return 0;
        }
        tm_http_response_free(resp);
        dm_af_cache_clear();
    }

    {
        const char *gen_body = "{\"type\":\"af\",\"lifetime\":\"1h\"}";
        tm_http_response_t *resp = tm_http_request(TM_HTTP_POST, DM_GEN_URL, dm_json_headers, gen_body, 15);
        if (!resp || resp->status != 200 || !resp->body || dm_parse_af_json(resp->body, tok_out, cap) != 0) {
            tm_http_response_free(resp);
            return -1;
        }
        tm_http_response_free(resp);
        dm_af_cache_clear();
        dm_af_cached = tm_strdup(tok_out);
        if (dm_af_cached) dm_af_deadline = time(NULL) + 50 * 60;
        return 0;
    }
}

static int dm_graphql_url(char *url, size_t cap) {
    char tok[512];
    const char *ex = dm_explicit_auth();
    if (ex) {
        snprintf(tok, sizeof(tok), "%s", ex);
    } else {
        if (dm_resolve_auto_af(tok, sizeof(tok)) != 0) return -1;
    }

    CURL *c = curl_easy_init();
    if (!c) return -1;
    char *esc = curl_easy_escape(c, tok, 0);
    curl_easy_cleanup(c);
    if (!esc) return -1;
    snprintf(url, cap, "https://dropmail.me/api/graphql/%s", esc);
    curl_free(esc);
    return 0;
}

tm_email_info_t* tm_provider_dropmail_generate(void) {
    char gql[1024];
    if (dm_graphql_url(gql, sizeof(gql)) != 0) return NULL;

    const char *body = "query=mutation%20%7BintroduceSession%20%7Bid%2C%20expiresAt%2C%20addresses%7Bid%2C%20address%7D%7D%7D";
    tm_http_response_t *resp = tm_http_request(TM_HTTP_POST, gql, dm_form_headers, body, 15);
    if (!resp || resp->status != 200) { tm_http_response_free(resp); return NULL; }

    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) return NULL;

    cJSON *data = cJSON_GetObjectItemCaseSensitive(json, "data");
    cJSON *session = cJSON_GetObjectItemCaseSensitive(data, "introduceSession");
    cJSON *addrs = cJSON_GetObjectItemCaseSensitive(session, "addresses");
    if (!cJSON_IsArray(addrs) || cJSON_GetArraySize(addrs) == 0) { cJSON_Delete(json); return NULL; }

    cJSON *first = cJSON_GetArrayItem(addrs, 0);
    tm_email_info_t *info = tm_email_info_new();
    info->channel = CHANNEL_DROPMAIL;
    info->email = tm_strdup(cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(first, "address")));
    info->token = tm_strdup(cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(session, "id")));
    cJSON_Delete(json);
    return info;
}

tm_email_t* tm_provider_dropmail_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    char gql[1024];
    if (dm_graphql_url(gql, sizeof(gql)) != 0) return NULL;

    char body[1024];
    snprintf(body, sizeof(body),
        "query=query%%20(%%24id%%3A%%20ID!)%%20%%7Bsession(id%%3A%%24id)%%20%%7Bmails%%20%%7Bid%%2C%%20fromAddr%%2C%%20toAddr%%2C%%20receivedAt%%2C%%20text%%2C%%20headerSubject%%2C%%20html%%7D%%7D%%7D&variables=%%7B%%22id%%22%%3A%%22%s%%22%%7D",
        token);

    tm_http_response_t *resp = tm_http_request(TM_HTTP_POST, gql, dm_form_headers, body, 15);
    if (!resp || resp->status != 200) { tm_http_response_free(resp); return NULL; }

    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) return NULL;

    cJSON *data = cJSON_GetObjectItemCaseSensitive(json, "data");
    cJSON *session = cJSON_GetObjectItemCaseSensitive(data, "session");
    cJSON *mails = cJSON_GetObjectItemCaseSensitive(session, "mails");
    int n = cJSON_IsArray(mails) ? cJSON_GetArraySize(mails) : 0;
    *count = n;
    if (n == 0) { cJSON_Delete(json); return NULL; }

    tm_email_t *emails = tm_emails_new(n);
    for (int i = 0; i < n; i++) {
        cJSON *m = cJSON_GetArrayItem(mails, i);
        cJSON *flat = cJSON_CreateObject();
        cJSON_AddStringToObject(flat, "id", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(m, "id"), ""));
        cJSON_AddStringToObject(flat, "from", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(m, "fromAddr"), ""));
        cJSON_AddStringToObject(flat, "to", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(m, "toAddr"), email));
        cJSON_AddStringToObject(flat, "subject", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(m, "headerSubject"), ""));
        cJSON_AddStringToObject(flat, "text", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(m, "text"), ""));
        cJSON_AddStringToObject(flat, "html", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(m, "html"), ""));
        cJSON_AddStringToObject(flat, "received_at", TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(m, "receivedAt"), ""));
        emails[i] = tm_normalize_email(flat, email);
        cJSON_Delete(flat);
    }
    cJSON_Delete(json);
    return emails;
}
