/**
 * HTTP 请求工具（基于 libcurl）
 */

#include "tempmail_internal.h"
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define strncasecmp _strnicmp
#else
#include <strings.h>
#endif

/* ========== 全局配置 ========== */

static tm_config_t g_config = { .telemetry_enabled = true };
static char *g_proxy_copy = NULL; /* 内部复制的代理 URL */
static char *g_telemetry_url_copy = NULL;

tm_config_t tm_default_config(void) {
    tm_config_t c;
    memset(&c, 0, sizeof(c));
    c.telemetry_enabled = true;
    return c;
}

void tm_set_config(const tm_config_t *config) {
    if (g_proxy_copy) { free(g_proxy_copy); g_proxy_copy = NULL; }
    if (g_telemetry_url_copy) { free(g_telemetry_url_copy); g_telemetry_url_copy = NULL; }
    if (!config) {
        memset(&g_config, 0, sizeof(g_config));
        g_config.telemetry_enabled = true;
        return;
    }
    if (config->proxy) {
        g_proxy_copy = strdup(config->proxy);
        g_config.proxy = g_proxy_copy;
    } else {
        g_config.proxy = NULL;
    }
    g_config.timeout_secs = config->timeout_secs;
    g_config.insecure = config->insecure;
    g_config.telemetry_enabled = config->telemetry_enabled;
    if (config->telemetry_url && config->telemetry_url[0]) {
        g_telemetry_url_copy = strdup(config->telemetry_url);
        g_config.telemetry_url = g_telemetry_url_copy;
    } else {
        g_config.telemetry_url = NULL;
    }
    TM_LOG_INF("SDK 配置已更新: proxy=%s timeout=%d insecure=%d telemetry_enabled=%d",
        g_config.proxy ? g_config.proxy : "(none)",
        g_config.timeout_secs, g_config.insecure, (int)g_config.telemetry_enabled);
}

const tm_config_t* tm_get_config(void) {
    return &g_config;
}

/* libcurl 写回调 */
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t total = size * nmemb;
    tm_http_response_t *resp = (tm_http_response_t*)userp;
    char *ptr = (char*)realloc(resp->body, resp->size + total + 1);
    if (!ptr) return 0;
    resp->body = ptr;
    memcpy(resp->body + resp->size, contents, total);
    resp->size += total;
    resp->body[resp->size] = '\0';
    return total;
}

/* libcurl header 回调（提取 Set-Cookie） */
static size_t header_callback(char *buffer, size_t size, size_t nitems, void *userp) {
    size_t total = size * nitems;
    tm_http_response_t *resp = (tm_http_response_t*)userp;

    if (strncasecmp(buffer, "set-cookie:", 11) == 0) {
        char *val = buffer + 11;
        while (*val == ' ') val++;
        size_t len = total - (val - buffer);
        /* 去掉末尾换行 */
        while (len > 0 && (val[len-1] == '\r' || val[len-1] == '\n')) len--;

        /* 优先提取 gm_sid */
        char *gm = strstr(val, "gm_sid=");
        if (gm) {
            char *end = strchr(gm, ';');
            size_t gm_len = end ? (size_t)(end - gm) : strlen(gm);
            char *gm_cookie = (char*)malloc(gm_len + 1);
            if (gm_cookie) {
                memcpy(gm_cookie, gm, gm_len);
                gm_cookie[gm_len] = '\0';
                if (resp->cookies) free(resp->cookies);
                resp->cookies = gm_cookie;
            }
            return total;
        }

        /* 已有 gm_sid 时不追加其他 Cookie（与 guerrillamail 行为一致） */
        if (resp->cookies && strstr(resp->cookies, "gm_sid=") != NULL) {
            return total;
        }

        /* 仅取 name=value（第一个分号前），多条 Set-Cookie 用 "; " 拼接 */
        char *semi = strchr(val, ';');
        size_t nvlen = semi ? (size_t)(semi - val) : len;
        while (nvlen > 0 && (val[nvlen - 1] == ' ' || val[nvlen - 1] == '\t')) nvlen--;

        char *nv = (char*)malloc(nvlen + 1);
        if (!nv) return total;
        memcpy(nv, val, nvlen);
        nv[nvlen] = '\0';

        if (resp->cookies && resp->cookies[0] != '\0') {
            size_t oldlen = strlen(resp->cookies);
            size_t newlen = oldlen + 2 + nvlen;
            char *merged = (char*)malloc(newlen + 1);
            if (merged) {
                memcpy(merged, resp->cookies, oldlen);
                merged[oldlen] = ';';
                merged[oldlen + 1] = ' ';
                memcpy(merged + oldlen + 2, nv, nvlen + 1);
                free(resp->cookies);
                resp->cookies = merged;
            }
            free(nv);
        } else {
            if (resp->cookies) free(resp->cookies);
            resp->cookies = nv;
        }
    }
    return total;
}

