/**
 * disposablemail.app 渠道
 *
 * 纯 JSON REST API，无需认证
 * 域名: @disposablemail.dev, @mailmehere.cc
 *
 * 创建邮箱流程:
 *   POST https://disposablemail.app/api/inbox
 *   Content-Type: application/json
 *   body: {}
 *   返回: {"id":"...","address":"...@disposablemail.dev","token":"...","domain":"...","expiresAt":"...","createdAt":"..."}
 *
 * 获取邮件流程:
 *   GET https://disposablemail.app/api/inbox/emails?token={token}
 *   返回: {"emails":[...],"total":0,"inbox":{"address":"...","expiresAt":"..."}}
 *
 * token 存储策略:
 *   直接存储 API 返回的 token 字符串
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DMA_BASE "https://disposablemail.app"
#define DMA_MAX_MAILS 128

/* 公共 UA */
static const char *dma_ua =
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36";

/* ========== 创建邮箱 ========== */

/**
 * 创建 disposablemail.app 临时邮箱
 *
 * 流程:
 *   1. POST /api/inbox，body 为空 JSON 对象
 *   2. 解析响应提取 address、token
 *   3. token 直接存储 API 返回的 token 字符串
 */
tm_email_info_t *tm_provider_disposablemail_app_generate(void) {
    int timeout = tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

    /* 请求头 */
    const char *headers[] = {
        dma_ua,
        "Content-Type: application/json",
        "Accept: application/json, text/plain, */*",
        "Referer: https://disposablemail.app/",
        "Origin: https://disposablemail.app",
        NULL
    };

    /* 发送请求，body 为空 JSON 对象 */
    tm_http_response_t *resp = tm_http_request(
        TM_HTTP_POST,
        DMA_BASE "/api/inbox",
        headers,
        "{}",
        timeout
    );

    if (!resp || resp->status != 200 || !resp->body) {
        TM_LOG_ERR("[disposablemail-app] 创建收件箱请求失败, status=%ld", resp ? resp->status : 0);
        tm_http_response_free(resp);
        return NULL;
    }

    /* 解析响应 JSON */
    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);

    if (!json) {
        TM_LOG_ERR("[disposablemail-app] 解析创建收件箱响应 JSON 失败");
        return NULL;
    }

    /* 提取 address 和 token */
    const cJSON *j_address = cJSON_GetObjectItemCaseSensitive(json, "address");
    const cJSON *j_token = cJSON_GetObjectItemCaseSensitive(json, "token");

    if (!cJSON_IsString(j_address) || !j_address->valuestring || !j_address->valuestring[0]) {
        TM_LOG_ERR("[disposablemail-app] 响应中缺少 address 字段");
        cJSON_Delete(json);
        return NULL;
    }

    if (!cJSON_IsString(j_token) || !j_token->valuestring || !j_token->valuestring[0]) {
        TM_LOG_ERR("[disposablemail-app] 响应中缺少 token 字段");
        cJSON_Delete(json);
        return NULL;
    }

    /* 验证邮箱格式 */
    if (!strchr(j_address->valuestring, '@')) {
        TM_LOG_ERR("[disposablemail-app] 返回的邮箱格式无效: %s", j_address->valuestring);
        cJSON_Delete(json);
        return NULL;
    }

    /* 提取可选字段 */
    const cJSON *j_expires_at = cJSON_GetObjectItemCaseSensitive(json, "expiresAt");
    const cJSON *j_created_at = cJSON_GetObjectItemCaseSensitive(json, "createdAt");

    /* 构建结果 */
    tm_email_info_t *info = tm_email_info_new();
    if (!info) {
        cJSON_Delete(json);
        return NULL;
    }

    info->channel = CHANNEL_DISPOSABLEMAIL_APP;
    info->email = tm_strdup(j_address->valuestring);
    info->token = tm_strdup(j_token->valuestring);
    info->expires_at = 0;
    info->created_at = tm_strdup(
        (cJSON_IsString(j_created_at) && j_created_at->valuestring) ? j_created_at->valuestring : ""
    );

    cJSON_Delete(json);

    TM_LOG_DBG("[disposablemail-app] 创建邮箱成功: %s", info->email);
    return info;
}

/* ========== 获取邮件 ========== */

/**
 * 获取 disposablemail.app 邮件列表
 *
 * 流程:
 *   1. GET /api/inbox/emails?token={token}
 *   2. 解析响应中的 emails 数组并标准化
 */
tm_email_t *tm_provider_disposablemail_app_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    if (!token || !email) return NULL;

    int timeout = tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

    /* 构造 URL: /api/inbox/emails?token={token} */
    size_t url_len = strlen(DMA_BASE "/api/inbox/emails?token=") + strlen(token) + 1;
    char *url = (char *)malloc(url_len);
    if (!url) return NULL;
    snprintf(url, url_len, DMA_BASE "/api/inbox/emails?token=%s", token);

    /* 请求头 */
    const char *headers[] = {
        dma_ua,
        "Accept: application/json, text/plain, */*",
        "Referer: https://disposablemail.app/",
        NULL
    };

    /* 发送请求 */
    tm_http_response_t *resp = tm_http_request(
        TM_HTTP_GET,
        url,
        headers,
        NULL,
        timeout
    );
    free(url);

    if (!resp || resp->status != 200 || !resp->body) {
        TM_LOG_ERR("[disposablemail-app] 获取邮件请求失败, status=%ld", resp ? resp->status : 0);
        tm_http_response_free(resp);
        return NULL;
    }

    /* 解析响应 JSON */
    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);

    if (!json) {
        TM_LOG_ERR("[disposablemail-app] 解析获取邮件响应 JSON 失败");
        return NULL;
    }

    /* 提取 emails 数组 */
    const cJSON *j_emails = cJSON_GetObjectItemCaseSensitive(json, "emails");
    if (!cJSON_IsArray(j_emails)) {
        /* 没有 emails 字段，视为无邮件 */
        cJSON_Delete(json);
        *count = 0;
        return NULL;
    }

    int mail_count = cJSON_GetArraySize(j_emails);
    if (mail_count <= 0) {
        cJSON_Delete(json);
        *count = 0;
        return NULL;
    }
    if (mail_count > DMA_MAX_MAILS) mail_count = DMA_MAX_MAILS;

    tm_email_t *emails = tm_emails_new(mail_count);
    if (!emails) {
        cJSON_Delete(json);
        return NULL;
    }

    int valid_count = 0;
    for (int i = 0; i < mail_count; i++) {
        const cJSON *item = cJSON_GetArrayItem(j_emails, i);
        if (!cJSON_IsObject(item)) continue;

        /* 复制邮件对象用于标准化 */
        cJSON *raw = cJSON_Duplicate(item, 1);
        if (!raw) continue;

        /* 确保 to 字段存在 */
        if (!cJSON_GetObjectItemCaseSensitive(raw, "to")) {
            cJSON_AddStringToObject(raw, "to", email);
        }

        emails[valid_count] = tm_normalize_email(raw, email);
        cJSON_Delete(raw);
        valid_count++;
    }

    cJSON_Delete(json);

    if (valid_count == 0) {
        free(emails);
        *count = 0;
        return NULL;
    }

    *count = valid_count;
    return emails;
}
