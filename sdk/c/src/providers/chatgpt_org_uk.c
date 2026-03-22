/**
 * mail.chatgpt.org.uk 渠道实现
 * 列表接口需 Cookie: gm_sid=… 与 x-inbox-token 同时存在。
 */
#include "tempmail_internal.h"

#define CG_BASE "https://mail.chatgpt.org.uk/api"
#define CG_HOME "https://mail.chatgpt.org.uk/"

static const char* cg_headers[] = {
    "Accept: */*",
    "Referer: https://mail.chatgpt.org.uk/",
    "Origin: https://mail.chatgpt.org.uk",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36",
    "DNT: 1",
    NULL
};

static char* cg_extract_browser_auth(const char *html) {
    if (!html) return NULL;
    const char *p = strstr(html, "__BROWSER_AUTH");
    if (!p) return NULL;
    const char *tok = strstr(p, "\"token\"");
    if (!tok) return NULL;
    const char *colon = strchr(tok, ':');
    if (!colon) return NULL;
    const char *q = colon + 1;
    while (*q == ' ' || *q == '\t') q++;
    if (*q != '"') return NULL;
    q++;
    const char *end = strchr(q, '"');
    if (!end) return NULL;
    size_t len = (size_t)(end - q);
    if (len == 0) return NULL;
    char *out = (char*)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, q, len);
    out[len] = '\0';
    return out;
}

/** cookie_line: "gm_sid=VALUE"（与 http 层存储一致） */
static char* cg_gm_sid_value(const char *cookie_line) {
    if (!cookie_line || strncmp(cookie_line, "gm_sid=", 7) != 0) return NULL;
    return tm_strdup(cookie_line + 7);
}

static int cg_retry_home(long status, const char *cookie_line, const char *browser) {
    if (status == 401) return 1;
    if (!cookie_line || !browser) return 1;
    return 0;
}

static int cg_fetch_home_session(char **cookie_line_out, char **browser_out, long *status_out) {
    if (status_out) *status_out = 0;
    *cookie_line_out = NULL;
    *browser_out = NULL;

    tm_http_response_t *home = tm_http_request(TM_HTTP_GET, CG_HOME, cg_headers, NULL, 15);
    if (!home) return -1;
    if (status_out) *status_out = home->status;
    if (home->status != 200) {
        tm_http_response_free(home);
        return -1;
    }
    if (!home->cookies || strncmp(home->cookies, "gm_sid=", 7) != 0) {
        tm_http_response_free(home);
        return -1;
    }

    *cookie_line_out = tm_strdup(home->cookies);
    *browser_out = cg_extract_browser_auth(home->body ? home->body : "");
    tm_http_response_free(home);

    if (!*cookie_line_out || !*browser_out) {
        free(*cookie_line_out);
        free(*browser_out);
        *cookie_line_out = NULL;
        *browser_out = NULL;
        return -1;
    }
    return 0;
}

static int cg_fetch_home_session_with_retry(char **cookie_line_out, char **browser_out) {
    long st = 0;
    if (cg_fetch_home_session(cookie_line_out, browser_out, &st) == 0)
        return 0;
    if (cg_retry_home(st, *cookie_line_out, *browser_out)) {
        free(*cookie_line_out);
        free(*browser_out);
        *cookie_line_out = NULL;
        *browser_out = NULL;
        return cg_fetch_home_session(cookie_line_out, browser_out, &st);
    }
    return -1;
}

static char* cg_fetch_inbox_token_once(const char *email, const char *cookie_line, long *status_out) {
    if (status_out) *status_out = 0;
    char body[512];
    snprintf(body, sizeof(body), "{\"email\":\"%s\"}", email);

    char cookie_hdr[512];
    snprintf(cookie_hdr, sizeof(cookie_hdr), "Cookie: %s", cookie_line);

    const char* headers[] = {
        "Accept: */*",
        "Referer: https://mail.chatgpt.org.uk/",
        "Origin: https://mail.chatgpt.org.uk",
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36",
        "DNT: 1",
        "Content-Type: application/json",
        cookie_hdr,
        NULL
    };

    tm_http_response_t *resp = tm_http_request(TM_HTTP_POST, CG_BASE "/inbox-token", headers, body, 15);
    if (!resp) return NULL;
    if (status_out) *status_out = resp->status;
    if (resp->status != 200) { tm_http_response_free(resp); return NULL; }

    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) return NULL;

    cJSON *auth = cJSON_GetObjectItemCaseSensitive(json, "auth");
    const char *token = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(auth, "token"));
    if (!token || !token[0]) { cJSON_Delete(json); return NULL; }

    char *out = tm_strdup(token);
    cJSON_Delete(json);
    return out;
}