tm_http_response_t* tm_http_request(
    tm_http_method_t method,
    const char *url,
    const char **headers,
    const char *body,
    int timeout_secs
) {
    tm_http_response_t *resp = (tm_http_response_t*)calloc(1, sizeof(tm_http_response_t));
    if (!resp) return NULL;

    CURL *curl = curl_easy_init();
    if (!curl) {
        free(resp);
        return NULL;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, resp);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, resp);
    /* 超时: 参数 > 全局配置 > 默认 15s */
    int effective_timeout = timeout_secs > 0 ? timeout_secs
        : (g_config.timeout_secs > 0 ? g_config.timeout_secs : 15);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)effective_timeout);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    /* SSL 验证 */
    if (g_config.insecure) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    } else {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
#ifdef _WIN32
        /* Windows 下使用系统原生证书存储 */
        curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, (long)CURLSSLOPT_NATIVE_CA);
#endif
    }

    /* 代理 */
    if (g_config.proxy) {
        curl_easy_setopt(curl, CURLOPT_PROXY, g_config.proxy);
    }

    /* 设置请求头 */
    struct curl_slist *header_list = NULL;
    if (headers) {
        for (int i = 0; headers[i] != NULL; i++) {
            header_list = curl_slist_append(header_list, headers[i]);
        }
    }
    if (header_list) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
    }

    /* 设置方法和 body */
    if (method == TM_HTTP_POST) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        if (body) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
        } else {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0L);
        }
    }

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        TM_LOG_ERR("HTTP request failed: %s", curl_easy_strerror(res));
        curl_slist_free_all(header_list);
        curl_easy_cleanup(curl);
        free(resp->body);
        free(resp->cookies);
        free(resp);
        return NULL;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp->status);

    curl_slist_free_all(header_list);
    curl_easy_cleanup(curl);
    return resp;
}

void tm_http_response_free(tm_http_response_t *resp) {
    if (!resp) return;
    free(resp->body);
    free(resp->cookies);
    free(resp);
}

/*
 * 从环境变量读取默认配置
 * TEMPMAIL_PROXY / TEMPMAIL_TIMEOUT / TEMPMAIL_INSECURE
 */
static void load_env_config(void) {
    const char *proxy = getenv("TEMPMAIL_PROXY");
    if (proxy && proxy[0]) {
        if (g_proxy_copy) free(g_proxy_copy);
        g_proxy_copy = strdup(proxy);
        g_config.proxy = g_proxy_copy;
    }
    const char *timeout = getenv("TEMPMAIL_TIMEOUT");
    if (timeout && timeout[0]) {
        int t = atoi(timeout);
        if (t > 0) g_config.timeout_secs = t;
    }
    const char *insecure = getenv("TEMPMAIL_INSECURE");
    if (insecure && (insecure[0] == '1' || strncasecmp(insecure, "true", 4) == 0)) {
        g_config.insecure = true;
    }
    const char *te = getenv("TEMPMAIL_TELEMETRY_ENABLED");
    if (te && te[0]) {
        if (strncasecmp(te, "false", 5) == 0 || strcmp(te, "0") == 0 || strncasecmp(te, "no", 3) == 0) {
            g_config.telemetry_enabled = false;
        } else if (strncasecmp(te, "true", 4) == 0 || strcmp(te, "1") == 0 || strncasecmp(te, "yes", 3) == 0) {
            g_config.telemetry_enabled = true;
        }
    }
    const char *tu = getenv("TEMPMAIL_TELEMETRY_URL");
    if (tu && tu[0]) {
        if (g_telemetry_url_copy) { free(g_telemetry_url_copy); g_telemetry_url_copy = NULL; }
        g_telemetry_url_copy = strdup(tu);
        if (g_telemetry_url_copy) g_config.telemetry_url = g_telemetry_url_copy;
    }
}

void tm_init(void) {
    curl_global_init(CURL_GLOBAL_ALL);
    load_env_config();
    tm_vip215_module_init();
    tm_tempmail_cn_module_init();
}

void tm_cleanup(void) {
    tm_telemetry_flush_batch();
    tm_vip215_module_cleanup();
    tm_tempmail_cn_module_cleanup();
    curl_global_cleanup();
}
