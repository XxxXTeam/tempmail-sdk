/**
 * vip.215.im：POST /api/temp-inbox；收信 libcurl WebSocket（>=7.86）message.new
 * text/html 与其它 SDK对齐 synthetic-v1
 */

#include "tempmail_internal.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if LIBCURL_VERSION_NUM >= 0x07560000
#define TM_VIP215_USE_CURL_WS 1
#endif

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

#define VIP215_HTTP_BASE "https://vip.215.im/api/temp-inbox"
#define VIP215_WS_PATH "wss://vip.215.im/v1/ws?token="
#define VIP215_MARKER "\xe3\x80\x90tempmail-sdk|synthetic|vip-215|v1\xe3\x80\x91"

static const char *vip215_post_headers[] = {
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
    "Cache-Control: no-cache",
    "Content-Type: application/json",
    "DNT: 1",
    "Origin: https://vip.215.im",
    "Pragma: no-cache",
    "Referer: https://vip.215.im/",
    "Sec-CH-UA: \"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"146\"",
    "Sec-CH-UA-Mobile: ?0",
    "Sec-CH-UA-Platform: \"Windows\"",
    "Sec-Fetch-Dest: empty",
    "Sec-Fetch-Mode: cors",
    "Sec-Fetch-Site: same-origin",
    "X-Locale: zh",
    NULL
};

#ifdef _WIN32
static CRITICAL_SECTION g_vip215_cs;
static int g_vip215_cs_inited;
static void vip215_lock(void) { EnterCriticalSection(&g_vip215_cs); }
static void vip215_unlock(void) { LeaveCriticalSection(&g_vip215_cs); }
#else
static pthread_mutex_t g_vip215_mutex = PTHREAD_MUTEX_INITIALIZER;
static void vip215_lock(void) { pthread_mutex_lock(&g_vip215_mutex); }
static void vip215_unlock(void) { pthread_mutex_unlock(&g_vip215_mutex); }
#endif

typedef struct vip215_entry {
    char *token;
    char *recipient;
    tm_email_t *list;
    int count;
    int cap;
    int thread_started;
    struct vip215_entry *next;
} vip215_entry_t;

static vip215_entry_t *g_vip215_head;

void tm_vip215_module_init(void) {
#ifdef _WIN32
    if (!g_vip215_cs_inited) {
        InitializeCriticalSection(&g_vip215_cs);
        g_vip215_cs_inited = 1;
    }
#endif
}

void tm_vip215_module_cleanup(void) {
#ifdef _WIN32
    if (g_vip215_cs_inited) {
        DeleteCriticalSection(&g_vip215_cs);
        g_vip215_cs_inited = 0;
    }
#endif
}

static void vip215_sanitize_inplace(char *s) {
    if (!s) return;
    char *w = s;
    for (char *r = s; *r; r++) {
        if (*r == '\r' || *r == '\n') {
            if (w > s && w[-1] != ' ') *w++ = ' ';
        } else {
            *w++ = *r;
        }
    }
    *w = '\0';
    while (w > s && (w[-1] == ' ' || w[-1] == '\t')) *--w = '\0';
    char *lead = s;
    while (*lead == ' ' || *lead == '\t') lead++;
    if (lead != s) memmove(s, lead, strlen(lead) + 1);
}

static char *vip215_json_item_to_line(const cJSON *item) {
    char buf[512];
    if (cJSON_IsString(item) && item->valuestring) {
        char *d = tm_strdup(item->valuestring);
        vip215_sanitize_inplace(d);
        return d;
    }
    if (cJSON_IsNumber(item)) {
        double x = item->valuedouble;
        if (x == (double)(long long)x)
            snprintf(buf, sizeof(buf), "%lld", (long long)x);
        else
            snprintf(buf, sizeof(buf), "%g", x);
        return tm_strdup(buf);
    }
    if (cJSON_IsBool(item))
        return tm_strdup(cJSON_IsTrue(item) ? "true" : "false");
    return tm_strdup("");
}

static size_t vip215_esc_html_len(const char *s) {
    size_t n = 0;
    for (; s && *s; s++) {
        switch (*s) {
            case '&': n += 5; break;
            case '<': case '>': n += 4; break;
            case '"': n += 6; break;
            default: n++; break;
        }
    }
    return n;
}

static void vip215_esc_html_cat(char *dst, const char *s) {
    char *w = dst;
    for (; s && *s; s++) {
        switch (*s) {
            case '&': memcpy(w, "&amp;", 5); w += 5; break;
            case '<': memcpy(w, "&lt;", 4); w += 4; break;
            case '>': memcpy(w, "&gt;", 4); w += 4; break;
            case '"': memcpy(w, "&quot;", 6); w += 6; break;
            default: *w++ = *s; break;
        }
    }
    *w = '\0';
}

