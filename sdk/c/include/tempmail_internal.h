#ifndef TEMPMAIL_INTERNAL_H
#define TEMPMAIL_INTERNAL_H

#include "cJSON.h"
#include "tempmail_registry.h"
#include "tempmail_sdk.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define TM_JSON_STR(item, def)                                                 \
  (cJSON_GetStringValue(item) ? cJSON_GetStringValue(item) : (def))

/* ========== 内部日志宏 ========== */

void tm_log(tm_log_level_t level, const char *fmt, ...);

/* 返回当前日志处理器 / 级别（WebUI 内部使用） */
tm_log_handler_t tm_get_log_handler(void);
tm_log_level_t tm_get_log_level(void);

#define TM_LOG_ERR(...) tm_log(TM_LOG_ERROR, __VA_ARGS__)
#define TM_LOG_WARN(...) tm_log(TM_LOG_WARN, __VA_ARGS__)
#define TM_LOG_INF(...) tm_log(TM_LOG_INFO, __VA_ARGS__)
#define TM_LOG_DBG(...) tm_log(TM_LOG_DEBUG, __VA_ARGS__)

/* HTTP 响应 */
typedef struct {
  char *body;    /* 响应体（需要 free） */
  size_t size;   /* 响应体大小 */
  long status;   /* HTTP 状态码 */
  char *cookies; /* Set-Cookie（需要 free，可能为 NULL） */
} tm_http_response_t;

/* HTTP 请求方法 */
typedef enum {
  TM_HTTP_GET,
  TM_HTTP_POST,
} tm_http_method_t;

/* 发送 HTTP 请求 */
tm_http_response_t *tm_http_request(
    tm_http_method_t method, const char *url,
    const char **headers, /* NULL-terminated 数组: ["Key: Value", ..., NULL] */
    const char *body,     /* POST body，GET 时可为 NULL */
    int timeout_secs);

/* 与 tm_http_request 相同，但强制跳过 TLS 证书校验（10mail-wangtz
 * 等渠道默认行为） */
tm_http_response_t *tm_http_request_skip_cert_verify(tm_http_method_t method,
                                                     const char *url,
                                                     const char **headers,
                                                     const char *body,
                                                     int timeout_secs);

/* 与 tm_http_request 相同，但强制 IPv4（部分 Cloudflare 站点 IPv6 TLS 会断开）
 */
tm_http_response_t *tm_http_request_ipv4(tm_http_method_t method,
                                         const char *url, const char **headers,
                                         const char *body, int timeout_secs);

/* 释放 HTTP 响应 */
void tm_http_response_free(tm_http_response_t *resp);

/* 匿名用量上报（内部异步 POST，失败忽略） */
void tm_telemetry_report(const char *operation, const char *channel,
                         bool success, int attempt_count, int channels_tried,
                         const char *error_msg);
/* 立即刷新队列（进程退出前可调用） */
void tm_telemetry_flush_batch(void);

/* ========== 字符串工具 ========== */

/* 安全复制字符串（NULL 返回空串） */
char *tm_strdup(const char *s);

/* 从 cJSON 对象中按候选 key 提取字符串 */
char *tm_json_get_str(const cJSON *obj, const char **keys, int key_count);

/* ========== 内存分配邮件结构 ========== */

tm_email_info_t *tm_email_info_new(void);
tm_email_t *tm_emails_new(int count);

/* 设置 EmailInfo 的渠道标识并原样返回（供镜像/别名渠道 thunk 使用） */
tm_email_info_t *tm_with_channel(tm_email_info_t *info, tm_channel_t channel);

/* ========== normalize ========== */

tm_email_t tm_normalize_email(const cJSON *raw, const char *recipient);

/* ========== provider 函数签名 ========== */

tm_email_info_t *tm_provider_tempmail_generate(int duration);
tm_email_t *tm_provider_tempmail_get_emails(const char *email, int *count);

void tm_tempmail_cn_module_init(void);
void tm_tempmail_cn_module_cleanup(void);

tm_email_info_t *tm_provider_tempmail_cn_generate(const char *domain);
tm_email_t *tm_provider_tempmail_cn_get_emails(const char *email, int *count);

tm_email_info_t *tm_provider_linshiyou_generate(void);
tm_email_t *tm_provider_linshiyou_get_emails(const char *token,
                                             const char *email, int *count);

tm_email_info_t *tm_provider_tempmail_lol_generate(const char *domain);
tm_email_t *tm_provider_tempmail_lol_get_emails(const char *token,
                                                const char *email, int *count);

tm_email_info_t *tm_provider_chatgpt_org_uk_generate(void);
tm_email_t *tm_provider_chatgpt_org_uk_get_emails(const char *token,
                                                  const char *email,
                                                  int *count);

tm_email_info_t *tm_provider_awamail_generate(void);
tm_email_t *tm_provider_awamail_get_emails(const char *token, const char *email,
                                           int *count);

