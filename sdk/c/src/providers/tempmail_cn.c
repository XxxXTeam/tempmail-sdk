/**
 * tempmail.cn：按 public 前端的 Socket.IO 事件协议工作
 * - `request shortid` -> `shortid`
 * - `set shortid` 持续订阅 `mail`
 */

#include "tempmail_internal.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if LIBCURL_VERSION_NUM >= 0x07560000
#define TM_TMCN_USE_CURL_WS 1
#endif

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

#define TMCN_DEFAULT_HOST "tempmail.cn"

static const char *tmcn_common_headers[] = {
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
    "Cache-Control: no-cache",
    "DNT: 1",
    "Pragma: no-cache",
    NULL
};

#ifdef _WIN32
static CRITICAL_SECTION g_tmcn_cs;
static int g_tmcn_cs_inited;
static void tmcn_lock(void) { EnterCriticalSection(&g_tmcn_cs); }
static void tmcn_unlock(void) { LeaveCriticalSection(&g_tmcn_cs); }
#else
static pthread_mutex_t g_tmcn_mutex = PTHREAD_MUTEX_INITIALIZER;
static void tmcn_lock(void) { pthread_mutex_lock(&g_tmcn_mutex); }
static void tmcn_unlock(void) { pthread_mutex_unlock(&g_tmcn_mutex); }
#endif

typedef struct tmcn_entry {
    char *email;
    char *local;
    char *host;
    tm_email_t *list;
    int count;
    int cap;
    int thread_started;
    struct tmcn_entry *next;
} tmcn_entry_t;

static tmcn_entry_t *g_tmcn_head;

void tm_tempmail_cn_module_init(void) {
#ifdef _WIN32
    if (!g_tmcn_cs_inited) {
        InitializeCriticalSection(&g_tmcn_cs);
        g_tmcn_cs_inited = 1;
    }
#endif
}

void tm_tempmail_cn_module_cleanup(void) {
#ifdef _WIN32
    if (g_tmcn_cs_inited) {
        DeleteCriticalSection(&g_tmcn_cs);
        g_tmcn_cs_inited = 0;
    }
#endif
}

static void tmcn_sleep_ms(int ms) {
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    usleep((useconds_t)ms * 1000);
#endif
}

static char *tmcn_substr(const char *s, size_t len) {
    char *out = (char*)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}

static char *tmcn_normalize_host(const char *domain) {
    const char *raw = (domain && domain[0]) ? domain : TMCN_DEFAULT_HOST;
    char *host = tm_strdup(raw);
    char *p;
    if (!host) return NULL;
    p = strstr(host, "://");
    if (p) memmove(host, p + 3, strlen(p + 3) + 1);
    p = strrchr(host, '@');
    if (p) memmove(host, p + 1, strlen(p + 1) + 1);
    p = strchr(host, '/');
    if (p) *p = '\0';
    while (*host == '.') memmove(host, host + 1, strlen(host));
    {
        size_t n = strlen(host);
        while (n > 0 && host[n - 1] == '.') host[--n] = '\0';
    }
    if (!host[0]) {
        free(host);
        return tm_strdup(TMCN_DEFAULT_HOST);
    }
    return host;
}

static int tmcn_split_email(const char *email, char **local, char **host) {
    const char *at;
    if (!email) return 0;
    at = strchr(email, '@');
    if (!at || at == email || !at[1]) return 0;
    *local = tmcn_substr(email, (size_t)(at - email));
    *host = tmcn_normalize_host(at + 1);
    return *local && *host;
}

