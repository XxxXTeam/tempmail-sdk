/**
 * 临时邮箱 SDK (C)
 */

#ifndef TEMPMAIL_SDK_H
#define TEMPMAIL_SDK_H

#include <stdbool.h>
#include <stddef.h>

#ifndef TEMPMAIL_SDK_VERSION
#define TEMPMAIL_SDK_VERSION "0.0.0"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ========== 渠道枚举 ========== */

typedef enum {
  CHANNEL_TEMPMAIL = 0,
  CHANNEL_LINSHIYOU,
  CHANNEL_TEMPMAIL_LOL,
  CHANNEL_CHATGPT_ORG_UK,
  CHANNEL_AWAMAIL,
  CHANNEL_MAIL_TM,
  CHANNEL_DROPMAIL,
  CHANNEL_GUERRILLAMAIL,
  CHANNEL_MAILDROP,
  CHANNEL_SMAIL_PW,
  CHANNEL_CATCHMAIL_MAILISTRY,
  CHANNEL_CATCHMAIL_ZEPPOST,
  CHANNEL_VIP_215,
  CHANNEL_MAILFORSPAM_TEMPMAIL_IO,
  CHANNEL_FAKE_LEGAL,
  CHANNEL_MFFAC,
  CHANNEL_TEMPMAIL_CN,
  CHANNEL_TA_EASY,
  CHANNEL_MAILFORSPAM_DISPOSABLE,
  CHANNEL_MOAKT,
  CHANNEL_10MINUTE_ONE,
  CHANNEL_GUERRILLAMAIL_COM,
  CHANNEL_SHARKLASERS_COM,
  CHANNEL_EMAIL10MIN,
  CHANNEL_MJJ_CM,
  CHANNEL_GRR_LA_COM,
  CHANNEL_LINSHI_CO,
  CHANNEL_HARAKIRIMAIL,
  CHANNEL_TEMPMAIL_PLUS,
  CHANNEL_TEMPMAIL_LOL_V2,
  CHANNEL_SHARKLASERS,
  CHANNEL_GRR_LA,
  CHANNEL_GUERRILLAMAIL_INFO,
  CHANNEL_TEMP_MAIL_IO,
  CHANNEL_MAIL_CX,
  CHANNEL_CATCHMAIL,
  CHANNEL_MAILFORSPAM,
  CHANNEL_TEMPMAILC,
  CHANNEL_MAILNESIA,
  CHANNEL_THROWAWAYMAIL,
  CHANNEL_INBOXKITTEN,
  CHANNEL_GETNADA,
  CHANNEL_MAIL123,
  CHANNEL_SPAM4ME,
  CHANNEL_GUERRILLAMAIL_NET,
  CHANNEL_GUERRILLAMAIL_ORG,
  CHANNEL_GUERRILLAMAILBLOCK,
  CHANNEL_GUERRILLAMAIL_COM_WWW,
  CHANNEL_ONE_SEC_MAIL,
  CHANNEL_FAKEMAIL,
  CHANNEL_OPENINBOX,
  CHANNEL_INBOXES,
  CHANNEL_UNCORREOTEMPORAL,
  CHANNEL_JQKJQK_XYZ,
  CHANNEL_IMGUI_DE,
  CHANNEL_PULSEWEBMENU_DE,
  CHANNEL_XGHFF_COM,
  CHANNEL_OQQAJ_COM,
  CHANNEL_PSOVV_COM,
  CHANNEL_DBWOT_COM,
  CHANNEL_YGWPR_COM,
  CHANNEL_IMXWE_COM,
  CHANNEL_DDKER_COM,
  CHANNEL_WEB_LIBRARY_NET,
  CHANNEL_ONE_VPN_NET,
  CHANNEL_ABEMATV_COM,
  CHANNEL_ABEMATV_NET,
  CHANNEL_ABEMATV_ORG,
  CHANNEL_ACEH_CC,
  CHANNEL_BANGKABELITUNG_NET,
  CHANNEL_CCTRUYEN_COM,
  CHANNEL_GETNADA_COM,
  CHANNEL_GETNADA_EMAIL,
  CHANNEL_GETNADA_NET,
  CHANNEL_JAWATENGAH_NET,
  CHANNEL_JAWATIMUR_NET,
  CHANNEL_KALIMANTANBARAT_NET,
  CHANNEL_KALIMANTANSELATAN_NET,
  CHANNEL_KALIMANTANTENGAH_NET,
  CHANNEL_KALIMANTANTIMUR_NET,
  CHANNEL_KALIMANTANUTARA_NET,
  CHANNEL_KEPULAUANRIAU_NET,
  CHANNEL_LUXURY345_COM,
  CHANNEL_MALUKUUTARA_NET,
  CHANNEL_NUSATENGGARABARAT_NET,
  CHANNEL_NUSATENGGARATIMUR_NET,
  CHANNEL_PAPUABARAT_NET,
  CHANNEL_PAPUABARATDAYA_NET,
  CHANNEL_PAPUASELATAN_NET,
  CHANNEL_PEHOL_COM,
  CHANNEL_PTRUYEN_COM,
  CHANNEL_PULAUBALI_NET,
  CHANNEL_RIAU_NET,
  CHANNEL_SEOKEY_ORG,
  CHANNEL_SULAWESIBARAT_NET,
  CHANNEL_SULAWESISELATAN_NET,
  CHANNEL_SULAWESITENGAH_NET,
  CHANNEL_SULAWESITENGGARA_NET,
  CHANNEL_SUMATERABARAT_NET,
  CHANNEL_SUMATERASELATAN_NET,
  CHANNEL_SUMATERAUTARA_NET,
  CHANNEL_VILLATOGEL_COM,
  CHANNEL_DRMAIL_IN,
  CHANNEL_TEML_NET,
  CHANNEL_TMPEML_COM,
  CHANNEL_TMPBOX_NET,
  CHANNEL_MOAKT_CC,
  CHANNEL_DISBOX_NET,
  CHANNEL_TMPMAIL_ORG,
  CHANNEL_TMPMAIL_NET,
  CHANNEL_TMAILS_NET,
  CHANNEL_DISBOX_ORG,
  CHANNEL_MOAKT_CO,
  CHANNEL_MOAKT_WS,
  CHANNEL_TMAIL_WS,
  CHANNEL_BAREED_WS,
  CHANNEL_FEXPOST_COM,
  CHANNEL_FEXBOX_ORG,
  CHANNEL_MAILBOX_IN_UA,
  CHANNEL_ROVER_INFO,
  CHANNEL_CHITTHI_IN,
  CHANNEL_FEXTEMP_COM,
  CHANNEL_ANY_PINK,
  CHANNEL_MEREPOST_COM,
  CHANNEL_TEMPGBOX,
  CHANNEL_EMAILNATOR,
  CHANNEL_TEMPORAM,
  CHANNEL_LYHLEVI_COM,
  CHANNEL_MAIL10S,
  CHANNEL_WEBMAILTEMP,
  CHANNEL_TEMPFASTMAIL,
  CHANNEL_SHITTY_EMAIL,
  CHANNEL_TEMPMAILPRO,
  CHANNEL_NEIGHBOURS,
  CHANNEL_CLEANTEMPMAIL,
  CHANNEL_M2U,
  CHANNEL_TEMPY_EMAIL,
  CHANNEL_FMAIL,
  CHANNEL_OCKITO,
  CHANNEL_ANONBOX,
  CHANNEL_DUCKMAIL,
  CHANNEL_MAILINATOR,
  CHANNEL_DEVMAIL_UK,
  CHANNEL_TEMPMAIL365,
  CHANNEL_TEMPINBOX,
  CHANNEL_BYOM,
  CHANNEL_ANONYMMAIL,
  CHANNEL_EYEPASTE,
  CHANNEL_MAIL_SUNLS,
  CHANNEL_EXPRESSINBOXHUB,
  CHANNEL_LROID,
  CHANNEL_HARIBU,
  CHANNEL_ROOTSH,
  CHANNEL_FAKE_EMAIL_SITE,
  CHANNEL_MOHMAL,
  CHANNEL_MAILGOLEM,
  CHANNEL_BEST_TEMP_MAIL,
  CHANNEL_DISPOSABLEMAIL_APP,
  CHANNEL_MAILTEMP_CC,
  CHANNEL_MINUTEINBOX,
  CHANNEL_MAILCATCH,
  CHANNEL_TEMPEMAIL_CO,
  CHANNEL_TEMPEMAILS_NET,
  CHANNEL_ALTMAILS,
  CHANNEL_TEMPEMAIL_INFO,
  CHANNEL_SMAILPRO,
  CHANNEL_TEMPMAILTEN,
  CHANNEL_MAILDROP_CC,
  CHANNEL_TENMINUTEMAIL_NET,
  CHANNEL_LINSHIYOUXIANG_NET,
  CHANNEL_TEMPMAIL_FYI,
  CHANNEL_DISPOSABLEMAIL_COM,
  CHANNEL_TEMPP_MAILS,
  CHANNEL_EMAILTEMP_ORG,
  CHANNEL_MYTEMPMAIL_CC,
  CHANNEL_TEMP_MAIL_NOW,
  CHANNEL_MAIL_TD,
  CHANNEL_MAILHOLE_DE,
  CHANNEL_TMAIL_LINK,
  CHANNEL_24MAIL_CHACUO,
  CHANNEL_NIMAIL,
  CHANNEL_FREECUSTOM,
  CHANNEL_APIHZ,
  CHANNEL_SOGETTHIS_COM,
  CHANNEL_BOBMAIL_INFO,
  CHANNEL_SUREMAIL_INFO,
  CHANNEL_BINKMAIL_COM,
  CHANNEL_VERYREALEMAIL_COM,
  CHANNEL_MAILMOMY,
  CHANNEL_CHAMMY_INFO,
  CHANNEL_THISISNOTMYREALEMAIL_COM,
  CHANNEL_NOTMAILINATOR_COM,
  CHANNEL_SPAMHEREPLEASE_COM,
  CHANNEL_SENDSPAMHERE_COM,
  CHANNEL_SENDFREE_ORG,
  CHANNEL_JUNK_BEATS_ORG,
  CHANNEL_JUNK_IHMEHL_COM,
  CHANNEL_JUNK_NOPLAY_ORG,
  CHANNEL_JUNK_VANILLASYSTEM_COM,
  CHANNEL_SPAM_JASONPEARCE_COM,
  CHANNEL_FISH_SKYTALE_NET,
  CHANNEL_SPAM_MCCREW_COM,
  CHANNEL_DROPMAIL_CLICK,
  CHANNEL_SPAM_COROIU_COM,
  CHANNEL_SPAM_DELUSER_NET,
  CHANNEL_SPAM_DHSF_NET,
  CHANNEL_SPAM_LUCATNT_COM,
  CHANNEL_SPAM_LYCEUM_LIFE_COM_RU,
  CHANNEL_SPAM_NETPIRATES_NET,
  CHANNEL_SPAM_NO_IP_NET,
  CHANNEL_SPAM_OZH_ORG,
  CHANNEL_SPAM_PYPHUS_ORG,
  CHANNEL_SPAM_SHEP_PW,
  CHANNEL_SPAM_WTF_AT,
  CHANNEL_SPAM_WULCZER_ORG,
  CHANNEL_CRAP_KAKADUA_NET,
  CHANNEL_SPAM_JANLUGT_NL,
  CHANNEL_MIN_BURNINGFISH_NET,
  CHANNEL_SINK_FBLAY_COM,
  CHANNEL_ETGDEV_DE,
  CHANNEL_MTMDEV_COM,
  CHANNEL_TEST_UNERGIE_COM,
  CHANNEL_BLOCK_BDEA_CC,
  CHANNEL_TORCH_YI_ORG,
  CHANNEL_CARLTON183_CHANGEIP_NET,
  CHANNEL_MAIL_FSMASH_ORG,
  CHANNEL_EBS_COM_AR,
  CHANNEL_JAMA_TRENET_EU,
  CHANNEL_BLACKHOLE_DJURBY_SE,
  CHANNEL_M8R_DAVIDFUHR_DE,
  CHANNEL_MI_MEON_BE,
  CHANNEL_M_NIK_ME,
  CHANNEL_MAIL_BENTRASK_COM,
  CHANNEL_T_ZIBET_NET,
  CHANNEL_M8R_MCASAL_COM,
  CHANNEL_RAMJANE_MOOO_COM,
  CHANNEL_RAUXA_SENY_CAT,
  CHANNEL_SP_WOOT_AT,
  CHANNEL_FWD2M_ESZETT_ES,
  CHANNEL_M_887_AT,
  CHANNEL_NOSPAM_THURSTONS_US,
  CHANNEL_NULL_K3VIN_NET,
  CHANNEL_REALLY_ISTRASH_COM,
  CHANNEL_SPAM_HORTUK_OVH,
  CHANNEL_16888888_CYOU,
  CHANNEL_17666688_SHOP,
  CHANNEL_282MAIL_COM,
  CHANNEL_BSDU32_BUZZ,
  CHANNEL_DOXU243_BUZZ,
  CHANNEL_EASYME_PRO,
  CHANNEL_EVERGREENCO_SHOP,
  CHANNEL_LAYUEMING_PICS,
  CHANNEL_MINGYUEKEJI_ONLINE,
  CHANNEL_MINGYUEMING_CLICK,
  CHANNEL_MINGYUEMING_SHOP,
  CHANNEL_MINGYUKEJI_LOL,
  CHANNEL_NUXH62_SPACE,
  CHANNEL_PROID_CLOUD_IP_CC,
  CHANNEL_SBOOK_PICS,
  CHANNEL_XUE32_BUZZ,
  CHANNEL_B_SMELLY_CC,
  CHANNEL_DEA_SOON_IT,
  CHANNEL_DISPOSABLE_AL_SUDANI_COM,
  CHANNEL_DISPOSABLE_NOGONAD_NL,
  CHANNEL_J_FAIRUSE_ORG,
  CHANNEL_MN_CURPPA_COM,
  CHANNEL_MAILINATORZZ_MOOO_COM,
  CHANNEL_NOTFOND_404_MN,
  CHANNEL_COUNT,       /* 渠道总数 */
  CHANNEL_RANDOM = -1, /* 随机选择 */
} tm_channel_t;
/* ========== 日志级别 ========== */

