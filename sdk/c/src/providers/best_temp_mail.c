/**
 * best-temp-mail.com 渠道
 *
 * 纯 JSON REST API，无需认证，无需 Session
 *
 * 创建邮箱流程:
 *   1. 客户端生成 UUID v4 作为 intToken
 *   2. POST https://best-temp-mail.com/api/v3/createEmail
 *      body: {"intToken":"<uuid>"}
 *   3. 解析响应提取 address, id, update_tag
 *
 * 获取邮件流程:
 *   1. POST https://best-temp-mail.com/api/v3/getEmailList
 *      body: {"address":"...","id":"...","intToken":"...","update_tag":"..."}
 *   2. 解析响应中的 emails 数组
 *
 * token 存储策略:
 *   将 intToken、id、update_tag 序列化为 JSON 字符串存入 token 字段
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BTM_BASE "https://best-temp-mail.com"
#define BTM_MAX_MAILS 128

/* 公共 UA */
static const char *btm_ua =
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36";

/**
 * 生成 UUID v4 字符串
 * 格式: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
 * y 的高两位为 10（即 8/9/a/b）
 */
static void btm_generate_uuid(char *out, size_t cap) {
    static const char hex[] = "0123456789abcdef";
    unsigned char bytes[16];

    /* 使用 rand() 生成 16 字节随机数 */
    for (int i = 0; i < 16; i++) {
        bytes[i] = (unsigned char)(rand() & 0xFF);
    }

    /* 设置版本号 4 */
    bytes[6] = (bytes[6] & 0x0F) | 0x40;
    /* 设置变体为 RFC 4122 (高两位为 10) */
    bytes[8] = (bytes[8] & 0x3F) | 0x80;

    snprintf(out, cap,
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        bytes[0], bytes[1], bytes[2], bytes[3],
        bytes[4], bytes[5],
        bytes[6], bytes[7],
        bytes[8], bytes[9],
        bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
}

/**
 * 构建 token: 将 intToken、id、update_tag 序列化为 JSON
 * 返回值需要 free
 */
static char *btm_build_token(const char *int_token, const char *id, const char *update_tag) {
    cJSON *obj = cJSON_CreateObject();
    if (!obj) return NULL;
    cJSON_AddStringToObject(obj, "intToken", int_token ? int_token : "");
    cJSON_AddStringToObject(obj, "id", id ? id : "");
    cJSON_AddStringToObject(obj, "update_tag", update_tag ? update_tag : "");
    char *json = cJSON_PrintUnformatted(obj);
    cJSON_Delete(obj);
    return json;
}

/**
 * 从 token 中解析 intToken、id、update_tag
 * 成功返回 0，失败返回 -1
 * 输出的字符串需要 free
 */
static int btm_parse_token(const char *token, char **int_token, char **id, char **update_tag) {
    *int_token = NULL;
    *id = NULL;
    *update_tag = NULL;

    if (!token || !token[0]) return -1;

    cJSON *obj = cJSON_Parse(token);
    if (!obj) return -1;

    const cJSON *j_int_token = cJSON_GetObjectItemCaseSensitive(obj, "intToken");
    const cJSON *j_id = cJSON_GetObjectItemCaseSensitive(obj, "id");
    const cJSON *j_update_tag = cJSON_GetObjectItemCaseSensitive(obj, "update_tag");

    if (!cJSON_IsString(j_int_token) || !j_int_token->valuestring ||
        !cJSON_IsString(j_id) || !j_id->valuestring ||
        !cJSON_IsString(j_update_tag) || !j_update_tag->valuestring) {
        cJSON_Delete(obj);
        return -1;
    }

    *int_token = tm_strdup(j_int_token->valuestring);
    *id = tm_strdup(j_id->valuestring);
    *update_tag = tm_strdup(j_update_tag->valuestring);

    cJSON_Delete(obj);

    if (!*int_token || !*id || !*update_tag) {
        free(*int_token);
        free(*id);
        free(*update_tag);
        *int_token = NULL;
        *id = NULL;
        *update_tag = NULL;
        return -1;
    }

    return 0;
}

/* ========== 创建邮箱 ========== */

/**
 * 创建 best-temp-mail 临时邮箱
 *
 * 流程:
 *   1. 生成 UUID v4 作为 intToken
 *   2. POST /api/v3/createEmail {"intToken":"<uuid>"}
 *   3. 解析响应提取 address, id, update_tag
 *   4. 将三者序列化为 JSON 存入 token
 */
tm_email_info_t *tm_provider_best_temp_mail_generate(void) {
    int timeout = tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

    /* 生成 UUID */
    char uuid[48];
    btm_generate_uuid(uuid, sizeof(uuid));

    /* 构造请求体 */
    cJSON *body_obj = cJSON_CreateObject();
    if (!body_obj) return NULL;
    cJSON_AddStringToObject(body_obj, "intToken", uuid);
    char *body_str = cJSON_PrintUnformatted(body_obj);
    cJSON_Delete(body_obj);
    if (!body_str) return NULL;

    /* 请求头 */
    const char *headers[] = {
        btm_ua,
        "Content-Type: application/json",
        "Accept: application/json, text/plain, */*",
        "Referer: https://best-temp-mail.com/",
        "Origin: https://best-temp-mail.com",
        NULL
    };

    /* 发送请求 */
    tm_http_response_t *resp = tm_http_request(
        TM_HTTP_POST,
        BTM_BASE "/api/v3/createEmail",
        headers,
        body_str,
        timeout
    );
    free(body_str);

    if (!resp || resp->status != 200 || !resp->body) {
        TM_LOG_ERR("[best-temp-mail] createEmail 请求失败, status=%ld", resp ? resp->status : 0);
        tm_http_response_free(resp);
        return NULL;
    }

    /* 解析响应 */
    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);

    if (!json) {
        TM_LOG_ERR("[best-temp-mail] 解析 createEmail 响应 JSON 失败");
        return NULL;
    }

    /* 检查状态 */
    const cJSON *j_status = cJSON_GetObjectItemCaseSensitive(json, "status");
    if (!cJSON_IsString(j_status) || strcmp(j_status->valuestring, "success") != 0) {
        TM_LOG_ERR("[best-temp-mail] createEmail 响应状态非 success");
        cJSON_Delete(json);
        return NULL;
    }

    /* 提取 data 对象 */
    const cJSON *j_data = cJSON_GetObjectItemCaseSensitive(json, "data");
    if (!cJSON_IsObject(j_data)) {
        TM_LOG_ERR("[best-temp-mail] createEmail 响应中无 data 对象");
        cJSON_Delete(json);
        return NULL;
    }

    const cJSON *j_address = cJSON_GetObjectItemCaseSensitive(j_data, "address");
    const cJSON *j_id = cJSON_GetObjectItemCaseSensitive(j_data, "id");
    const cJSON *j_update_tag = cJSON_GetObjectItemCaseSensitive(j_data, "update_tag");

    if (!cJSON_IsString(j_address) || !j_address->valuestring || !j_address->valuestring[0] ||
        !cJSON_IsString(j_id) || !j_id->valuestring ||
        !cJSON_IsString(j_update_tag) || !j_update_tag->valuestring) {
        TM_LOG_ERR("[best-temp-mail] createEmail 响应中缺少必要字段");
        cJSON_Delete(json);
        return NULL;
    }

    /* 验证邮箱格式 */
    if (!strchr(j_address->valuestring, '@')) {
        TM_LOG_ERR("[best-temp-mail] 返回的邮箱格式无效: %s", j_address->valuestring);
        cJSON_Delete(json);
        return NULL;
    }

    /* 构建 token */
    char *token = btm_build_token(uuid, j_id->valuestring, j_update_tag->valuestring);
    if (!token) {
        cJSON_Delete(json);
        return NULL;
    }

    /* 构建结果 */
    tm_email_info_t *info = tm_email_info_new();
    if (!info) {
        free(token);
        cJSON_Delete(json);
        return NULL;
    }

    info->channel = CHANNEL_BEST_TEMP_MAIL;
    info->email = tm_strdup(j_address->valuestring);
    info->token = token;
    info->expires_at = 0;
    info->created_at = tm_strdup("");

    cJSON_Delete(json);

    TM_LOG_DBG("[best-temp-mail] 创建邮箱成功: %s", info->email);
    return info;
}