static char *vip215_build_text(const char *recipient, cJSON *data) {
    char *id = vip215_json_item_to_line(cJSON_GetObjectItemCaseSensitive(data, "id"));
    char *subj = vip215_json_item_to_line(cJSON_GetObjectItemCaseSensitive(data, "subject"));
    char *from = vip215_json_item_to_line(cJSON_GetObjectItemCaseSensitive(data, "from"));
    char *to = tm_strdup(recipient ? recipient : "");
    vip215_sanitize_inplace(to);
    char *date = vip215_json_item_to_line(cJSON_GetObjectItemCaseSensitive(data, "date"));

    size_t cap = 512 + strlen(id) + strlen(subj) + strlen(from) + strlen(to) + strlen(date);
    char *out = (char*)malloc(cap);
    if (!out) {
        free(id); free(subj); free(from); free(to); free(date);
        return tm_strdup("");
    }
    int n = snprintf(out, cap, "%s\nid: %s\nsubject: %s\nfrom: %s\nto: %s\ndate: %s",
        VIP215_MARKER, id, subj, from, to, date);
    const cJSON *sz = cJSON_GetObjectItemCaseSensitive(data, "size");
    if (cJSON_IsNumber(sz) && sz->valuedouble >= 0) {
        n += snprintf(out + n, cap > (size_t)n ? cap - (size_t)n : 0, "\nsize: %lld", (long long)sz->valuedouble);
    }
    free(id); free(subj); free(from); free(to); free(date);
    return out;
}

static char *vip215_build_html(const char *recipient, cJSON *data) {
    char *id = vip215_json_item_to_line(cJSON_GetObjectItemCaseSensitive(data, "id"));
    char *subj = vip215_json_item_to_line(cJSON_GetObjectItemCaseSensitive(data, "subject"));
    char *from = vip215_json_item_to_line(cJSON_GetObjectItemCaseSensitive(data, "from"));
    char *to = tm_strdup(recipient ? recipient : "");
    vip215_sanitize_inplace(to);
    char *date = vip215_json_item_to_line(cJSON_GetObjectItemCaseSensitive(data, "date"));

    const char *pairs[5][2] = { {"id", id}, {"subject", subj}, {"from", from}, {"to", to}, {"date", date} };
    size_t inner = 0;
    for (int i = 0; i < 5; i++)
        inner += 9 + vip215_esc_html_len(pairs[i][0]) * 2 + vip215_esc_html_len(pairs[i][1]) + 10;

    char szbuf[32];
    szbuf[0] = '\0';
    const cJSON *sz = cJSON_GetObjectItemCaseSensitive(data, "size");
    if (cJSON_IsNumber(sz) && sz->valuedouble >= 0) {
        snprintf(szbuf, sizeof(szbuf), "%lld", (long long)sz->valuedouble);
        inner += 9 + 4 + 4 + vip215_esc_html_len(szbuf) + 10;
    }

    size_t total = inner + 200;
    char *html = (char*)malloc(total);
    if (!html) {
        free(id); free(subj); free(from); free(to); free(date);
        return tm_strdup("");
    }
    char *w = html;
    w += sprintf(w, "<div class=\"tempmail-sdk-synthetic\" data-tempmail-sdk-format=\"synthetic-v1\" data-channel=\"vip-215\"><dl class=\"tempmail-sdk-meta\">");

    for (int i = 0; i < 5; i++) {
        *w++ = '<'; *w++ = 'd'; *w++ = 't'; *w++ = '>';
        vip215_esc_html_cat(w, pairs[i][0]);
        w += strlen(w);
        memcpy(w, "</dt><dd>", 8); w += 8;
        vip215_esc_html_cat(w, pairs[i][1]);
        w += strlen(w);
        memcpy(w, "</dd>", 5); w += 5;
    }
    if (szbuf[0]) {
        memcpy(w, "<dt>size</dt><dd>", 17); w += 17;
        vip215_esc_html_cat(w, szbuf);
        w += strlen(w);
        memcpy(w, "</dd>", 5); w += 5;
    }
    memcpy(w, "</dl></div>", 11);
    w += 11;
    *w = '\0';

    free(id); free(subj); free(from); free(to); free(date);
    return html;
}