typedef enum {
  TM_LOG_SILENT = 0,
  TM_LOG_ERROR = 1,
  TM_LOG_WARN = 2,
  TM_LOG_INFO = 3,
  TM_LOG_DEBUG = 4,
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
  char *id;                     /* 邮件唯一标识 */
  char *from_addr;              /* 发件人地址 */
  char *to;                     /* 收件人地址 */
  char *subject;                /* 主题 */
  char *text;                   /* 纯文本内容 */
  char *html;                   /* HTML 内容 */
  char *date;                   /* 日期（ISO 8601） */
  bool is_read;                 /* 是否已读 */
  tm_attachment_t *attachments; /* 附件数组 */
  int attachment_count;         /* 附件数量 */
} tm_email_t;

/* 邮箱信息（创建后返回） */
typedef struct {
  tm_channel_t channel; /* 渠道 */
  char *email;          /* 邮箱地址 */
  char *token;          /* 认证令牌（SDK 内部维护，用户不应直接访问） */
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
  int max_retries;      /* 最大重试次数，默认 2 */
  int initial_delay_ms; /* 初始延迟毫秒，默认 1000 */
  int max_delay_ms;     /* 最大延迟毫秒，默认 5000 */
  int timeout_secs;     /* 请求超时秒，默认 15 */
} tm_retry_config_t;

