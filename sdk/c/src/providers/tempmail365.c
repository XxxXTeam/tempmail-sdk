/**
 * Tempmail365 — https://tempmail365.cn
 * 无需认证、无token、无cookie
 */
#include "tempmail_internal.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TM365_BASE "https://tempmail365.cn/tempemail.php"

/* 备用域名（API 不可用时使用） */
static const char *tm365_fallback_domains[] = {
    "fengyou.cc", "shop345.com", "nutemail.com", "qvrf.cn"
};
#define TM365_FALLBACK_DOMAIN_COUNT 4

static const char *tm365_headers[] = {
    "Accept: application/json, text/plain, */*",
    "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8",
    "Cache-Control: no-cache",
    "DNT: 1",
    "Pragma: no-cache",
    "Referer: https://tempmail365.cn/",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    NULL
};

/**
 * 生成8位随机字母数字用户名
 */
static void tm365_random_username(char *buf, size_t len) {
    static const char charset[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    size_t gen = (len - 1 < 8) ? len - 1 : 8;
    for (size_t i = 0; i < gen; i++) {
        buf[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    buf[gen] = '\0';
}

/**
 * URL 编码辅助
 */
static char *tm365_curl_escape(const char *s) {
    if (!s) return tm_strdup("");
    CURL *c = curl_easy_init();
    if (!c) return tm_strdup(s);
    char *e = curl_easy_escape(c, s, 0);
    curl_easy_cleanup(c);
    if (!e) return tm_strdup(s);
    char *d = tm_strdup(e);
    curl_free(e);
    return d;
}

/**
 * 从 HTML content 中提取发件人
 * 匹配 "发件人:" 或 "From:" 后的内容
 */
static char *tm365_extract_sender(const char *html) {
    if (!html) return tm_strdup("unknown");

    /* 尝试匹配 "发件人:" */
    const char *p = strstr(html, "\xe5\x8f\x91\xe4\xbb\xb6\xe4\xba\xba:");
    if (p) {
        p += strlen("\xe5\x8f\x91\xe4\xbb\xb6\xe4\xba\xba:");
        while (*p == ' ') p++;
        const char *end = p;
        while (*end && *end != '<' && *end != '\n' && *end != '\r') end++;
        if (end > p) {
            char *result = (char *)malloc((size_t)(end - p + 1));
            if (result) {
                memcpy(result, p, (size_t)(end - p));
                result[end - p] = '\0';
                return result;
            }
        }
    }

    /* 尝试匹配 "From:" */
    p = strstr(html, "From:");
    if (p) {
        p += 5;
        while (*p == ' ') p++;
        const char *end = p;
        while (*end && *end != '<' && *end != '\n' && *end != '\r') end++;
        if (end > p) {
            char *result = (char *)malloc((size_t)(end - p + 1));
            if (result) {
                memcpy(result, p, (size_t)(end - p));
                result[end - p] = '\0';
                return result;
            }
        }
    }

    return tm_strdup("unknown");
}

/**
 * 从 HTML content 中提取主题
 * 匹配 "主题:" 或 "Subject:" 后的内容
 */
static char *tm365_extract_subject(const char *html) {
    if (!html) return tm_strdup("");

    /* 尝试匹配 "主题:" */
    const char *p = strstr(html, "\xe4\xb8\xbb\xe9\xa2\x98:");
    if (p) {
        p += strlen("\xe4\xb8\xbb\xe9\xa2\x98:");
        while (*p == ' ') p++;
        const char *end = p;
        while (*end && *end != '<' && *end != '\n' && *end != '\r') end++;
        if (end > p) {
            char *result = (char *)malloc((size_t)(end - p + 1));
            if (result) {
                memcpy(result, p, (size_t)(end - p));
                result[end - p] = '\0';
                return result;
            }
        }
    }

    /* 尝试匹配 "Subject:" */
    p = strstr(html, "Subject:");
    if (p) {
        p += 8;
        while (*p == ' ') p++;
        const char *end = p;
        while (*end && *end != '<' && *end != '\n' && *end != '\r') end++;
        if (end > p) {
            char *result = (char *)malloc((size_t)(end - p + 1));
            if (result) {
                memcpy(result, p, (size_t)(end - p));
                result[end - p] = '\0';
                return result;
            }
        }
    }

    return tm_strdup("");
}

/**
 * 获取当前时间的 ISO 8601 字符串
 */
static char *tm365_now_iso(void) {
    time_t now = time(NULL);
    struct tm *t = gmtime(&now);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", t);
    return tm_strdup(buf);
}

tm_email_info_t* tm_tempmail365_generate(const char *domain) {
    srand((unsigned)time(NULL));

    /* 第一步：获取域名列表 */
    char domains[64][256];
    int nd = 0;

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, TM365_BASE "?action=get_config", tm365_headers, NULL, 15);
    if (resp && resp->status >= 200 && resp->status < 300 && resp->body) {
        cJSON *root = cJSON_Parse(resp->body);
        if (root) {
            cJSON *darr = cJSON_GetObjectItemCaseSensitive(root, "domains");
            if (cJSON_IsArray(darr)) {
                int n = cJSON_GetArraySize(darr);
                for (int i = 0; i < n && nd < 64; i++) {
                    cJSON *it = cJSON_GetArrayItem(darr, i);
                    const char *s = cJSON_GetStringValue(it);
                    if (s && s[0]) {
                        strncpy(domains[nd], s, sizeof(domains[0]) - 1);
                        domains[nd][sizeof(domains[0]) - 1] = '\0';
                        nd++;
                    }
                }
            }
            cJSON_Delete(root);
        }
    }
    tm_http_response_free(resp);

    /* API 返回域名为空时使用备用域名 */
    if (nd == 0) {
        for (int i = 0; i < TM365_FALLBACK_DOMAIN_COUNT && nd < 64; i++) {
            strncpy(domains[nd], tm365_fallback_domains[i], sizeof(domains[0]) - 1);
            domains[nd][sizeof(domains[0]) - 1] = '\0';
            nd++;
        }
    }
    if (nd == 0) return NULL;

    /* 选择域名：优先使用指定域名 */
    const char *pick = NULL;
    if (domain && domain[0]) {
        for (int i = 0; i < nd; i++) {
            if (strcmp(domains[i], domain) == 0) {
                pick = domains[i];
                break;
            }
        }
        if (!pick) return NULL;
    } else {
        pick = domains[rand() % nd];
    }

    /* 生成随机用户名 */
    char username[16];
    tm365_random_username(username, sizeof(username));

    /* 构造邮箱地址 */
    char email_addr[512];
    snprintf(email_addr, sizeof(email_addr), "%s@%s", username, pick);

    /* 第二步：创建邮箱 */
    char *enc_email = tm365_curl_escape(email_addr);
    char *enc_domain = tm365_curl_escape(pick);
    char url[1024];
    snprintf(url, sizeof(url), "%s?action=create_email&email=%s&domain=%s", TM365_BASE, enc_email, enc_domain);
    free(enc_email);
    free(enc_domain);

    tm_http_response_t *r2 = tm_http_request(TM_HTTP_GET, url, tm365_headers, NULL, 15);
    if (!r2 || r2->status < 200 || r2->status >= 300) {
        tm_http_response_free(r2);
        return NULL;
    }

    cJSON *cj = cJSON_Parse(r2->body);
    tm_http_response_free(r2);
    if (!cj) return NULL;

    const cJSON *ok = cJSON_GetObjectItemCaseSensitive(cj, "success");
    if (!cJSON_IsBool(ok) || !cJSON_IsTrue(ok)) {
        cJSON_Delete(cj);
        return NULL;
    }
    cJSON_Delete(cj);

    tm_email_info_t *info = tm_email_info_new();
    info->channel = CHANNEL_TEMPMAIL365;
    info->email = tm_strdup(email_addr);
    info->token = NULL;
    return info;
}

tm_email_t* tm_tempmail365_get_emails(const char *email, int *count) {
    *count = -1;
    if (!email || !email[0]) return NULL;

    char *enc_email = tm365_curl_escape(email);
    char url[1024];
    snprintf(url, sizeof(url), "%s?action=fetch_mail&email=%s", TM365_BASE, enc_email);
    free(enc_email);

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, tm365_headers, NULL, 15);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }

    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!root) return NULL;

    const char *content = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(root, "content"));
    if (!content || !content[0]) {
        *count = 0;
        cJSON_Delete(root);
        return NULL;
    }

    /* "无邮件" 表示没有邮件 */
    if (strcmp(content, "\xe6\x97\xa0\xe9\x82\xae\xe4\xbb\xb6") == 0) {
        *count = 0;
        cJSON_Delete(root);
        return NULL;
    }

    /* 从 HTML 中提取发件人和主题 */
    char *sender = tm365_extract_sender(content);
    char *subject = tm365_extract_subject(content);
    char *date_str = tm365_now_iso();

    /* 构造 cJSON 对象用于 normalize */
    cJSON *mail_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(mail_obj, "from", sender);
    cJSON_AddStringToObject(mail_obj, "subject", subject);
    cJSON_AddStringToObject(mail_obj, "body", content);
    cJSON_AddStringToObject(mail_obj, "html_body", content);
    cJSON_AddStringToObject(mail_obj, "date", date_str);

    free(sender);
    free(subject);
    free(date_str);

    *count = 1;
    tm_email_t *emails = tm_emails_new(1);
    if (!emails) {
        cJSON_Delete(mail_obj);
        cJSON_Delete(root);
        *count = -1;
        return NULL;
    }

    emails[0] = tm_normalize_email(mail_obj, email);

    cJSON_Delete(mail_obj);
    cJSON_Delete(root);
    return emails;
}
