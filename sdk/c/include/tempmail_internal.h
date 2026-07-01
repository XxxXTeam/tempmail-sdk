#ifndef TEMPMAIL_INTERNAL_H
#define TEMPMAIL_INTERNAL_H

#include "tempmail_sdk.h"
#include "cJSON.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define TM_JSON_STR(item, def) \
    (cJSON_GetStringValue(item) ? cJSON_GetStringValue(item) : (def))

/* ========== 内部日志宏 ========== */

void tm_log(tm_log_level_t level, const char *fmt, ...);

#define TM_LOG_ERR(...)  tm_log(TM_LOG_ERROR, __VA_ARGS__)
#define TM_LOG_WARN(...) tm_log(TM_LOG_WARN, __VA_ARGS__)
#define TM_LOG_INF(...)  tm_log(TM_LOG_INFO, __VA_ARGS__)
#define TM_LOG_DBG(...)  tm_log(TM_LOG_DEBUG, __VA_ARGS__)

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

/* 与 tm_http_request 相同，但强制跳过 TLS 证书校验（10mail-wangtz 等渠道默认行为） */
tm_http_response_t* tm_http_request_skip_cert_verify(
    tm_http_method_t method,
    const char *url,
    const char **headers,
    const char *body,
    int timeout_secs
);

/* 与 tm_http_request 相同，但强制 IPv4（部分 Cloudflare 站点 IPv6 TLS 会断开） */
tm_http_response_t* tm_http_request_ipv4(
    tm_http_method_t method,
    const char *url,
    const char **headers,
    const char *body,
    int timeout_secs
);

/* 释放 HTTP 响应 */
void tm_http_response_free(tm_http_response_t *resp);

/* 匿名用量上报（内部异步 POST，失败忽略） */
void tm_telemetry_report(const char *operation, const char *channel, bool success,
    int attempt_count, int channels_tried, const char *error_msg);
/* 立即刷新队列（进程退出前可调用） */
void tm_telemetry_flush_batch(void);

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

void tm_tempmail_cn_module_init(void);
void tm_tempmail_cn_module_cleanup(void);

tm_email_info_t* tm_provider_tempmail_cn_generate(const char *domain);
tm_email_t* tm_provider_tempmail_cn_get_emails(const char *email, int *count);

tm_email_info_t* tm_provider_linshiyou_generate(void);
tm_email_t* tm_provider_linshiyou_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_tempmail_lol_generate(const char *domain);
tm_email_t* tm_provider_tempmail_lol_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_chatgpt_org_uk_generate(void);
tm_email_t* tm_provider_chatgpt_org_uk_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_awamail_generate(void);
tm_email_t* tm_provider_awamail_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_mail_tm_generate(void);
tm_email_t* tm_provider_mail_tm_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_dropmail_generate(void);
tm_email_t* tm_provider_dropmail_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_guerrillamail_generate(void);
tm_email_t* tm_provider_guerrillamail_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_maildrop_generate(const char *domain);
tm_email_t* tm_provider_maildrop_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_smail_pw_generate(void);
tm_email_t* tm_provider_smail_pw_get_emails(const char *token, const char *email, int *count);

void tm_vip215_module_init(void);
void tm_vip215_module_cleanup(void);

tm_email_info_t* tm_provider_vip215_generate(void);
tm_email_t* tm_provider_vip215_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_fake_legal_generate(const char *domain);
tm_email_info_t* tm_provider_mffac_generate(void);
tm_email_t* tm_provider_fake_legal_get_emails(const char *email, int *count);
tm_email_t* tm_provider_mffac_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_ta_easy_generate(void);
tm_email_t* tm_provider_ta_easy_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_moakt_generate(const char *domain);
tm_email_t* tm_provider_moakt_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_tenminute_one_generate(const char *domain);
tm_email_t* tm_provider_tenminute_one_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_email10min_generate(void);
tm_email_t* tm_provider_email10min_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_harakirimail_generate(void);
tm_email_t* tm_provider_harakirimail_get_emails(const char *email, int *count);

tm_email_info_t* tm_provider_tempmail_plus_generate(const char *domain, tm_channel_t channel);
tm_email_t* tm_provider_tempmail_plus_get_emails(const char *email, int *count);


