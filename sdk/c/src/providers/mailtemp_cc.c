/**
 * mailtemp.cc 渠道
 *
 * PHP 后端，使用 form-urlencoded POST 请求
 * 域名: @neocea.com
 *
 * 创建邮箱流程:
 *   POST https://mailtemp.cc/api.php
 *   Content-Type: application/x-www-form-urlencoded
 *   body: action=inbox
 *   返回: JSON 字符串（带引号），如 "vindictiverate"（只是用户名部分）
 *   完整邮箱: {username}@neocea.com
 *
 * 获取邮件流程:
 *   POST https://mailtemp.cc/api.php
 *   Content-Type: application/x-www-form-urlencoded
 *   body: action=fetch&inbox={username}&last_id=0
 *   返回: JSON 数组 [{id, subject, sender, sender_email, received_at, ...}]
 *
 * 查看邮件详情:
 *   POST https://mailtemp.cc/api.php
 *   Content-Type: application/x-www-form-urlencoded
 *   body: action=view&id={id}&inbox={username}
 *   返回: JSON {id, subject, sender, sender_email, received_at, body_html, advertisement}
 *
 * token 存储策略:
 *   存储 username（@前面的部分）
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MTC_API  "https://mailtemp.cc/api.php"
#define MTC_DOMAIN "neocea.com"
#define MTC_MAX_MAILS 128

/* 公共请求头 */
static const char *mtc_headers[] = {
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
    "Content-Type: application/x-www-form-urlencoded",
    "Accept: */*",
    "Referer: https://mailtemp.cc/",
    "Origin: https://mailtemp.cc",
    NULL
};

/**
 * 去除字符串首尾的双引号
 * 返回新分配的字符串，调用者需 free
 */
