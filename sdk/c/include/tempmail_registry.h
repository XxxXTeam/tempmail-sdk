#ifndef TEMPMAIL_REGISTRY_H
#define TEMPMAIL_REGISTRY_H

#include "tempmail_sdk.h"

/*
 * 渠道注册表（内部）
 *
 * 复刻 Go 端 registry.go 的 ChannelSpec：每渠道一处注册，其余派生。
 * C 无闭包，故创建/收信各用一个固定签名的 thunk 包裹因渠道而异的 provider
 * 调用（固定域名、token 校验、参数顺序均在 thunk 内原样保留）。
 */

/* 创建邮箱的动态上下文（switch 分支中唯一用到的两个入参） */
typedef struct {
  int duration;       /* 有效期（分钟），部分渠道使用 */
  const char *domain; /* 接入/首选域名，可为 NULL */
} tm_gen_ctx_t;

/* 单个渠道注册规格（对应 Go 的 ChannelSpec） */
typedef struct {
  tm_channel_t channel; /* 渠道标识 */
  const char *name;     /* 渠道显示名称 */
  const char *website;  /* 对应服务网站 */
  /* 创建邮箱：成功返回 EmailInfo，失败返回 NULL */
  tm_email_info_t *(*generate)(const tm_gen_ctx_t *ctx);
  /* 获取邮件：*count>=0 表示成功（即使为空），-1 表示失败 */
  tm_email_t *(*get_emails)(const char *email, const char *token, int *count);
} tm_channel_spec_t;

/* 返回有序注册表（顺序即 baseline / listChannels 顺序），*count 为渠道数 */
const tm_channel_spec_t *tm_registry_all(int *count);

/* 注册表渠道数量 */
int tm_registry_count(void);

/* 按渠道标识查找规格，未注册返回 NULL */
const tm_channel_spec_t *tm_registry_find(tm_channel_t channel);

#endif /* TEMPMAIL_REGISTRY_H */