tm_email_info_t* tm_provider_tempmail_lol_v2_generate(void);
tm_email_t* tm_provider_tempmail_lol_v2_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_tempgbox_generate(void);
tm_email_t* tm_provider_tempgbox_get_emails(const char *device_id, const char *email, int *count);

tm_email_info_t* tm_provider_emailnator_generate(void);
tm_email_t* tm_provider_emailnator_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_temporam_generate(const char *preferred_domain);
tm_email_t* tm_provider_temporam_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_guerrillamail_mirror_generate(tm_channel_t channel, const char *base_url);
tm_email_t* tm_provider_guerrillamail_mirror_get_emails(const char *base_url, const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_temp_mail_io_generate(void);
tm_email_t* tm_provider_temp_mail_io_get_emails(const char *email, int *count);

tm_email_info_t* tm_provider_mail_cx_generate(const char *domain);
tm_email_t* tm_provider_mail_cx_get_emails(const char *client_id, const char *email, int *count);

tm_email_info_t* tm_provider_catchmail_generate(const char *domain);
tm_email_t* tm_provider_catchmail_get_emails(const char *email, int *count);

tm_email_info_t* tm_provider_mailforspam_generate(const char *domain);
tm_email_t* tm_provider_mailforspam_get_emails(const char *email, int *count);


tm_email_info_t* tm_provider_tempmailc_generate(void);
tm_email_t* tm_provider_tempmailc_get_emails(const char *email, int *count);

tm_email_info_t* tm_provider_mailnesia_generate(void);
tm_email_t* tm_provider_mailnesia_get_emails(const char *email, int *count);

tm_email_info_t* tm_provider_throwawaymail_generate(void);
tm_email_t* tm_provider_throwawaymail_get_emails(const char *mailbox_id, const char *email, int *count);

tm_email_info_t* tm_provider_shitty_email_generate(void);
tm_email_t* tm_provider_shitty_email_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_tempmailpro_generate(void);
tm_email_t* tm_provider_tempmailpro_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_devmail_uk_generate(void);
tm_email_t* tm_provider_devmail_uk_get_emails(const char *email, int *count);

tm_email_info_t* tm_provider_neighbours_generate(const char *preferred_domain);
tm_email_t* tm_provider_neighbours_get_emails(const char *email, int *count);


tm_email_info_t* tm_provider_cleantempmail_generate(void);
tm_email_t* tm_provider_cleantempmail_get_emails(const char *email, int *count);

tm_email_info_t* tm_provider_inboxkitten_generate(void);
tm_email_t* tm_provider_inboxkitten_get_emails(const char *email, int *count);

tm_email_info_t* tm_provider_getnada_generate(const char *domain);
tm_email_t* tm_provider_getnada_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_mail123_generate(void);
tm_email_t* tm_provider_mail123_get_emails(const char *email, int *count);

tm_email_info_t* tm_provider_mail10s_generate(void);
tm_email_t* tm_provider_mail10s_get_emails(const char *email, int *count);

tm_email_info_t* tm_provider_webmailtemp_generate(void);
tm_email_t* tm_provider_webmailtemp_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_tempfastmail_generate(void);
tm_email_t* tm_provider_tempfastmail_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_one_sec_mail_generate(void);
tm_email_t* tm_provider_one_sec_mail_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_fakemail_generate(void);
tm_email_t* tm_provider_fakemail_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_openinbox_generate(void);
tm_email_t* tm_provider_openinbox_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_inboxes_generate(const char *domain);
tm_email_t* tm_provider_inboxes_get_emails(const char *email, int *count);

tm_email_info_t* tm_provider_uncorreotemporal_generate(void);
tm_email_t* tm_provider_uncorreotemporal_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_zhujump_generate(const char *domain, tm_channel_t channel);
tm_email_info_t* tm_provider_moemail_generate(const char *base_url, const char *domain, tm_channel_t channel, int expiry_time);
tm_email_t* tm_provider_zhujump_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_m2u_generate(void);
tm_email_t* tm_provider_m2u_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_tempy_email_generate(const char *domain);
tm_email_t* tm_provider_tempy_email_get_emails(const char *email, int *count);

tm_email_info_t* tm_provider_fmail_generate(const char *domain);
tm_email_t* tm_provider_fmail_get_emails(const char *email, int *count);

tm_email_info_t* tm_provider_ockito_generate(void);
tm_email_t* tm_provider_ockito_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_anonbox_generate(void);
tm_email_t* tm_provider_anonbox_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_duckmail_generate(void);
tm_email_t* tm_provider_duckmail_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_mailinator_generate(void);
tm_email_t* tm_provider_mailinator_get_emails(const char *email, int *count);


tm_email_info_t* tm_tempmail365_generate(const char *domain);
tm_email_t* tm_tempmail365_get_emails(const char *email, int *count);

tm_email_info_t* tm_provider_tempinbox_generate(const char *domain);
tm_email_t* tm_provider_tempinbox_get_emails(const char *email, int *count);

tm_email_info_t* tm_provider_byom_generate(void);
tm_email_t* tm_provider_byom_get_emails(const char *email, int *count);

tm_email_info_t* tm_provider_anonymmail_generate(void);
tm_email_t* tm_provider_anonymmail_get_emails(const char *email, int *count);

tm_email_info_t* tm_provider_eyepaste_generate(void);
tm_email_t* tm_provider_eyepaste_get_emails(const char *email, int *count);

tm_email_info_t* tm_provider_segamail_generate(void);
tm_email_t* tm_provider_segamail_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_mail_sunls_generate(void);
tm_email_t* tm_provider_mail_sunls_get_emails(const char *email, int *count);


tm_email_info_t* tm_provider_expressinboxhub_generate(void);
tm_email_t* tm_provider_expressinboxhub_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_lroid_generate(void);
tm_email_t* tm_provider_lroid_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_haribu_generate(void);
tm_email_t* tm_provider_haribu_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_pleasenospam_generate(void);
tm_email_t* tm_provider_pleasenospam_get_emails(const char *email, int *count);

tm_email_info_t* tm_provider_rootsh_generate(void);
tm_email_t* tm_provider_rootsh_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_fake_email_site_generate(void);
tm_email_t* tm_provider_fake_email_site_get_emails(const char *email, int *count);

tm_email_info_t* tm_provider_mohmal_generate(void);
tm_email_t* tm_provider_mohmal_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_mailgolem_generate(void);
tm_email_t* tm_provider_mailgolem_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_best_temp_mail_generate(void);
tm_email_t* tm_provider_best_temp_mail_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_disposablemail_app_generate(void);
tm_email_t* tm_provider_disposablemail_app_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_mailtemp_cc_generate(void);
tm_email_t* tm_provider_mailtemp_cc_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_minuteinbox_generate(void);
tm_email_t* tm_provider_minuteinbox_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_mailcatch_generate(void);
tm_email_t* tm_provider_mailcatch_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_tempemail_co_generate(void);
tm_email_t* tm_provider_tempemail_co_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_tempemails_net_generate(void);
tm_email_t* tm_provider_tempemails_net_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_altmails_generate(void);
tm_email_t* tm_provider_altmails_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_tempemail_info_generate(void);
tm_email_t* tm_provider_tempemail_info_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_smailpro_generate(void);
tm_email_t* tm_provider_smailpro_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_tempmailten_generate(void);
tm_email_t* tm_provider_tempmailten_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_maildrop_cc_generate(void);
tm_email_t* tm_provider_maildrop_cc_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_tenminutemail_net_generate(void);
tm_email_t* tm_provider_tenminutemail_net_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_linshiyouxiang_net_generate(void);
tm_email_t* tm_provider_linshiyouxiang_net_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_tempmail_fyi_generate(void);
tm_email_t* tm_provider_tempmail_fyi_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_disposablemail_com_generate(void);
tm_email_t* tm_provider_disposablemail_com_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_tempp_mails_generate(void);
tm_email_t* tm_provider_tempp_mails_get_emails(const char *token, const char *email, int *count);

tm_email_info_t* tm_provider_emailtemp_org_generate(void);
tm_email_t* tm_provider_emailtemp_org_get_emails(const char *token, const char *email, int *count);

#endif /* TEMPMAIL_INTERNAL_H */