/* 创建邮箱选项 */
typedef struct {
  tm_channel_t channel; /* 渠道，CHANNEL_RANDOM 随机 */
  int duration;         /* tempmail 有效期（分钟） */
  const char *domain;   /* tempmail-cn 接入域名，或 tempmail-lol 等渠道域名 */
  tm_retry_config_t *retry; /* 重试配置，NULL 使用默认 */
} tm_generate_options_t;

/* 获取邮件选项（Channel/Email/Token 由 SDK 从 tm_email_info_t 中自动获取） */
typedef struct {
  tm_retry_config_t *retry; /* 重试配置，NULL 使用默认 */
} tm_get_emails_options_t;

/* 渠道信息 */
typedef struct {
  tm_channel_t channel;
  const char *name;
  const char *website;
} tm_channel_info_t;

/* ========== SDK 全局配置 ========== */

/**
 * SDK 全局配置
 * 包含代理、超时、SSL 等设置，作用于所有 HTTP 请求
 */
typedef struct {
  const char *proxy;      /* 代理 URL，支持 http/https/socks5，如
                             "http://127.0.0.1:7890"，NULL 不使用代理 */
  int timeout_secs;       /* 全局默认超时秒数，0 使用默认值 15 */
  bool insecure;          /* 跳过 SSL 证书验证（调试用） */
  bool telemetry_enabled; /* true（默认）发送匿名用量上报；false 关闭 */
  const char *
      telemetry_url; /* 非 NULL 时作为上报 URL；NULL 则使用环境变量或内置默认 */
} tm_config_t;