static void tmcn_apply_config(CURL *curl) {
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

#ifdef TM_TMCN_USE_CURL_WS
static int tmcn_ws_send_text(CURL *curl, const char *text) {
    size_t sent = 0;
    CURLcode res;
    if (!curl || !text) return 0;
    res = curl_ws_send(curl, text, strlen(text), &sent, 0, CURLWS_TEXT);
    return res == CURLE_OK && sent == strlen(text);
}

static int tmcn_ws_recv_text(CURL *curl, char *buf, size_t cap) {
    for (;;) {
        size_t rlen = 0;
        const struct curl_ws_frame *meta = NULL;
        CURLcode res = curl_ws_recv(curl, buf, cap - 1, &rlen, &meta);
        if (res == CURLE_AGAIN) {
            tmcn_sleep_ms(10);
            continue;
        }
        if (res != CURLE_OK || res == CURLE_GOT_NOTHING) return 0;
        if (meta && !(meta->flags & CURLWS_TEXT) && !(meta->flags & CURLWS_BINARY))
            continue;
        buf[rlen] = '\0';
        return 1;
    }
}

static CURL *tmcn_connect_socket(const char *host) {
    int versions[] = {4, 3};
    size_t i;
    for (i = 0; i < sizeof(versions) / sizeof(versions[0]); i++) {
        CURL *curl = curl_easy_init();
        struct curl_slist *hl = NULL;
        char url[768];
        char origin[384];
        char referer[384];
        char packet[8192];
        if (!curl) return NULL;

        snprintf(url, sizeof(url), "wss://%s/socket.io/?EIO=%d&transport=websocket", host, versions[i]);
        snprintf(origin, sizeof(origin), "Origin: https://%s", host);
        snprintf(referer, sizeof(referer), "Referer: https://%s/", host);

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 2L);
        tmcn_apply_config(curl);

        hl = curl_slist_append(hl, tmcn_common_headers[0]);
        hl = curl_slist_append(hl, tmcn_common_headers[1]);
        hl = curl_slist_append(hl, tmcn_common_headers[2]);
        hl = curl_slist_append(hl, tmcn_common_headers[3]);
        hl = curl_slist_append(hl, tmcn_common_headers[4]);
        hl = curl_slist_append(hl, origin);
        hl = curl_slist_append(hl, referer);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hl);

        if (curl_easy_perform(curl) == CURLE_OK && tmcn_ws_recv_text(curl, packet, sizeof(packet)) && packet[0] == '0' && tmcn_ws_send_text(curl, "40")) {
            curl_slist_free_all(hl);
            return curl;
        }
        curl_slist_free_all(hl);
        curl_easy_cleanup(curl);
    }
    return NULL;
}

static int tmcn_send_event(CURL *curl, const char *event, cJSON *payload) {
    cJSON *arr = cJSON_CreateArray();
    char *json;
    char *packet;
    size_t cap;
    int ok;
    if (!arr) return 0;
    cJSON_AddItemToArray(arr, cJSON_CreateString(event));
    cJSON_AddItemToArray(arr, payload ? payload : cJSON_CreateNull());
    json = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);
    if (!json) return 0;
    cap = strlen(json) + 3;
    packet = (char*)malloc(cap);
    if (!packet) {
        free(json);
        return 0;
    }
    snprintf(packet, cap, "42%s", json);
    free(json);
    ok = tmcn_ws_send_text(curl, packet);
    free(packet);
    return ok;
}
#endif

static char *tmcn_json_to_string(const cJSON *item) {
    if (!item) return tm_strdup("");
    if (cJSON_IsString(item) && item->valuestring) return tm_strdup(item->valuestring);
    if (cJSON_IsNumber(item)) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%g", item->valuedouble);
        return tm_strdup(buf);
    }
    if (cJSON_IsBool(item)) return tm_strdup(cJSON_IsTrue(item) ? "true" : "false");
    return tm_strdup("");
}

static char *tmcn_stable_id(cJSON *raw, cJSON *headers, const char *recipient) {
    const char *raw_keys[] = {"id", "messageId"};
    const char *hdr_keys[] = {"message-id", "messageId"};
    int i;
    for (i = 0; i < 2; i++) {
        char *s = tmcn_json_to_string(cJSON_GetObjectItemCaseSensitive(raw, raw_keys[i]));
        if (s && s[0]) return s;
        free(s);
    }
    for (i = 0; i < 2; i++) {
        char *s = tmcn_json_to_string(cJSON_GetObjectItemCaseSensitive(headers, hdr_keys[i]));
        if (s && s[0]) return s;
        free(s);
    }
    {
        char *from = tmcn_json_to_string(cJSON_GetObjectItemCaseSensitive(headers, "from"));
        char *subject = tmcn_json_to_string(cJSON_GetObjectItemCaseSensitive(headers, "subject"));
        char *date = tmcn_json_to_string(cJSON_GetObjectItemCaseSensitive(headers, "date"));
        size_t cap = strlen(from) + strlen(subject) + strlen(date) + strlen(recipient ? recipient : "") + 8;
        char *out = (char*)malloc(cap);
        if (out) snprintf(out, cap, "%s\n%s\n%s\n%s", from, subject, date, recipient ? recipient : "");
        free(from); free(subject); free(date);
        return out ? out : tm_strdup("");
    }
}