static char* cg_fetch_inbox_token(const char *email) {
    char *cookie_line = NULL;
    char *browser = NULL;
    if (cg_fetch_home_session_with_retry(&cookie_line, &browser) != 0) {
        free(browser);
        return NULL;
    }
    free(browser);

    long status = 0;
    char *token = cg_fetch_inbox_token_once(email, cookie_line, &status);
    if (!token && status == 401) {
        free(cookie_line);
        cookie_line = NULL;
        browser = NULL;
        if (cg_fetch_home_session_with_retry(&cookie_line, &browser) != 0) {
            free(browser);
            return NULL;
        }
        free(browser);
        token = cg_fetch_inbox_token_once(email, cookie_line, &status);
    }
    free(cookie_line);
    return token;
}

/** 同一首页会话下刷新 inbox JWT 与 gm_sid，供 401 后重试 */
static int cg_refresh_inbox_pair(const char *email, char **inbox_out, char **gm_val_out) {
    char *cookie_line = NULL;
    char *browser = NULL;
    if (cg_fetch_home_session_with_retry(&cookie_line, &browser) != 0) {
        free(browser);
        return -1;
    }
    free(browser);

    char *inbox = cg_fetch_inbox_token_once(email, cookie_line, NULL);
    char *gv = cg_gm_sid_value(cookie_line);
    free(cookie_line);
    if (!inbox || !gv) {
        free(inbox);
        free(gv);
        return -1;
    }
    *inbox_out = inbox;
    *gm_val_out = gv;
    return 0;
}

static char* cg_pack_token_json(const char *gm_val, const char *inbox) {
    cJSON *o = cJSON_CreateObject();
    if (!o) return NULL;
    cJSON_AddStringToObject(o, "gmSid", gm_val);
    cJSON_AddStringToObject(o, "inbox", inbox);
    char *s = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    return s;
}

static int cg_parse_packed(const char *token, char **gm_val_out, char **inbox_out) {
    *gm_val_out = NULL;
    *inbox_out = NULL;
    if (!token || !token[0]) return -1;

    if (token[0] == '{') {
        cJSON *j = cJSON_Parse(token);
        if (j) {
            const char *gs = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(j, "gmSid"));
            const char *ib = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(j, "inbox"));
            if (gs && ib && gs[0] && ib[0]) {
                *gm_val_out = tm_strdup(gs);
                *inbox_out = tm_strdup(ib);
                cJSON_Delete(j);
                return 0;
            }
            cJSON_Delete(j);
        }
    }

    *inbox_out = tm_strdup(token);
    return 0;
}

static tm_email_t* cg_fetch_emails_once(const char *inbox_token, const char *email,
    const char *gm_val, int *count, long *status_out) {
    if (count) *count = -1;
    if (status_out) *status_out = 0;
    if (!inbox_token || !inbox_token[0] || !gm_val || !gm_val[0]) return NULL;

    char url[512];
    /* 简单拼接；邮箱一般为安全字符 */
    snprintf(url, sizeof(url), CG_BASE "/emails?email=%s", email);

    char token_hdr[2048];
    snprintf(token_hdr, sizeof(token_hdr), "x-inbox-token: %s", inbox_token);

    char cookie_hdr[512];
    snprintf(cookie_hdr, sizeof(cookie_hdr), "Cookie: gm_sid=%s", gm_val);

    const char* headers[] = {
        "Accept: */*",
        "Referer: https://mail.chatgpt.org.uk/",
        "Origin: https://mail.chatgpt.org.uk",
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36",
        "DNT: 1",
        cookie_hdr,
        token_hdr,
        NULL
    };

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, headers, NULL, 15);
    if (!resp) return NULL;
    if (status_out) *status_out = resp->status;
    if (resp->status != 200) { tm_http_response_free(resp); return NULL; }

    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) return NULL;

    if (!cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(json, "success"))) { cJSON_Delete(json); return NULL; }

    cJSON *data = cJSON_GetObjectItemCaseSensitive(json, "data");
    cJSON *arr = cJSON_GetObjectItemCaseSensitive(data, "emails");
    int n = cJSON_IsArray(arr) ? cJSON_GetArraySize(arr) : 0;
    if (count) *count = n;
    if (n == 0) { cJSON_Delete(json); return NULL; }

    tm_email_t *emails = tm_emails_new(n);
    for (int i = 0; i < n; i++) emails[i] = tm_normalize_email(cJSON_GetArrayItem(arr, i), email);
    cJSON_Delete(json);
    return emails;
}


