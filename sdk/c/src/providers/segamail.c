/**
 * Segamail -- https://segamail.com
 *
 * Generate: POST https://segamail.com/en/getEmailAddress
 *   -> {"id":"591815","address":"xxx@segamail.com","creation_time":"...","recover_key":"LSJFEYQ591815"}
 *
 * Inbox: POST https://segamail.com/en/getInbox
 *   -> [{"from":"...","date":"...","subject":"...","body":"..."}]
 *
 * token = recover_key
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SEGA_GENERATE_URL "https://segamail.com/en/getEmailAddress"
#define SEGA_INBOX_URL    "https://segamail.com/en/getInbox"

static const char *sega_headers[] = {
    "Accept: application/json, text/plain, */*",
    "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8",
    "Content-Type: application/x-www-form-urlencoded; charset=UTF-8",
    "Origin: https://segamail.com",
    "Referer: https://segamail.com/en/",
    "X-Requested-With: XMLHttpRequest",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
    NULL
};

/**
 * tm_provider_segamail_generate - 创建 Segamail 临时邮箱
 *
 * POST getEmailAddress，从响应中提取 address 和 recover_key
 * recover_key 作为 token 保存
 *
 * @return 成功返回 tm_email_info_t 指针，失败返回 NULL
 */
tm_email_info_t *tm_provider_segamail_generate(void) {
    tm_http_response_t *resp = tm_http_request(
        TM_HTTP_POST, SEGA_GENERATE_URL, sega_headers, "", 15);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        TM_LOG_ERR("segamail: 创建邮箱请求失败, status=%ld",
                   resp ? resp->status : 0);
        tm_http_response_free(resp);
        return NULL;
    }

    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!root) {
        TM_LOG_ERR("segamail: 解析创建响应 JSON 失败");
        return NULL;
    }

    /* 提取 address */
    cJSON *addr_item = cJSON_GetObjectItemCaseSensitive(root, "address");
    if (!cJSON_IsString(addr_item) || !addr_item->valuestring || !addr_item->valuestring[0]) {
        TM_LOG_ERR("segamail: 响应中缺少 address 字段");
        cJSON_Delete(root);
        return NULL;
    }

    /* 提取 recover_key 作为 token */
    cJSON *rk_item = cJSON_GetObjectItemCaseSensitive(root, "recover_key");
    const char *recover_key = (cJSON_IsString(rk_item) && rk_item->valuestring)
                              ? rk_item->valuestring : "";

    tm_email_info_t *info = tm_email_info_new();
    if (!info) {
        cJSON_Delete(root);
        return NULL;
    }

    info->channel = CHANNEL_SEGAMAIL;
    info->email   = tm_strdup(addr_item->valuestring);
    info->token   = tm_strdup(recover_key);

    /* 如果响应中包含 creation_time，存入 created_at */
    cJSON *ct_item = cJSON_GetObjectItemCaseSensitive(root, "creation_time");
    if (cJSON_IsString(ct_item) && ct_item->valuestring) {
        info->created_at = tm_strdup(ct_item->valuestring);
    }

    cJSON_Delete(root);
    return info;
}

/**
 * tm_provider_segamail_get_emails - 获取 Segamail 收件箱邮件
 *
 * POST getInbox，响应为 JSON 数组
 * 由于 C SDK 的 HTTP 函数无 cookie jar，此处直接 POST 请求
 * 如果服务端不强制要求 session cookie，即可正常工作
 *
 * @param token  recover_key（当前实现中未使用，保留备用）
 * @param email  邮箱地址
 * @param count  输出邮件数量，-1 表示失败
 * @return 邮件数组，失败返回 NULL
 */
tm_email_t *tm_provider_segamail_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    (void)token; /* recover_key 暂未用于 inbox 请求 */

    tm_http_response_t *resp = tm_http_request(
        TM_HTTP_POST, SEGA_INBOX_URL, sega_headers, "", 15);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        TM_LOG_ERR("segamail: 获取收件箱请求失败, status=%ld",
                   resp ? resp->status : 0);
        tm_http_response_free(resp);
        return NULL;
    }

    cJSON *root = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!root) {
        TM_LOG_ERR("segamail: 解析收件箱 JSON 失败");
        return NULL;
    }

    /* 响应是顶层数组 */
    if (!cJSON_IsArray(root)) {
        TM_LOG_ERR("segamail: 收件箱响应不是数组");
        cJSON_Delete(root);
        return NULL;
    }

    int n = cJSON_GetArraySize(root);
    *count = n;
    if (n == 0) {
        cJSON_Delete(root);
        return NULL;
    }

    tm_email_t *emails = tm_emails_new(n);
    if (!emails) {
        cJSON_Delete(root);
        *count = -1;
        return NULL;
    }

    for (int i = 0; i < n; i++) {
        cJSON *raw = cJSON_GetArrayItem(root, i);
        emails[i] = tm_normalize_email(raw, email);
    }

    cJSON_Delete(root);
    return emails;
}