static cJSON *tmcn_build_flat_raw(const char *recipient, cJSON *data) {
    cJSON *headers = cJSON_GetObjectItemCaseSensitive(data, "headers");
    cJSON *raw = cJSON_CreateObject();
    char *id;
    char *from;
    char *subject;
    char *text;
    char *html;
    char *date;
    cJSON *attachments;
    if (!headers || !cJSON_IsObject(headers)) headers = cJSON_CreateObject();
    if (!raw) return NULL;
    id = tmcn_stable_id(data, headers, recipient);
    from = tmcn_json_to_string(cJSON_GetObjectItemCaseSensitive(headers, "from"));
    subject = tmcn_json_to_string(cJSON_GetObjectItemCaseSensitive(headers, "subject"));
    text = tmcn_json_to_string(cJSON_GetObjectItemCaseSensitive(data, "text"));
    html = tmcn_json_to_string(cJSON_GetObjectItemCaseSensitive(data, "html"));
    date = tmcn_json_to_string(cJSON_GetObjectItemCaseSensitive(headers, "date"));
    cJSON_AddStringToObject(raw, "id", id ? id : "");
    cJSON_AddStringToObject(raw, "from", from ? from : "");
    cJSON_AddStringToObject(raw, "to", recipient ? recipient : "");
    cJSON_AddStringToObject(raw, "subject", subject ? subject : "");
    cJSON_AddStringToObject(raw, "text", text ? text : "");
    cJSON_AddStringToObject(raw, "html", html ? html : "");
    cJSON_AddStringToObject(raw, "date", date ? date : "");
    attachments = cJSON_GetObjectItemCaseSensitive(data, "attachments");
    cJSON_AddItemToObject(raw, "attachments", attachments && cJSON_IsArray(attachments) ? cJSON_Duplicate(attachments, 1) : cJSON_CreateArray());
    free(id); free(from); free(subject); free(text); free(html); free(date);
    if (headers && headers != cJSON_GetObjectItemCaseSensitive(data, "headers")) cJSON_Delete(headers);
    return raw;
}

static int tmcn_has_id(tmcn_entry_t *e, const char *id) {
    int i;
    if (!id || !id[0]) return 1;
    for (i = 0; i < e->count; i++) {
        if (e->list[i].id && strcmp(e->list[i].id, id) == 0) return 1;
    }
    return 0;
}