tm_email_info_t *tm_provider_mail_tm_generate(void);
tm_email_t *tm_provider_mail_tm_get_emails(const char *token, const char *email,
                                           int *count);

tm_email_info_t *tm_provider_dropmail_generate(void);
tm_email_t *tm_provider_dropmail_get_emails(const char *token,
                                            const char *email, int *count);

tm_email_info_t *tm_provider_guerrillamail_generate(void);
tm_email_t *tm_provider_guerrillamail_get_emails(const char *token,
                                                 const char *email, int *count);

tm_email_info_t *tm_provider_maildrop_generate(const char *domain);
tm_email_t *tm_provider_maildrop_get_emails(const char *token,
                                            const char *email, int *count);

tm_email_info_t *tm_provider_smail_pw_generate(void);
tm_email_t *tm_provider_smail_pw_get_emails(const char *token,
                                            const char *email, int *count);

void tm_vip215_module_init(void);
void tm_vip215_module_cleanup(void);

tm_email_info_t *tm_provider_vip215_generate(void);
tm_email_t *tm_provider_vip215_get_emails(const char *token, const char *email,
                                          int *count);

tm_email_info_t *tm_provider_fake_legal_generate(const char *domain);
tm_email_info_t *tm_provider_mffac_generate(void);
tm_email_t *tm_provider_fake_legal_get_emails(const char *email, int *count);
tm_email_t *tm_provider_mffac_get_emails(const char *token, const char *email,
                                         int *count);

tm_email_info_t *tm_provider_ta_easy_generate(void);
tm_email_t *tm_provider_ta_easy_get_emails(const char *token, const char *email,
                                           int *count);

tm_email_info_t *tm_provider_moakt_generate(const char *domain);
tm_email_t *tm_provider_moakt_get_emails(const char *token, const char *email,
                                         int *count);

tm_email_info_t *tm_provider_tenminute_one_generate(const char *domain);
tm_email_t *tm_provider_tenminute_one_get_emails(const char *token,
                                                 const char *email, int *count);

tm_email_info_t *tm_provider_email10min_generate(void);
tm_email_t *tm_provider_email10min_get_emails(const char *token,
                                              const char *email, int *count);

tm_email_info_t *tm_provider_harakirimail_generate(void);
tm_email_t *tm_provider_harakirimail_get_emails(const char *email, int *count);

tm_email_info_t *tm_provider_tempmail_plus_generate(const char *domain,
                                                    tm_channel_t channel);
tm_email_t *tm_provider_tempmail_plus_get_emails(const char *email, int *count);

tm_email_info_t *tm_provider_tempmail_lol_v2_generate(void);
tm_email_t *tm_provider_tempmail_lol_v2_get_emails(const char *token,
                                                   const char *email,
                                                   int *count);

tm_email_info_t *tm_provider_tempgbox_generate(void);
tm_email_t *tm_provider_tempgbox_get_emails(const char *device_id,
                                            const char *email, int *count);

tm_email_info_t *tm_provider_emailnator_generate(void);
tm_email_t *tm_provider_emailnator_get_emails(const char *token,
                                              const char *email, int *count);

tm_email_info_t *tm_provider_temporam_generate(const char *preferred_domain);
tm_email_t *tm_provider_temporam_get_emails(const char *token,
                                            const char *email, int *count);

tm_email_info_t *
tm_provider_guerrillamail_mirror_generate(tm_channel_t channel,
                                          const char *base_url);
tm_email_t *tm_provider_guerrillamail_mirror_get_emails(const char *base_url,
                                                        const char *token,
                                                        const char *email,
                                                        int *count);

tm_email_info_t *tm_provider_temp_mail_io_generate(void);
tm_email_t *tm_provider_temp_mail_io_get_emails(const char *email, int *count);

tm_email_info_t *tm_provider_mail_cx_generate(const char *domain);
tm_email_t *tm_provider_mail_cx_get_emails(const char *client_id,
                                           const char *email, int *count);

tm_email_info_t *tm_provider_catchmail_generate(const char *domain);
tm_email_t *tm_provider_catchmail_get_emails(const char *email, int *count);

tm_email_info_t *tm_provider_mailforspam_generate(const char *domain);
tm_email_t *tm_provider_mailforspam_get_emails(const char *email, int *count);

tm_email_info_t *tm_provider_tempmailc_generate(void);
tm_email_t *tm_provider_tempmailc_get_emails(const char *email, int *count);

tm_email_info_t *tm_provider_mailnesia_generate(void);
tm_email_t *tm_provider_mailnesia_get_emails(const char *email, int *count);

tm_email_info_t *tm_provider_throwawaymail_generate(void);
tm_email_t *tm_provider_throwawaymail_get_emails(const char *mailbox_id,
                                                 const char *email, int *count);

tm_email_info_t *tm_provider_tempmail_fish_generate(void);
tm_email_t *tm_provider_tempmail_fish_get_emails(const char *token,
                                                 const char *email, int *count);