/**
 * 默认 SDK 配置（含 telemetry_enabled=true，其余与零初始化一致）
 */
tm_config_t tm_default_config(void);

/**
 * 设置 SDK 全局配置
 * 内部复制配置内容，调用后原始指针可释放
 */
void tm_set_config(const tm_config_t *config);

/**
 * 获取当前 SDK 全局配置
 */
const tm_config_t *tm_get_config(void);

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
 * 返回全部渠道信息（*count 与 CHANNEL_COUNT 一致，顺序与 Go SDK allChannels
 * 相同） 随机生成邮箱时会在 C SDK 内独立打乱尝试顺序，不要求跨 SDK 一致
 */
const tm_channel_info_t *tm_list_channels(int *count);

/**
 * 获取渠道名称字符串
 */
const char *tm_channel_name(tm_channel_t channel);

/**
 * 创建临时邮箱
 * 成功返回 EmailInfo 指针，失败返回 NULL
 * 调用者需使用 tm_free_email_info() 释放
 */
tm_email_info_t *tm_generate_email(const tm_generate_options_t *options);

/**
 * 获取邮件列表
 * Channel/Email/Token 等由 SDK 从 email_info 中自动获取
 * 始终返回结果（即使失败也返回 success=false 的结果）
 * 调用者需使用 tm_free_get_emails_result() 释放
 *
 * @param email_info  tm_generate_email() 返回的邮箱信息
 * @param options     可选配置，NULL 使用默认值
 */
tm_get_emails_result_t *tm_get_emails(const tm_email_info_t *email_info,
                                      const tm_get_emails_options_t *options);

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