static cJSON *vip215_build_raw(const char *recipient, cJSON *data) {
    cJSON *raw = cJSON_CreateObject();
    if (!raw) return NULL;

    const char *copy_keys[] = {"id", "from", "subject", "date"};
    for (size_t i = 0; i < sizeof(copy_keys) / sizeof(copy_keys[0]); i++) {
        const cJSON *it = cJSON_GetObjectItemCaseSensitive(data, copy_keys[i]);
        if (cJSON_IsString(it))
            cJSON_AddStringToObject(raw, copy_keys[i], it->valuestring);
        else if (cJSON_IsNumber(it)) {
            char b[64];
            snprintf(b, sizeof(b), "%g", it->valuedouble);
            cJSON_AddStringToObject(raw, copy_keys[i], b);
        } else
            cJSON_AddStringToObject(raw, copy_keys[i], "");
    }
    cJSON_AddStringToObject(raw, "to", recipient ? recipient : "");

    char *t = vip215_build_text(recipient, data);
    char *h = vip215_build_html(recipient, data);
    cJSON_AddStringToObject(raw, "text", t ? t : "");
    cJSON_AddStringToObject(raw, "html", h ? h : "");
    free(t);
    free(h);
    return raw;
}

static int vip215_has_id(vip215_entry_t *e, const char *id) {
    if (!id || !id[0]) return 1;
    for (int i = 0; i < e->count; i++) {
        if (e->list[i].id && strcmp(e->list[i].id, id) == 0) return 1;
    }
    return 0;
}

static int vip215_append(vip215_entry_t *e, tm_email_t *em) {
    if (!em->id || !em->id[0]) return 0;
    if (vip215_has_id(e, em->id)) {
        tm_free_email(em);
        memset(em, 0, sizeof(*em));
        return 0;
    }
    if (e->count >= e->cap) {
        int ncap = e->cap ? e->cap * 2 : 8;
        tm_email_t *nl = (tm_email_t*)realloc(e->list, (size_t)ncap * sizeof(tm_email_t));
        if (!nl) return 0;
        e->list = nl;
        e->cap = ncap;
    }
    e->list[e->count++] = *em;
    memset(em, 0, sizeof(*em));
    return 1;
}

static vip215_entry_t *vip215_find_or_create(const char *token, const char *recipient) {
    vip215_lock();
    for (vip215_entry_t *p = g_vip215_head; p; p = p->next) {
        if (strcmp(p->token, token) == 0) {
            vip215_unlock();
            return p;
        }
    }
    vip215_entry_t *e = (vip215_entry_t*)calloc(1, sizeof(vip215_entry_t));
    if (e) {
        e->token = tm_strdup(token);
        e->recipient = tm_strdup(recipient);
        e->next = g_vip215_head;
        g_vip215_head = e;
    }
    vip215_unlock();
    return e;
}

#ifdef TM_VIP215_USE_CURL_WS
static void vip215_apply_config(CURL *curl) {
    const tm_config_t *cfg = tm_get_config();
    int to = (cfg && cfg->timeout_secs > 0) ? cfg->timeout_secs : 15;
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)to);
    if (cfg && cfg->insecure) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    } else {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
#ifdef _WIN32
        curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, (long)CURLSSLOPT_NATIVE_CA);
#endif
    }
    if (cfg && cfg->proxy && cfg->proxy[0])
        curl_easy_setopt(curl, CURLOPT_PROXY, cfg->proxy);
}