tm_email_info_t *tm_provider_neighbours_sh_generate(void);
tm_email_t *tm_provider_neighbours_sh_get_emails(const char *token,
                                                 const char *email, int *count);

tm_email_info_t *tm_provider_shitty_email_generate(void);
tm_email_t *tm_provider_shitty_email_get_emails(const char *token,
                                                const char *email, int *count);

tm_email_info_t *tm_provider_tempmailpro_generate(void);
tm_email_t *tm_provider_tempmailpro_get_emails(const char *token,
                                               const char *email, int *count);

tm_email_info_t *tm_provider_devmail_uk_generate(void);
tm_email_t *tm_provider_devmail_uk_get_emails(const char *email, int *count);

tm_email_info_t *tm_provider_neighbours_generate(const char *preferred_domain);
tm_email_t *tm_provider_neighbours_get_emails(const char *email, int *count);

tm_email_info_t *tm_provider_cleantempmail_generate(void);
tm_email_t *tm_provider_cleantempmail_get_emails(const char *email, int *count);

tm_email_info_t *tm_provider_inboxkitten_generate(void);
tm_email_t *tm_provider_inboxkitten_get_emails(const char *email, int *count);

tm_email_info_t *tm_provider_getnada_generate(const char *domain);
tm_email_t *tm_provider_getnada_get_emails(const char *token, const char *email,
                                           int *count);

tm_email_info_t *tm_provider_mail123_generate(void);
tm_email_t *tm_provider_mail123_get_emails(const char *email, int *count);

tm_email_info_t *tm_provider_mail10s_generate(void);
tm_email_t *tm_provider_mail10s_get_emails(const char *email, int *count);

tm_email_info_t *tm_provider_webmailtemp_generate(void);
tm_email_t *tm_provider_webmailtemp_get_emails(const char *token,
                                               const char *email, int *count);

tm_email_info_t *tm_provider_tempfastmail_generate(void);
tm_email_t *tm_provider_tempfastmail_get_emails(const char *token,
                                                const char *email, int *count);

tm_email_info_t *tm_provider_one_sec_mail_generate(void);
tm_email_t *tm_provider_one_sec_mail_get_emails(const char *token,
                                                const char *email, int *count);

tm_email_info_t *tm_provider_fakemail_generate(void);
tm_email_t *tm_provider_fakemail_get_emails(const char *token,
                                            const char *email, int *count);

tm_email_info_t *tm_provider_openinbox_generate(void);
tm_email_t *tm_provider_openinbox_get_emails(const char *token,
                                             const char *email, int *count);

tm_email_info_t *tm_provider_inboxes_generate(const char *domain);
tm_email_t *tm_provider_inboxes_get_emails(const char *email, int *count);

tm_email_info_t *tm_provider_uncorreotemporal_generate(void);
tm_email_t *tm_provider_uncorreotemporal_get_emails(const char *token,
                                                    const char *email,
                                                    int *count);

tm_email_info_t *tm_provider_zhujump_generate(const char *domain,
                                              tm_channel_t channel);
tm_email_info_t *tm_provider_moemail_generate(const char *base_url,
                                              const char *domain,
                                              tm_channel_t channel,
                                              int expiry_time);
tm_email_t *tm_provider_zhujump_get_emails(const char *token, const char *email,
                                           int *count);

tm_email_info_t *tm_provider_m2u_generate(void);
tm_email_t *tm_provider_m2u_get_emails(const char *token, const char *email,
                                       int *count);

tm_email_info_t *tm_provider_tempy_email_generate(const char *domain);
tm_email_t *tm_provider_tempy_email_get_emails(const char *email, int *count);

tm_email_info_t *tm_provider_fmail_generate(const char *domain);
tm_email_t *tm_provider_fmail_get_emails(const char *email, int *count);

tm_email_info_t *tm_provider_ockito_generate(void);
tm_email_t *tm_provider_ockito_get_emails(const char *token, const char *email,
                                          int *count);

tm_email_info_t *tm_provider_anonbox_generate(void);
tm_email_t *tm_provider_anonbox_get_emails(const char *token, const char *email,
                                           int *count);

tm_email_info_t *tm_provider_duckmail_generate(void);
tm_email_t *tm_provider_duckmail_get_emails(const char *token,
                                            const char *email, int *count);

tm_email_info_t *tm_provider_mailinator_generate(void);
tm_email_t *tm_provider_mailinator_get_emails(const char *email, int *count);

tm_email_info_t *tm_tempmail365_generate(const char *domain);
tm_email_t *tm_tempmail365_get_emails(const char *email, int *count);

tm_email_info_t *tm_provider_tempinbox_generate(const char *domain);
tm_email_t *tm_provider_tempinbox_get_emails(const char *email, int *count);

tm_email_info_t *tm_provider_byom_generate(void);
tm_email_t *tm_provider_byom_get_emails(const char *email, int *count);

tm_email_info_t *tm_provider_anonymmail_generate(void);
tm_email_t *tm_provider_anonymmail_get_emails(const char *email, int *count);