tm_email_info_t* tm_provider_chatgpt_org_uk_generate(void) {
    char *cookie_line = NULL;
    char *browser = NULL;
    if (cg_fetch_home_session_with_retry(&cookie_line, &browser) != 0) {
        free(browser);
        return NULL;
    }

    char xtok_hdr[4096];
    snprintf(xtok_hdr, sizeof(xtok_hdr), "X-Inbox-Token: %s", browser);
    free(browser);

    char cookie_hdr[512];
    snprintf(cookie_hdr, sizeof(cookie_hdr), "Cookie: %s", cookie_line);

    const char* gen_headers[] = {
        "Accept: */*",
        "Referer: https://mail.chatgpt.org.uk/",
        "Origin: https://mail.chatgpt.org.uk",
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36",
        "DNT: 1",
        cookie_hdr,
        xtok_hdr,
        NULL
    };

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, CG_BASE "/generate-email", gen_headers, NULL, 15);
    if (!resp || resp->status != 200) { tm_http_response_free(resp); free(cookie_line); return NULL; }

    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) { free(cookie_line); return NULL; }

    if (!cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(json, "success"))) { cJSON_Delete(json); free(cookie_line); return NULL; }

    cJSON *data = cJSON_GetObjectItemCaseSensitive(json, "data");
    const char *email = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(data, "email"));
    if (!email || !email[0]) { cJSON_Delete(json); free(cookie_line); return NULL; }

    char *inbox_jwt = NULL;
    cJSON *auth = cJSON_GetObjectItemCaseSensitive(json, "auth");
    if (auth) {
        const char *t = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(auth, "token"));
        if (t && t[0]) inbox_jwt = tm_strdup(t);
    }
    if (!inbox_jwt) {
        inbox_jwt = cg_fetch_inbox_token_once(email, cookie_line, NULL);
    }

    char *gm_val = cg_gm_sid_value(cookie_line);
    free(cookie_line);

    if (!inbox_jwt || !gm_val) {
        free(inbox_jwt);
        free(gm_val);
        cJSON_Delete(json);
        return NULL;
    }

    char *packed = cg_pack_token_json(gm_val, inbox_jwt);
    free(gm_val);
    free(inbox_jwt);
    cJSON_Delete(json);

    if (!packed) return NULL;

    tm_email_info_t *info = tm_email_info_new();
    info->channel = CHANNEL_CHATGPT_ORG_UK;
    info->email = tm_strdup(email);
    info->token = packed;
    return info;
}


tm_email_t* tm_provider_chatgpt_org_uk_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    if (!token || !token[0]) return NULL;

    char *gm_val = NULL;
    char *inbox = NULL;
    if (cg_parse_packed(token, &gm_val, &inbox) != 0 || !inbox) {
        free(gm_val);
        free(inbox);
        return NULL;
    }

    if (!gm_val) {
        char *cookie_line = NULL;
        char *browser = NULL;
        if (cg_fetch_home_session_with_retry(&cookie_line, &browser) != 0) {
            free(browser);
            free(inbox);
            return NULL;
        }
        free(browser);
        gm_val = cg_gm_sid_value(cookie_line);
        free(cookie_line);
        if (!gm_val) {
            free(inbox);
            return NULL;
        }
    }

    long status = 0;
    tm_email_t *emails = cg_fetch_emails_once(inbox, email, gm_val, count, &status);

    if (status == 401) {
        free(emails);
        char *ref_in = NULL;
        char *ref_gv = NULL;
        if (cg_refresh_inbox_pair(email, &ref_in, &ref_gv) == 0) {
            emails = cg_fetch_emails_once(ref_in, email, ref_gv, count, &status);
            free(ref_in);
            free(ref_gv);
        }
    }

    free(gm_val);
    free(inbox);
    return emails;
}
