/**
 * HTTP 请求工具（基于 libcurl）
 */

#include "tempmail_internal.h"
#include <curl/curl.h>

#ifdef _WIN32
#define strncasecmp _strnicmp
#endif

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
        if (resp->cookies) free(resp->cookies);
        resp->cookies = (char*)malloc(len + 1);
        if (resp->cookies) {
            memcpy(resp->cookies, val, len);
            resp->cookies[len] = '\0';
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
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)(timeout_secs > 0 ? timeout_secs : 15));
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
#ifdef _WIN32
    /* Windows 下使用系统原生证书存储 */
    curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, (long)CURLSSLOPT_NATIVE_CA);
#endif

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

void tm_init(void) {
    curl_global_init(CURL_GLOBAL_ALL);
}

void tm_cleanup(void) {
    curl_global_cleanup();
}