tm_email_info_t *tm_provider_eyepaste_generate(void);
tm_email_t *tm_provider_eyepaste_get_emails(const char *email, int *count);

tm_email_info_t *tm_provider_mail_sunls_generate(void);
tm_email_t *tm_provider_mail_sunls_get_emails(const char *email, int *count);

tm_email_info_t *tm_provider_expressinboxhub_generate(void);
tm_email_t *tm_provider_expressinboxhub_get_emails(const char *token,
                                                   const char *email,
                                                   int *count);

tm_email_info_t *tm_provider_lroid_generate(void);
tm_email_t *tm_provider_lroid_get_emails(const char *token, const char *email,
                                         int *count);

tm_email_info_t *tm_provider_haribu_generate(void);
tm_email_t *tm_provider_haribu_get_emails(const char *token, const char *email,
                                          int *count);

tm_email_info_t *tm_provider_rootsh_generate(void);
tm_email_t *tm_provider_rootsh_get_emails(const char *token, const char *email,
                                          int *count);

tm_email_info_t *tm_provider_fake_email_site_generate(void);
tm_email_t *tm_provider_fake_email_site_get_emails(const char *email,
                                                   int *count);

tm_email_info_t *tm_provider_mohmal_generate(void);
tm_email_t *tm_provider_mohmal_get_emails(const char *token, const char *email,
                                          int *count);

tm_email_info_t *tm_provider_mailgolem_generate(void);
tm_email_t *tm_provider_mailgolem_get_emails(const char *token,
                                             const char *email, int *count);

tm_email_info_t *tm_provider_best_temp_mail_generate(void);
tm_email_t *tm_provider_best_temp_mail_get_emails(const char *token,
                                                  const char *email,
                                                  int *count);

tm_email_info_t *tm_provider_disposablemail_app_generate(void);
tm_email_t *tm_provider_disposablemail_app_get_emails(const char *token,
                                                      const char *email,
                                                      int *count);

tm_email_info_t *tm_provider_mailtemp_cc_generate(void);
tm_email_t *tm_provider_mailtemp_cc_get_emails(const char *token,
                                               const char *email, int *count);

tm_email_info_t *tm_provider_minuteinbox_generate(void);
tm_email_t *tm_provider_minuteinbox_get_emails(const char *token,
                                               const char *email, int *count);

tm_email_info_t *tm_provider_mailcatch_generate(void);
tm_email_t *tm_provider_mailcatch_get_emails(const char *token,
                                             const char *email, int *count);

tm_email_info_t *tm_provider_tempemail_co_generate(void);
tm_email_t *tm_provider_tempemail_co_get_emails(const char *token,
                                                const char *email, int *count);

tm_email_info_t *tm_provider_tempemails_net_generate(void);
tm_email_t *tm_provider_tempemails_net_get_emails(const char *token,
                                                  const char *email,
                                                  int *count);

tm_email_info_t *tm_provider_altmails_generate(void);
tm_email_t *tm_provider_altmails_get_emails(const char *token,
                                            const char *email, int *count);

tm_email_info_t *tm_provider_tempemail_info_generate(void);
tm_email_t *tm_provider_tempemail_info_get_emails(const char *token,
                                                  const char *email,
                                                  int *count);

tm_email_info_t *tm_provider_smailpro_generate(void);
tm_email_t *tm_provider_smailpro_get_emails(const char *token,
                                            const char *email, int *count);

tm_email_info_t *tm_provider_tempmailten_generate(void);
tm_email_t *tm_provider_tempmailten_get_emails(const char *token,
                                               const char *email, int *count);

tm_email_info_t *tm_provider_maildrop_cc_generate(void);
tm_email_t *tm_provider_maildrop_cc_get_emails(const char *token,
                                               const char *email, int *count);

tm_email_info_t *tm_provider_tenminutemail_net_generate(void);
tm_email_t *tm_provider_tenminutemail_net_get_emails(const char *token,
                                                     const char *email,
                                                     int *count);

tm_email_info_t *tm_provider_linshiyouxiang_net_generate(void);
tm_email_t *tm_provider_linshiyouxiang_net_get_emails(const char *token,
                                                      const char *email,
                                                      int *count);

tm_email_info_t *tm_provider_tempmail_fyi_generate(void);
tm_email_t *tm_provider_tempmail_fyi_get_emails(const char *token,
                                                const char *email, int *count);

tm_email_info_t *tm_provider_disposablemail_com_generate(void);
tm_email_t *tm_provider_disposablemail_com_get_emails(const char *token,
                                                      const char *email,
                                                      int *count);

tm_email_info_t *tm_provider_tempp_mails_generate(void);
tm_email_t *tm_provider_tempp_mails_get_emails(const char *token,
                                               const char *email, int *count);

tm_email_info_t *tm_provider_emailtemp_org_generate(void);
tm_email_t *tm_provider_emailtemp_org_get_emails(const char *token,
                                                 const char *email, int *count);