#ifdef _WIN32
static unsigned __stdcall vip215_ws_thread(void *arg)
#else
static void *vip215_ws_thread(void *arg)
#endif
{
    vip215_entry_t *e = (vip215_entry_t*)arg;
    if (!e || !e->token || !e->recipient) {
#ifdef _WIN32
        return 0;
#else
        return NULL;
#endif
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
#ifdef _WIN32
        return 0;
#else
        return NULL;
#endif
    }

    char *esc = curl_easy_escape(curl, e->token, 0);
    if (!esc) {
        curl_easy_cleanup(curl);
#ifdef _WIN32
        return 0;
#else
        return NULL;
#endif
    }
    size_t ulen = strlen(VIP215_WS_PATH) + strlen(esc) + 1;
    char *url = (char*)malloc(ulen);
    if (!url) {
        curl_free(esc);
        curl_easy_cleanup(curl);
#ifdef _WIN32
        return 0;
#else
        return NULL;
#endif
    }
    snprintf(url, ulen, "%s%s", VIP215_WS_PATH, esc);
    curl_free(esc);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 2L);
    vip215_apply_config(curl);

    struct curl_slist *hl = NULL;
    hl = curl_slist_append(hl, "Origin: https://vip.215.im");
    hl = curl_slist_append(hl, vip215_post_headers[0]);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hl);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(hl);
    free(url);

    if (res != CURLE_OK) {
        TM_LOG_WARN("vip-215 WebSocket 连接失败: %s", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
#ifdef _WIN32
        return 0;
#else
        return NULL;
#endif
    }

    unsigned char buf[65536];
    for (;;) {
        size_t rlen = 0;
        const struct curl_ws_frame *meta = NULL;
        res = curl_ws_recv(curl, buf, sizeof(buf) - 1, &rlen, &meta);
        if (res == CURLE_GOT_NOTHING) break;
        if (res == CURLE_AGAIN) {
#ifdef _WIN32
            Sleep(10);
#else
            usleep(10000);
#endif
            continue;
        }
        if (res != CURLE_OK) break;
        if (meta && !(meta->flags & CURLWS_TEXT) && !(meta->flags & CURLWS_BINARY))
            continue;
        buf[rlen] = '\0';

        cJSON *root = cJSON_Parse((char*)buf);
        if (!root) continue;
        cJSON *typ = cJSON_GetObjectItemCaseSensitive(root, "type");
        if (!cJSON_IsString(typ) || strcmp(typ->valuestring, "message.new") != 0) {
            cJSON_Delete(root);
            continue;
        }
        cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
        if (!data || !cJSON_IsObject(data)) {
            cJSON_Delete(root);
            continue;
        }
        cJSON *raw = vip215_build_raw(e->recipient, data);
        if (raw) {
            tm_email_t em = tm_normalize_email(raw, e->recipient);
            cJSON_Delete(raw);
            vip215_lock();
            vip215_append(e, &em);
            vip215_unlock();
        }
        cJSON_Delete(root);
    }

    curl_easy_cleanup(curl);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static void vip215_start_reader(vip215_entry_t *e) {
    if (!e) return;
    vip215_lock();
    if (!e->thread_started) {
        e->thread_started = 1;
#ifdef _WIN32
        uintptr_t h = _beginthreadex(NULL, 0, vip215_ws_thread, e, 0, NULL);
        if (h) CloseHandle((HANDLE)h);
#else
        pthread_t t;
        if (pthread_create(&t, NULL, vip215_ws_thread, e) == 0)
            pthread_detach(t);
#endif
    }
    vip215_unlock();
}
#endif /* TM_VIP215_USE_CURL_WS */

tm_email_info_t* tm_provider_vip215_generate(void) {
    tm_http_response_t *resp = tm_http_request(TM_HTTP_POST, VIP215_HTTP_BASE, vip215_post_headers, NULL, 15);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }

    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) return NULL;

    cJSON *ok = cJSON_GetObjectItemCaseSensitive(json, "success");
    if (!cJSON_IsBool(ok) || !cJSON_IsTrue(ok)) {
        cJSON_Delete(json);
        return NULL;
    }
    cJSON *data = cJSON_GetObjectItemCaseSensitive(json, "data");
    if (!data || !cJSON_IsObject(data)) {
        cJSON_Delete(json);
        return NULL;
    }
    const char *addr = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(data, "address"));
    const char *tok = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(data, "token"));
    if (!addr || !tok) {
        cJSON_Delete(json);
        return NULL;
    }

    tm_email_info_t *info = tm_email_info_new();
    info->channel = CHANNEL_VIP_215;
    info->email = tm_strdup(addr);
    info->token = tm_strdup(tok);
    cJSON *ca = cJSON_GetObjectItemCaseSensitive(data, "createdAt");
    if (cJSON_IsString(ca) && ca->valuestring)
        info->created_at = tm_strdup(ca->valuestring);

    cJSON_Delete(json);
    return info;
}

static tm_email_t *vip215_dup_list(const tm_email_t *src, int n) {
    if (n <= 0) return NULL;
    tm_email_t *out = tm_emails_new(n);
    if (!out) return NULL;
    for (int i = 0; i < n; i++) {
        out[i].id = tm_strdup(src[i].id);
        out[i].from_addr = tm_strdup(src[i].from_addr);
        out[i].to = tm_strdup(src[i].to);
        out[i].subject = tm_strdup(src[i].subject);
        out[i].text = tm_strdup(src[i].text);
        out[i].html = tm_strdup(src[i].html);
        out[i].date = tm_strdup(src[i].date);
        out[i].is_read = src[i].is_read;
        out[i].attachments = NULL;
        out[i].attachment_count = 0;
    }
    return out;
}

tm_email_t* tm_provider_vip215_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    if (!token || !email) return NULL;

#ifndef TM_VIP215_USE_CURL_WS
    TM_LOG_WARN("vip-215: 当前 libcurl 版本 < 7.86，无 WebSocket，无法收信（请升级 libcurl）");
    *count = 0;
    return NULL;
#else
    vip215_entry_t *e = vip215_find_or_create(token, email);
    if (!e) return NULL;

    vip215_start_reader(e);
#ifdef _WIN32
    Sleep(80);
#else
    usleep(80000);
#endif

    vip215_lock();
    int n = e->count;
    tm_email_t *dup = vip215_dup_list(e->list, n);
    vip215_unlock();

    *count = n;
    return dup;
#endif
}
