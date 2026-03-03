/**
 * SDK 内部头文件，不对外暴露
 */

#ifndef TEMPMAIL_INTERNAL_H
#define TEMPMAIL_INTERNAL_H

#include "tempmail_sdk.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* 安全获取 cJSON 字符串值，NULL 时返回默认值 */
#define TM_JSON_STR(item, def) \
    (cJSON_GetStringValue(item) ? cJSON_GetStringValue(item) : (def))

/* ========== 内部日志宏 ========== */

void tm_log(tm_log_level_t level, const char *fmt, ...);

#define TM_LOG_ERR(...)  tm_log(TM_LOG_ERROR, __VA_ARGS__)
#define TM_LOG_WARN(...) tm_log(TM_LOG_WARN, __VA_ARGS__)
#define TM_LOG_INF(...)  tm_log(TM_LOG_INFO, __VA_ARGS__)
#define TM_LOG_DBG(...)  tm_log(TM_LOG_DEBUG, __VA_ARGS__)

/* ========== HTTP 工具 ========== */

/* HTTP 响应 */
typedef struct {
    char *body;      /* 响应体（需要 free） */
    size_t size;     /* 响应体大小 */
    long status;     /* HTTP 状态码 */
    char *cookies;   /* Set-Cookie（需要 free，可能为 NULL） */
} tm_http_response_t;

/* HTTP 请求方法 */
typedef enum {
    TM_HTTP_GET,
    TM_HTTP_POST,
} tm_http_method_t;

/* 发送 HTTP 请求 */
tm_http_response_t* tm_http_request(
    tm_http_method_t method,
    const char *url,
    const char **headers,  /* NULL-terminated 数组: ["Key: Value", ..., NULL] */
    const char *body,      /* POST body，GET 时可为 NULL */
    int timeout_secs
);

/* 释放 HTTP 响应 */
void tm_http_response_free(tm_http_response_t *resp);

/* ========== 字符串工具 ========== */

/* 安全复制字符串（NULL 返回空串） */
char* tm_strdup(const char *s);

/* 从 cJSON 对象中按候选 key 提取字符串 */
char* tm_json_get_str(const cJSON *obj, const char **keys, int key_count);

/* ========== 内存分配邮件结构 ========== */

tm_email_info_t* tm_email_info_new(void);
tm_email_t* tm_emails_new(int count);

/* ========== normalize ========== */

tm_email_t tm_normalize_email(const cJSON *raw, const char *recipient);

/* ========== provider 函数签名 ========== */

tm_email_info_t* tm_provider_tempmail_generate(int duration);
tm_email_t* tm_provider_tempmail_get_emails(const char *email, int *count);

tm_email_info_t* tm_provider_linshi_email_generate(void);
tm_email_t* tm_provider_linshi_email_get_emails(const char *email, int *count);

tm_email_info_t* tm_provider_tempmail_lol_generate(const char *domain);
tm_email_t* tm_provider_tempmail_lol_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_chatgpt_org_uk_generate(void);
tm_email_t* tm_provider_chatgpt_org_uk_get_emails(const char *email, int *count);

tm_email_info_t* tm_provider_tempmail_la_generate(void);
tm_email_t* tm_provider_tempmail_la_get_emails(const char *email, int *count);

tm_email_info_t* tm_provider_temp_mail_io_generate(void);
tm_email_t* tm_provider_temp_mail_io_get_emails(const char *email, int *count);

tm_email_info_t* tm_provider_awamail_generate(void);
tm_email_t* tm_provider_awamail_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_mail_tm_generate(void);
tm_email_t* tm_provider_mail_tm_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_dropmail_generate(void);
tm_email_t* tm_provider_dropmail_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_guerrillamail_generate(void);
tm_email_t* tm_provider_guerrillamail_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_maildrop_generate(void);
tm_email_t* tm_provider_maildrop_get_emails(const char *token, const char *email, int *count);

#endif /* TEMPMAIL_INTERNAL_H */