tm_email_info_t *tm_provider_mytempmail_cc_generate(void);
tm_email_t *tm_provider_mytempmail_cc_get_emails(const char *token,
                                                 const char *email, int *count);

tm_email_info_t *tm_provider_temp_mail_now_generate(void);
tm_email_t *tm_provider_temp_mail_now_get_emails(const char *token,
                                                 const char *email, int *count);

tm_email_info_t *tm_provider_mail_td_generate(void);
tm_email_t *tm_provider_mail_td_get_emails(const char *token, const char *email,
                                           int *count);

/* mailhole-de（https://mailhole.de，公共临时邮箱，无需认证，token 即邮箱地址）
 */
tm_email_info_t *tm_provider_mailhole_de_generate(void);
tm_email_t *tm_provider_mailhole_de_get_emails(const char *token,
                                               const char *email, int *count);

/* tmail-link（https://tmail.link，Django CSRF + Cookie，token 为 csrftoken） */
tm_email_info_t *tm_provider_tmail_link_generate(void);
tm_email_t *tm_provider_tmail_link_get_emails(const char *token,
                                              const char *email, int *count);

/* 24mail-chacuo（http://24mail.chacuo.net，POST 注册/刷新同一接口，token
 * 即邮箱地址） */
tm_email_info_t *tm_provider_24mail_chacuo_generate(void);
tm_email_t *tm_provider_24mail_chacuo_get_emails(const char *email, int *count);

/* nimail（https://www.nimail.cn，POST 表单接口，token 即邮箱地址） */
tm_email_info_t *tm_provider_nimail_generate(void);
tm_email_t *tm_provider_nimail_get_emails(const char *token, const char *email,
                                          int *count);

/* freecustom（https://www.freecustom.email，免注册，token
 * 即邮箱地址；读信动态取匿名 JWT） */
tm_email_info_t *tm_provider_freecustom_generate(void);
tm_email_t *tm_provider_freecustom_get_emails(const char *token,
                                              const char *email, int *count);

/* apihz（接口盒子，https://cn.apihz.cn，需 id/key 凭据；token 为
 * base64(json{mail,pwd})） */
tm_email_info_t *tm_provider_apihz_generate(void);
tm_email_t *tm_provider_apihz_get_emails(const char *token, const char *email,
                                         int *count);

/* Mailinator 官方姊妹域名（MX 指向 mail.mailinator.com，读信复用 mailinator
 * provider） */
tm_email_info_t *tm_provider_sogetthis_com_generate(void);
tm_email_t *tm_provider_sogetthis_com_get_emails(const char *email, int *count);

tm_email_info_t *tm_provider_bobmail_info_generate(void);
tm_email_t *tm_provider_bobmail_info_get_emails(const char *email, int *count);

tm_email_info_t *tm_provider_suremail_info_generate(void);
tm_email_t *tm_provider_suremail_info_get_emails(const char *email, int *count);

tm_email_info_t *tm_provider_binkmail_com_generate(void);
tm_email_t *tm_provider_binkmail_com_get_emails(const char *email, int *count);

tm_email_info_t *tm_provider_veryrealemail_com_generate(void);
tm_email_t *tm_provider_veryrealemail_com_get_emails(const char *email,
                                                     int *count);

/* mailmomy（https://mailmomy.com，免注册纯 GET JSON，token 即邮箱地址） */
tm_email_info_t *tm_provider_mailmomy_generate(void);
tm_email_t *tm_provider_mailmomy_get_emails(const char *token,
                                            const char *email, int *count);

tm_email_info_t *tm_provider_chammy_info_generate(void);
tm_email_t *tm_provider_chammy_info_get_emails(const char *email, int *count);

tm_email_info_t *tm_provider_thisisnotmyrealemail_com_generate(void);
tm_email_t *tm_provider_thisisnotmyrealemail_com_get_emails(const char *email,
                                                            int *count);

tm_email_info_t *tm_provider_notmailinator_com_generate(void);
tm_email_t *tm_provider_notmailinator_com_get_emails(const char *email,
                                                     int *count);

tm_email_info_t *tm_provider_spamhereplease_com_generate(void);
tm_email_t *tm_provider_spamhereplease_com_get_emails(const char *email,
                                                      int *count);

tm_email_info_t *tm_provider_junk_beats_org_generate(void);
tm_email_t *tm_provider_junk_beats_org_get_emails(const char *email,
                                                  int *count);

tm_email_info_t *tm_provider_junk_ihmehl_com_generate(void);
tm_email_t *tm_provider_junk_ihmehl_com_get_emails(const char *email,
                                                   int *count);

tm_email_info_t *tm_provider_junk_noplay_org_generate(void);
tm_email_t *tm_provider_junk_noplay_org_get_emails(const char *email,
                                                   int *count);

tm_email_info_t *tm_provider_junk_vanillasystem_com_generate(void);
tm_email_t *tm_provider_junk_vanillasystem_com_get_emails(const char *email,
                                                          int *count);

