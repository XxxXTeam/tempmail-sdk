/**
 * 临时邮箱 SDK (C)
 * 支持 11 个邮箱服务提供商，所有渠道返回统一标准化格式
 */

#ifndef TEMPMAIL_SDK_H
#define TEMPMAIL_SDK_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========== 渠道枚举 ========== */

typedef enum {
    CHANNEL_TEMPMAIL = 0,
    CHANNEL_LINSHI_EMAIL,
    CHANNEL_TEMPMAIL_LOL,
    CHANNEL_CHATGPT_ORG_UK,
    CHANNEL_TEMPMAIL_LA,
    CHANNEL_TEMP_MAIL_IO,
    CHANNEL_AWAMAIL,
    CHANNEL_MAIL_TM,
    CHANNEL_DROPMAIL,
    CHANNEL_GUERRILLAMAIL,
    CHANNEL_MAILDROP,
    CHANNEL_COUNT,       /* 渠道总数 */
    CHANNEL_RANDOM = -1, /* 随机选择 */
} tm_channel_t;

/* ========== 日志级别 ========== */

typedef enum {
    TM_LOG_SILENT = 0,
    TM_LOG_ERROR  = 1,
    TM_LOG_WARN   = 2,
    TM_LOG_INFO   = 3,
    TM_LOG_DEBUG  = 4,
} tm_log_level_t;

/* 自定义日志回调函数类型 */
typedef void (*tm_log_handler_t)(tm_log_level_t level, const char *msg);

/* 设置日志级别 */
void tm_set_log_level(tm_log_level_t level);

/* 设置自定义日志处理器 */
void tm_set_log_handler(tm_log_handler_t handler);

/* ========== 数据结构 ========== */

/* 邮件附件 */
typedef struct {
    char *filename;     /* 文件名 */
    long size;          /* 大小（字节），-1 表示未知 */
    char *content_type; /* MIME 类型 */
    char *url;          /* 下载地址 */
} tm_attachment_t;

/* 标准化邮件 */
typedef struct {
    char *id;           /* 邮件唯一标识 */
    char *from_addr;    /* 发件人地址 */
    char *to;           /* 收件人地址 */
    char *subject;      /* 主题 */
    char *text;         /* 纯文本内容 */
    char *html;         /* HTML 内容 */
    char *date;         /* 日期（ISO 8601） */
    bool is_read;       /* 是否已读 */
    tm_attachment_t *attachments; /* 附件数组 */
    int attachment_count;        /* 附件数量 */
} tm_email_t;

/* 邮箱信息（创建后返回） */
typedef struct {
    tm_channel_t channel; /* 渠道 */
    char *email;          /* 邮箱地址 */
    char *token;          /* 认证令牌 */
    long long expires_at; /* 过期时间（毫秒时间戳），0 表示未知 */
    char *created_at;     /* 创建时间 */
} tm_email_info_t;

/* 获取邮件结果 */
typedef struct {
    tm_channel_t channel; /* 渠道 */
    char *email;          /* 邮箱地址 */
    tm_email_t *emails;   /* 邮件数组 */
    int email_count;      /* 邮件数量 */
    bool success;         /* 是否成功 */
    char *error;          /* 错误信息（success=false 时） */
} tm_get_emails_result_t;

/* 重试配置 */
typedef struct {
    int max_retries;       /* 最大重试次数，默认 2 */
    int initial_delay_ms;  /* 初始延迟毫秒，默认 1000 */
    int max_delay_ms;      /* 最大延迟毫秒，默认 5000 */
    int timeout_secs;      /* 请求超时秒，默认 15 */
} tm_retry_config_t;

/* 创建邮箱选项 */
typedef struct {
    tm_channel_t channel;        /* 渠道，CHANNEL_RANDOM 随机 */
    int duration;                /* tempmail 有效期（分钟） */
    const char *domain;          /* tempmail-lol 域名 */
    tm_retry_config_t *retry;    /* 重试配置，NULL 使用默认 */
} tm_generate_options_t;

/* 获取邮件选项 */
typedef struct {
    tm_channel_t channel;        /* 渠道 */
    const char *email;           /* 邮箱地址 */
    const char *token;           /* 认证令牌 */
    tm_retry_config_t *retry;    /* 重试配置，NULL 使用默认 */
} tm_get_emails_options_t;

/* 渠道信息 */
typedef struct {
    tm_channel_t channel;
    const char *name;
    const char *website;
} tm_channel_info_t;

/* ========== API 函数 ========== */

/**
 * 初始化 SDK（必须在使用前调用）
 * 内部初始化 libcurl
 */
void tm_init(void);

/**
 * 清理 SDK（程序退出前调用）
 */
void tm_cleanup(void);

/**
 * 获取所有支持的渠道列表
 * 返回 CHANNEL_COUNT 个渠道信息
 */
const tm_channel_info_t* tm_list_channels(int *count);

/**
 * 获取渠道名称字符串
 */
const char* tm_channel_name(tm_channel_t channel);

/**
 * 创建临时邮箱
 * 成功返回 EmailInfo 指针，失败返回 NULL
 * 调用者需使用 tm_free_email_info() 释放
 */
tm_email_info_t* tm_generate_email(const tm_generate_options_t *options);

/**
 * 获取邮件列表
 * 始终返回结果（即使失败也返回 success=false 的结果）
 * 调用者需使用 tm_free_get_emails_result() 释放
 */
tm_get_emails_result_t* tm_get_emails(const tm_get_emails_options_t *options);

/* ========== 内存释放函数 ========== */

void tm_free_email_info(tm_email_info_t *info);
void tm_free_email(tm_email_t *email);
void tm_free_get_emails_result(tm_get_emails_result_t *result);

/* ========== 默认重试配置 ========== */

tm_retry_config_t tm_default_retry_config(void);

#ifdef __cplusplus
}
#endif

#endif /* TEMPMAIL_SDK_H */