static char *strip_json_quotes(const char *s) {
    if (!s || !*s) return tm_strdup("");
    size_t len = strlen(s);
    const char *start = s;
    const char *end = s + len;
    /* 跳过首尾空白 */
    while (start < end && (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r'))
        start++;
    while (end > start && (*(end - 1) == ' ' || *(end - 1) == '\t' || *(end - 1) == '\n' || *(end - 1) == '\r'))
        end--;
    /* 去除引号 */
    if (end - start >= 2 && *start == '"' && *(end - 1) == '"') {
        start++;
        end--;
    }
    size_t out_len = (size_t)(end - start);
    char *out = (char *)malloc(out_len + 1);
    if (!out) return NULL;
    memcpy(out, start, out_len);
    out[out_len] = '\0';
    return out;
}

/* ========== 创建邮箱 ========== */

/**
 * 创建 mailtemp.cc 临时邮箱
 *
 * 流程:
 *   1. POST api.php，body: action=inbox
 *   2. 响应为 JSON 字符串（带引号），解析出用户名
 *   3. 拼接完整邮箱: {username}@neocea.com
 *   4. token 存储用户名部分
 */
tm_email_info_t *tm_provider_mailtemp_cc_generate(void) {
    int timeout = tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

    tm_http_response_t *resp = tm_http_request(
        TM_HTTP_POST,
        MTC_API,
        mtc_headers,
        "action=inbox",
        timeout
    );

    if (!resp || resp->status != 200 || !resp->body) {
        TM_LOG_ERR("[mailtemp-cc] 创建收件箱请求失败, status=%ld", resp ? resp->status : 0);
        tm_http_response_free(resp);
        return NULL;
    }

    /* 响应是 JSON 字符串格式（带引号），如 "vindictiverate"，需要去掉引号 */
    char *username = strip_json_quotes(resp->body);
    tm_http_response_free(resp);

    if (!username || !*username) {
        TM_LOG_ERR("[mailtemp-cc] 返回的用户名为空");
        free(username);
        return NULL;
    }

    /* 验证用户名不包含非法字符 */
    for (const char *p = username; *p; p++) {
        if (*p == '@' || *p == ' ' || *p == '\n' || *p == '\r') {
            TM_LOG_ERR("[mailtemp-cc] 返回的用户名格式无效: %s", username);
            free(username);
            return NULL;
        }
    }

    /* 构建邮箱地址: {username}@neocea.com */
    size_t email_len = strlen(username) + 1 + strlen(MTC_DOMAIN) + 1;
    char *email_addr = (char *)malloc(email_len);
    if (!email_addr) {
        free(username);
        return NULL;
    }
    snprintf(email_addr, email_len, "%s@%s", username, MTC_DOMAIN);

    /* 构建结果 */
    tm_email_info_t *info = tm_email_info_new();
    if (!info) {
        free(username);
        free(email_addr);
        return NULL;
    }

    info->channel = CHANNEL_MAILTEMP_CC;
    info->email = email_addr;
    info->token = username;    /* token 存储用户名部分 */
    info->expires_at = 0;
    info->created_at = tm_strdup("");

    TM_LOG_DBG("[mailtemp-cc] 创建邮箱成功: %s", info->email);
    return info;
}

/* ========== 获取邮件 ========== */

/**
 * 获取 mailtemp.cc 邮件列表
 *
 * 流程:
 *   1. POST api.php, body: action=fetch&inbox={username}&last_id=0 获取邮件列表
 *   2. 对每封邮件，POST api.php, body: action=view&id={id}&inbox={username} 获取详情
 *   3. 将详情标准化后返回
 *
 * @param token  用户名（@ 前面的部分）
 * @param email  完整邮箱地址
 * @param count  输出邮件数量，-1 表示请求失败
 */
tm_email_t *tm_provider_mailtemp_cc_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    if (!token || !email) return NULL;

    int timeout = tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

    /* 构造 fetch 请求 body: action=fetch&inbox={username}&last_id=0 */
    size_t fetch_body_len = strlen("action=fetch&inbox=") + strlen(token) + strlen("&last_id=0") + 1;
    char *fetch_body = (char *)malloc(fetch_body_len);
    if (!fetch_body) return NULL;
    snprintf(fetch_body, fetch_body_len, "action=fetch&inbox=%s&last_id=0", token);

    tm_http_response_t *resp = tm_http_request(
        TM_HTTP_POST,
        MTC_API,
        mtc_headers,
        fetch_body,
        timeout
    );
    free(fetch_body);

    if (!resp || resp->status != 200 || !resp->body) {
        TM_LOG_ERR("[mailtemp-cc] 获取邮件列表请求失败, status=%ld", resp ? resp->status : 0);
        tm_http_response_free(resp);
        return NULL;
    }

    /* 解析邮件列表 JSON 数组 */
    cJSON *list = cJSON_Parse(resp->body);
    tm_http_response_free(resp);

    if (!list) {
        TM_LOG_ERR("[mailtemp-cc] 解析邮件列表 JSON 失败");
        return NULL;
    }

    if (!cJSON_IsArray(list)) {
        /* 可能返回空对象或错误，视为无邮件 */
        cJSON_Delete(list);
        *count = 0;
        return NULL;
    }

    int list_size = cJSON_GetArraySize(list);
    if (list_size <= 0) {
        cJSON_Delete(list);
        *count = 0;
        return NULL;
    }
    if (list_size > MTC_MAX_MAILS) list_size = MTC_MAX_MAILS;

    tm_email_t *emails = tm_emails_new(list_size);
    if (!emails) {
        cJSON_Delete(list);
        return NULL;
    }

    int valid = 0;
    for (int i = 0; i < list_size; i++) {
        const cJSON *item = cJSON_GetArrayItem(list, i);
        if (!cJSON_IsObject(item)) continue;

        /* 提取邮件 ID，用于获取详情 */
        const cJSON *j_id = cJSON_GetObjectItemCaseSensitive(item, "id");
        if (!j_id) continue;

        /* 获取 id 的字符串表示 */
        char id_str[64];
        if (cJSON_IsString(j_id) && j_id->valuestring) {
            snprintf(id_str, sizeof(id_str), "%s", j_id->valuestring);
        } else if (cJSON_IsNumber(j_id)) {
            snprintf(id_str, sizeof(id_str), "%d", (int)j_id->valuedouble);
        } else {
            continue;
        }

        /* 获取邮件详情: action=view&id={id}&inbox={username} */
        size_t view_body_len = strlen("action=view&id=") + strlen(id_str) + strlen("&inbox=") + strlen(token) + 1;
        char *view_body = (char *)malloc(view_body_len);
        if (!view_body) continue;
        snprintf(view_body, view_body_len, "action=view&id=%s&inbox=%s", id_str, token);

        tm_http_response_t *detail_resp = tm_http_request(
            TM_HTTP_POST,
            MTC_API,
            mtc_headers,
            view_body,
            timeout
        );
        free(view_body);

        if (!detail_resp || detail_resp->status != 200 || !detail_resp->body) {
            tm_http_response_free(detail_resp);
            /* 详情获取失败时使用列表中的基本信息 */
            cJSON *raw = cJSON_Duplicate(item, 1);
            if (!raw) continue;
            /* 映射字段以匹配 normalize 候选策略 */
            const cJSON *j_sender_email = cJSON_GetObjectItemCaseSensitive(raw, "sender_email");
            if (cJSON_IsString(j_sender_email) && j_sender_email->valuestring) {
                if (!cJSON_GetObjectItemCaseSensitive(raw, "from")) {
                    cJSON_AddStringToObject(raw, "from", j_sender_email->valuestring);
                }
            }
            if (!cJSON_GetObjectItemCaseSensitive(raw, "to")) {
                cJSON_AddStringToObject(raw, "to", email);
            }
            /* 映射 received_at 到 date */
            const cJSON *j_received = cJSON_GetObjectItemCaseSensitive(raw, "received_at");
            if (cJSON_IsString(j_received) && j_received->valuestring) {
                if (!cJSON_GetObjectItemCaseSensitive(raw, "date")) {
                    cJSON_AddStringToObject(raw, "date", j_received->valuestring);
                }
            }
            emails[valid] = tm_normalize_email(raw, email);
            cJSON_Delete(raw);
            valid++;
            continue;
        }

        /* 解析详情 JSON */
        cJSON *detail = cJSON_Parse(detail_resp->body);
        tm_http_response_free(detail_resp);

        if (!detail || !cJSON_IsObject(detail)) {
            cJSON_Delete(detail);
            /* 回退到列表数据 */
            cJSON *raw = cJSON_Duplicate(item, 1);
            if (!raw) continue;
            if (!cJSON_GetObjectItemCaseSensitive(raw, "to")) {
                cJSON_AddStringToObject(raw, "to", email);
            }
            emails[valid] = tm_normalize_email(raw, email);
            cJSON_Delete(raw);
            valid++;
            continue;
        }

        /* 映射 sender_email -> from */
        const cJSON *j_sender_email = cJSON_GetObjectItemCaseSensitive(detail, "sender_email");
        if (cJSON_IsString(j_sender_email) && j_sender_email->valuestring) {
            if (!cJSON_GetObjectItemCaseSensitive(detail, "from")) {
                cJSON_AddStringToObject(detail, "from", j_sender_email->valuestring);
            }
        }

        /* 补充 to 字段 */
        if (!cJSON_GetObjectItemCaseSensitive(detail, "to")) {
            cJSON_AddStringToObject(detail, "to", email);
        }

        /* 映射 body_html -> html */
        const cJSON *j_body_html = cJSON_GetObjectItemCaseSensitive(detail, "body_html");
        if (cJSON_IsString(j_body_html) && j_body_html->valuestring) {
            if (!cJSON_GetObjectItemCaseSensitive(detail, "html")) {
                cJSON_AddStringToObject(detail, "html", j_body_html->valuestring);
            }
        }

        /* 映射 received_at -> date */
        const cJSON *j_received = cJSON_GetObjectItemCaseSensitive(detail, "received_at");
        if (cJSON_IsString(j_received) && j_received->valuestring) {
            if (!cJSON_GetObjectItemCaseSensitive(detail, "date")) {
                cJSON_AddStringToObject(detail, "date", j_received->valuestring);
            }
        }

        emails[valid] = tm_normalize_email(detail, email);
        cJSON_Delete(detail);
        valid++;
    }

    cJSON_Delete(list);

    if (valid == 0) {
        free(emails);
        *count = 0;
        return NULL;
    }

    *count = valid;
    return emails;
}