tm_email_info_t *tm_provider_spam_jasonpearce_com_generate(void);
tm_email_t *tm_provider_spam_jasonpearce_com_get_emails(const char *email,
                                                        int *count);

tm_email_info_t *tm_provider_fish_skytale_net_generate(void);
tm_email_t *tm_provider_fish_skytale_net_get_emails(const char *email,
                                                    int *count);

tm_email_info_t *tm_provider_spam_mccrew_com_generate(void);
tm_email_t *tm_provider_spam_mccrew_com_get_emails(const char *email,
                                                   int *count);

/* dropmail-click（https://dropmail.click，独立后端，POST 创建/GET 读信，token
 * 即邮箱地址） */
tm_email_info_t *tm_provider_dropmail_click_generate(void);
tm_email_t *tm_provider_dropmail_click_get_emails(const char *token,
                                                  const char *email,
                                                  int *count);

/* Mailinator 姊妹 spam.* 子域名（复用 mailinator get_emails） */
tm_email_info_t *tm_provider_spam_coroiu_com_generate(void);
tm_email_t *tm_provider_spam_coroiu_com_get_emails(const char *email,
                                                   int *count);
tm_email_info_t *tm_provider_spam_deluser_net_generate(void);
tm_email_t *tm_provider_spam_deluser_net_get_emails(const char *email,
                                                    int *count);
tm_email_info_t *tm_provider_spam_dhsf_net_generate(void);
tm_email_t *tm_provider_spam_dhsf_net_get_emails(const char *email, int *count);
tm_email_info_t *tm_provider_spam_lucatnt_com_generate(void);
tm_email_t *tm_provider_spam_lucatnt_com_get_emails(const char *email,
                                                    int *count);
tm_email_info_t *tm_provider_spam_lyceum_life_com_ru_generate(void);
tm_email_t *tm_provider_spam_lyceum_life_com_ru_get_emails(const char *email,
                                                           int *count);
tm_email_info_t *tm_provider_spam_netpirates_net_generate(void);
tm_email_t *tm_provider_spam_netpirates_net_get_emails(const char *email,
                                                       int *count);
tm_email_info_t *tm_provider_spam_no_ip_net_generate(void);
tm_email_t *tm_provider_spam_no_ip_net_get_emails(const char *email,
                                                  int *count);
tm_email_info_t *tm_provider_spam_ozh_org_generate(void);
tm_email_t *tm_provider_spam_ozh_org_get_emails(const char *email, int *count);
tm_email_info_t *tm_provider_spam_pyphus_org_generate(void);
tm_email_t *tm_provider_spam_pyphus_org_get_emails(const char *email,
                                                   int *count);
tm_email_info_t *tm_provider_spam_shep_pw_generate(void);
tm_email_t *tm_provider_spam_shep_pw_get_emails(const char *email, int *count);
tm_email_info_t *tm_provider_spam_wtf_at_generate(void);
tm_email_t *tm_provider_spam_wtf_at_get_emails(const char *email, int *count);
tm_email_info_t *tm_provider_spam_wulczer_org_generate(void);
tm_email_t *tm_provider_spam_wulczer_org_get_emails(const char *email,
                                                    int *count);
tm_email_info_t *tm_provider_crap_kakadua_net_generate(void);
tm_email_t *tm_provider_crap_kakadua_net_get_emails(const char *email,
                                                    int *count);
tm_email_info_t *tm_provider_spam_janlugt_nl_generate(void);
tm_email_t *tm_provider_spam_janlugt_nl_get_emails(const char *email,
                                                   int *count);
tm_email_info_t *tm_provider_min_burningfish_net_generate(void);
tm_email_t *tm_provider_min_burningfish_net_get_emails(const char *email,
                                                       int *count);
tm_email_info_t *tm_provider_sink_fblay_com_generate(void);
tm_email_t *tm_provider_sink_fblay_com_get_emails(const char *email,
                                                  int *count);
tm_email_info_t *tm_provider_etgdev_de_generate(void);
tm_email_t *tm_provider_etgdev_de_get_emails(const char *email, int *count);
tm_email_info_t *tm_provider_mtmdev_com_generate(void);
tm_email_t *tm_provider_mtmdev_com_get_emails(const char *email, int *count);
tm_email_info_t *tm_provider_test_unergie_com_generate(void);
tm_email_t *tm_provider_test_unergie_com_get_emails(const char *email,
                                                    int *count);
tm_email_info_t *tm_provider_block_bdea_cc_generate(void);
tm_email_t *tm_provider_block_bdea_cc_get_emails(const char *email, int *count);
tm_email_info_t *tm_provider_torch_yi_org_generate(void);
tm_email_t *tm_provider_torch_yi_org_get_emails(const char *email, int *count);
tm_email_info_t *tm_provider_carlton183_changeip_net_generate(void);
tm_email_t *tm_provider_carlton183_changeip_net_get_emails(const char *email,
                                                           int *count);