/* ========== 获取邮件 ========== */

/**
 * 获取 best-temp-mail 邮件列表
 *
 * 流程:
 *   1. 从 token 解析 intToken、id、update_tag
 *   2. POST /api/v3/getEmailList
 *   3. 解析响应中的邮件数组并标准化
 */
tm_email_t *tm_provider_best_temp_mail_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    if (!token || !email) return NULL;

    int timeout = tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

    /* 解析 token */
    char *int_token = NULL;
    char *id = NULL;
    char *update_tag = NULL;
    if (btm_parse_token(token, &int_token, &id, &update_tag) != 0) {
        TM_LOG_ERR("[best-temp-mail] 解析 token 失败");
        return NULL;
    }

    /* 构造请求体 */
    cJSON *body_obj = cJSON_CreateObject();
    if (!body_obj) {
        free(int_token);
        free(id);
        free(update_tag);
        return NULL;
    }
    cJSON_AddStringToObject(body_obj, "address", email);
    cJSON_AddStringToObject(body_obj, "id", id);
    cJSON_AddStringToObject(body_obj, "intToken", int_token);
    cJSON_AddStringToObject(body_obj, "update_tag", update_tag);
    char *body_str = cJSON_PrintUnformatted(body_obj);
    cJSON_Delete(body_obj);

    free(int_token);
    free(id);
    free(update_tag);

    if (!body_str) return NULL;

    /* 请求头 */
    const char *headers[] = {
        btm_ua,
        "Content-Type: application/json",
        "Accept: application/json, text/plain, */*",
        "Referer: https://best-temp-mail.com/",
        "Origin: https://best-temp-mail.com",
        NULL
    };

    /* 发送请求 */
    tm_http_response_t *resp = tm_http_request(
        TM_HTTP_POST,
        BTM_BASE "/api/v3/getEmailList",
        headers,
        body_str,
        timeout
    );
    free(body_str);

    if (!resp || resp->status != 200 || !resp->body) {
        TM_LOG_ERR("[best-temp-mail] getEmailList 请求失败, status=%ld", resp ? resp->status : 0);
        tm_http_response_free(resp);
        return NULL;
    }

    /* 解析响应 */
    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);

    if (!json) {
        TM_LOG_ERR("[best-temp-mail] 解析 getEmailList 响应 JSON 失败");
        return NULL;
    }

    /* 检查状态 */
    const cJSON *j_status = cJSON_GetObjectItemCaseSensitive(json, "status");
    if (!cJSON_IsString(j_status) || strcmp(j_status->valuestring, "success") != 0) {
        TM_LOG_ERR("[best-temp-mail] getEmailList 响应状态非 success");
        cJSON_Delete(json);
        return NULL;
    }

    /* 提取 data 对象 */
    const cJSON *j_data = cJSON_GetObjectItemCaseSensitive(json, "data");
    if (!cJSON_IsObject(j_data)) {
        TM_LOG_ERR("[best-temp-mail] getEmailList 响应中无 data 对象");
        cJSON_Delete(json);
        return NULL;
    }

    /* 检查是否有新邮件 */
    const cJSON *j_has_new = cJSON_GetObjectItemCaseSensitive(j_data, "hasNewEmail");
    if (cJSON_IsBool(j_has_new) && !cJSON_IsTrue(j_has_new)) {
        /* 无新邮件 */
        cJSON_Delete(json);
        *count = 0;
        return NULL;
    }

    /* 提取邮件数组 */
    const cJSON *j_emails = cJSON_GetObjectItemCaseSensitive(j_data, "emails");
    if (!cJSON_IsArray(j_emails)) {
        /* 没有 emails 字段但状态成功，视为无邮件 */
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
    if (mail_count > BTM_MAX_MAILS) mail_count = BTM_MAX_MAILS;

    tm_email_t *emails = tm_emails_new(mail_count);
    if (!emails) {
        cJSON_Delete(json);
        return NULL;
    }

    int valid_count = 0;
    for (int i = 0; i < mail_count; i++) {
        const cJSON *item = cJSON_GetArrayItem(j_emails, i);
        if (!cJSON_IsObject(item)) continue;

        /* 构建标准化邮件 JSON，传入 normalize */
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