static int tmcn_append(tmcn_entry_t *e, tm_email_t *em) {
    if (!em->id || !em->id[0]) return 0;
    if (tmcn_has_id(e, em->id)) {
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

static tmcn_entry_t *tmcn_find_or_create(const char *email, const char *local, const char *host) {
    tmcn_entry_t *p;
    tmcn_entry_t *e;
    tmcn_lock();
    for (p = g_tmcn_head; p; p = p->next) {
        if (p->email && strcmp(p->email, email) == 0) {
            tmcn_unlock();
            return p;
        }
    }
    e = (tmcn_entry_t*)calloc(1, sizeof(tmcn_entry_t));
    if (e) {
        e->email = tm_strdup(email);
        e->local = tm_strdup(local);
        e->host = tm_strdup(host);
        e->next = g_tmcn_head;
        g_tmcn_head = e;
    }
    tmcn_unlock();
    return e;
}

static tm_email_t *tmcn_dup_list(const tm_email_t *src, int n) {
    int i;
    tm_email_t *out;
    if (n <= 0) return NULL;
    out = tm_emails_new(n);
    if (!out) return NULL;
    for (i = 0; i < n; i++) {
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

#ifdef TM_TMCN_USE_CURL_WS
#ifdef _WIN32
static unsigned __stdcall tmcn_ws_thread(void *arg)
#else
static void *tmcn_ws_thread(void *arg)
#endif
{
    tmcn_entry_t *e = (tmcn_entry_t*)arg;
    CURL *curl;
    char packet[65536];
    if (!e || !e->email || !e->local || !e->host) {
#ifdef _WIN32
        return 0;
#else
        return NULL;
#endif
    }
    curl = tmcn_connect_socket(e->host);
    if (!curl || !tmcn_send_event(curl, "set shortid", cJSON_CreateString(e->local))) {
        if (curl) curl_easy_cleanup(curl);
        tmcn_lock(); e->thread_started = 0; tmcn_unlock();
#ifdef _WIN32
        return 0;
#else
        return NULL;
#endif
    }
    while (tmcn_ws_recv_text(curl, packet, sizeof(packet))) {
        if (strcmp(packet, "2") == 0) {
            if (!tmcn_ws_send_text(curl, "3")) break;
            continue;
        }
        if (strncmp(packet, "42", 2) == 0) {
            cJSON *arr = cJSON_Parse(packet + 2);
            cJSON *event = arr ? cJSON_GetArrayItem(arr, 0) : NULL;
            cJSON *payload = arr ? cJSON_GetArrayItem(arr, 1) : NULL;
            if (arr && cJSON_IsString(event) && strcmp(TM_JSON_STR(event, ""), "mail") == 0 && cJSON_IsObject(payload)) {
                cJSON *raw = tmcn_build_flat_raw(e->email, payload);
                if (raw) {
                    tm_email_t em = tm_normalize_email(raw, e->email);
                    cJSON_Delete(raw);
                    tmcn_lock();
                    tmcn_append(e, &em);
                    tmcn_unlock();
                }
            }
            cJSON_Delete(arr);
        }
    }
    curl_easy_cleanup(curl);
    tmcn_lock(); e->thread_started = 0; tmcn_unlock();
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static void tmcn_start_reader(tmcn_entry_t *e) {
    if (!e) return;
    tmcn_lock();
    if (!e->thread_started) {
        e->thread_started = 1;
#ifdef _WIN32
        { uintptr_t h = _beginthreadex(NULL, 0, tmcn_ws_thread, e, 0, NULL); if (h) CloseHandle((HANDLE)h); }
#else
        { pthread_t t; if (pthread_create(&t, NULL, tmcn_ws_thread, e) == 0) pthread_detach(t); }
#endif
    }
    tmcn_unlock();
}
#endif

tm_email_info_t *tm_provider_tempmail_cn_generate(const char *domain) {
    char *host = tmcn_normalize_host(domain);
    tm_email_info_t *info = NULL;
    if (!host) return NULL;
#ifndef TM_TMCN_USE_CURL_WS
    free(host);
    return NULL;
#else
    {
        CURL *curl = tmcn_connect_socket(host);
        char packet[8192];
        if (!curl || !tmcn_send_event(curl, "request shortid", cJSON_CreateBool(1))) {
            if (curl) curl_easy_cleanup(curl);
            free(host);
            return NULL;
        }
        while (tmcn_ws_recv_text(curl, packet, sizeof(packet))) {
            if (strcmp(packet, "2") == 0) {
                if (!tmcn_ws_send_text(curl, "3")) break;
                continue;
            }
            if (strncmp(packet, "42", 2) == 0) {
                cJSON *arr = cJSON_Parse(packet + 2);
                cJSON *event = arr ? cJSON_GetArrayItem(arr, 0) : NULL;
                cJSON *payload = arr ? cJSON_GetArrayItem(arr, 1) : NULL;
                if (arr && cJSON_IsString(event) && strcmp(TM_JSON_STR(event, ""), "shortid") == 0 && cJSON_IsString(payload) && payload->valuestring && payload->valuestring[0]) {
                    size_t cap = strlen(payload->valuestring) + strlen(host) + 2;
                    char *email = (char*)malloc(cap);
                    if (email) {
                        snprintf(email, cap, "%s@%s", payload->valuestring, host);
                        info = tm_email_info_new();
                        if (info) {
                            info->channel = CHANNEL_TEMPMAIL_CN;
                            info->email = email;
                            email = NULL;
                        }
                        free(email);
                    }
                    cJSON_Delete(arr);
                    break;
                }
                cJSON_Delete(arr);
            }
        }
        curl_easy_cleanup(curl);
    }
    free(host);
    return info;
#endif
}

tm_email_t *tm_provider_tempmail_cn_get_emails(const char *email, int *count) {
    char *local = NULL;
    char *host = NULL;
    tmcn_entry_t *e;
    tm_email_t *dup;
    int n;
    *count = -1;
    if (!email || !email[0]) return NULL;
#ifndef TM_TMCN_USE_CURL_WS
    *count = 0;
    return NULL;
#else
    if (!tmcn_split_email(email, &local, &host)) {
        free(local); free(host);
        return NULL;
    }
    e = tmcn_find_or_create(email, local, host);
    free(local); free(host);
    if (!e) return NULL;
    tmcn_start_reader(e);
    tmcn_sleep_ms(80);
    tmcn_lock();
    n = e->count;
    dup = tmcn_dup_list(e->list, n);
    tmcn_unlock();
    *count = n;
    return dup;
#endif
}