tm_email_info_t *tm_provider_mail_fsmash_org_generate(void);
tm_email_t *tm_provider_mail_fsmash_org_get_emails(const char *email,
                                                   int *count);
tm_email_info_t *tm_provider_ebs_com_ar_generate(void);
tm_email_t *tm_provider_ebs_com_ar_get_emails(const char *email, int *count);
tm_email_info_t *tm_provider_jama_trenet_eu_generate(void);
tm_email_t *tm_provider_jama_trenet_eu_get_emails(const char *email,
                                                  int *count);
tm_email_info_t *tm_provider_blackhole_djurby_se_generate(void);
tm_email_t *tm_provider_blackhole_djurby_se_get_emails(const char *email,
                                                       int *count);
tm_email_info_t *tm_provider_m8r_davidfuhr_de_generate(void);
tm_email_t *tm_provider_m8r_davidfuhr_de_get_emails(const char *email,
                                                    int *count);
tm_email_info_t *tm_provider_mi_meon_be_generate(void);
tm_email_t *tm_provider_mi_meon_be_get_emails(const char *email, int *count);
tm_email_info_t *tm_provider_m_nik_me_generate(void);
tm_email_t *tm_provider_m_nik_me_get_emails(const char *email, int *count);
tm_email_info_t *tm_provider_mail_bentrask_com_generate(void);
tm_email_t *tm_provider_mail_bentrask_com_get_emails(const char *email,
                                                     int *count);
tm_email_info_t *tm_provider_t_zibet_net_generate(void);
tm_email_t *tm_provider_t_zibet_net_get_emails(const char *email, int *count);
tm_email_info_t *tm_provider_m8r_mcasal_com_generate(void);
tm_email_t *tm_provider_m8r_mcasal_com_get_emails(const char *email,
                                                  int *count);
tm_email_info_t *tm_provider_ramjane_mooo_com_generate(void);
tm_email_t *tm_provider_ramjane_mooo_com_get_emails(const char *email,
                                                    int *count);
tm_email_info_t *tm_provider_rauxa_seny_cat_generate(void);
tm_email_t *tm_provider_rauxa_seny_cat_get_emails(const char *email,
                                                  int *count);
tm_email_info_t *tm_provider_sp_woot_at_generate(void);
tm_email_t *tm_provider_sp_woot_at_get_emails(const char *email, int *count);
tm_email_info_t *tm_provider_fwd2m_eszett_es_generate(void);
tm_email_t *tm_provider_fwd2m_eszett_es_get_emails(const char *email,
                                                   int *count);
tm_email_info_t *tm_provider_m_887_at_generate(void);
tm_email_t *tm_provider_m_887_at_get_emails(const char *email, int *count);
tm_email_info_t *tm_provider_nospam_thurstons_us_generate(void);
tm_email_t *tm_provider_nospam_thurstons_us_get_emails(const char *email,
                                                       int *count);
tm_email_info_t *tm_provider_null_k3vin_net_generate(void);
tm_email_t *tm_provider_null_k3vin_net_get_emails(const char *email,
                                                  int *count);
tm_email_info_t *tm_provider_really_istrash_com_generate(void);
tm_email_t *tm_provider_really_istrash_com_get_emails(const char *email,
                                                      int *count);
tm_email_info_t *tm_provider_spam_hortuk_ovh_generate(void);
tm_email_t *tm_provider_spam_hortuk_ovh_get_emails(const char *email,
                                                   int *count);

tm_email_info_t *tm_provider_sendspamhere_com_generate(void);
tm_email_t *tm_provider_sendspamhere_com_get_emails(const char *email,
                                                    int *count);

tm_email_info_t *tm_provider_sendfree_org_generate(void);
tm_email_t *tm_provider_sendfree_org_get_emails(const char *email, int *count);

/* mailmomy 域名变体（复用 mailmomy 读信） */
tm_email_info_t *tm_provider_16888888_cyou_generate(void);
tm_email_t *tm_provider_16888888_cyou_get_emails(const char *email, int *count);
tm_email_info_t *tm_provider_17666688_shop_generate(void);
tm_email_t *tm_provider_17666688_shop_get_emails(const char *email, int *count);
tm_email_info_t *tm_provider_282mail_com_generate(void);
tm_email_t *tm_provider_282mail_com_get_emails(const char *email, int *count);
tm_email_info_t *tm_provider_bsdu32_buzz_generate(void);
tm_email_t *tm_provider_bsdu32_buzz_get_emails(const char *email, int *count);
tm_email_info_t *tm_provider_doxu243_buzz_generate(void);
tm_email_t *tm_provider_doxu243_buzz_get_emails(const char *email, int *count);
tm_email_info_t *tm_provider_easyme_pro_generate(void);
tm_email_t *tm_provider_easyme_pro_get_emails(const char *email, int *count);
tm_email_info_t *tm_provider_evergreenco_shop_generate(void);
tm_email_t *tm_provider_evergreenco_shop_get_emails(const char *email,
                                                    int *count);
tm_email_info_t *tm_provider_layueming_pics_generate(void);
tm_email_t *tm_provider_layueming_pics_get_emails(const char *email,
                                                  int *count);
tm_email_info_t *tm_provider_mingyuekeji_online_generate(void);
tm_email_t *tm_provider_mingyuekeji_online_get_emails(const char *email,
                                                      int *count);
tm_email_info_t *tm_provider_mingyueming_click_generate(void);
tm_email_t *tm_provider_mingyueming_click_get_emails(const char *email,
                                                     int *count);
tm_email_info_t *tm_provider_mingyueming_shop_generate(void);
tm_email_t *tm_provider_mingyueming_shop_get_emails(const char *email,
                                                    int *count);
tm_email_info_t *tm_provider_mingyukeji_lol_generate(void);
tm_email_t *tm_provider_mingyukeji_lol_get_emails(const char *email,
                                                  int *count);
tm_email_info_t *tm_provider_nuxh62_space_generate(void);
tm_email_t *tm_provider_nuxh62_space_get_emails(const char *email, int *count);
tm_email_info_t *tm_provider_proid_cloud_ip_cc_generate(void);
tm_email_t *tm_provider_proid_cloud_ip_cc_get_emails(const char *email,
                                                     int *count);
tm_email_info_t *tm_provider_sbook_pics_generate(void);
tm_email_t *tm_provider_sbook_pics_get_emails(const char *email, int *count);
tm_email_info_t *tm_provider_xue32_buzz_generate(void);
tm_email_t *tm_provider_xue32_buzz_get_emails(const char *email, int *count);
tm_email_info_t *tm_provider_xkx_me_generate(void);
tm_email_t *tm_provider_xkx_me_get_emails(const char *token, const char *email,
                                           int *count);

/* gonebox.email — 无需认证 */
tm_email_info_t *tm_provider_gonebox_email_generate(void);
tm_email_t *tm_provider_gonebox_email_get_emails(const char *email, int *count);

/* mailcat.ai — 需要 Bearer token */
tm_email_info_t *tm_provider_mailcat_ai_generate(void);
tm_email_t *tm_provider_mailcat_ai_get_emails(const char *token,
                                              const char *email, int *count);

/* mailinator 姊妹域名（复用 mailinator 读信） */
tm_email_info_t *tm_provider_b_smelly_cc_generate(void);
tm_email_t *tm_provider_b_smelly_cc_get_emails(const char *email, int *count);
tm_email_info_t *tm_provider_dea_soon_it_generate(void);
tm_email_t *tm_provider_dea_soon_it_get_emails(const char *email, int *count);
tm_email_info_t *tm_provider_disposable_al_sudani_com_generate(void);
tm_email_t *tm_provider_disposable_al_sudani_com_get_emails(const char *email,
                                                            int *count);
tm_email_info_t *tm_provider_disposable_nogonad_nl_generate(void);
tm_email_t *tm_provider_disposable_nogonad_nl_get_emails(const char *email,
                                                         int *count);
tm_email_info_t *tm_provider_j_fairuse_org_generate(void);
tm_email_t *tm_provider_j_fairuse_org_get_emails(const char *email, int *count);
tm_email_info_t *tm_provider_mn_curppa_com_generate(void);
tm_email_t *tm_provider_mn_curppa_com_get_emails(const char *email, int *count);
tm_email_info_t *tm_provider_mailinatorzz_mooo_com_generate(void);
tm_email_t *tm_provider_mailinatorzz_mooo_com_get_emails(const char *email,
                                                         int *count);
tm_email_info_t *tm_provider_notfond_404_mn_generate(void);
tm_email_t *tm_provider_notfond_404_mn_get_emails(const char *email,
                                                  int *count);

/* tempgo-email（https://tempgo.email，POST 创建/GET 读信，token 为 mailbox_id） */
tm_email_info_t *tm_provider_tempgo_email_generate(void);
tm_email_t *tm_provider_tempgo_email_get_emails(const char *token,
                                                const char *email, int *count);

/* restmail-net（https://restmail.net，ad-hoc 模式，无需 token） */
tm_email_info_t *tm_provider_restmail_net_generate(void);
tm_email_t *tm_provider_restmail_net_get_emails(const char *email, int *count);

/* dropmail-me（https://dropmail.me，GraphQL 创建会话，token 为 JSON） */
tm_email_info_t *tm_provider_dropmail_me_generate(void);
tm_email_t *tm_provider_dropmail_me_get_emails(const char *token,
                                               const char *email, int *count);

/* ten-minute-mail-net（https://10minutemail.net，Cookie 会话模式） */
tm_email_info_t *tm_provider_ten_minute_mail_net_generate(void);
tm_email_t *tm_provider_ten_minute_mail_net_get_emails(const char *token,
                                                       const char *email,
                                                       int *count);

#endif /* TEMPMAIL_INTERNAL_H */
