/**
 * SDK 主入口
 */

#include "tempmail_internal.h"
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#define tm_sleep_ms(ms) Sleep(ms)
#else
#include <unistd.h>
#define tm_sleep_ms(ms) usleep((ms) * 1000)
#endif

/* 公共渠道列表顺序与 Go SDK allChannels / listChannels
 * 一致；随机尝试顺序会在本端独立洗牌，不要求跨 SDK 一致。
 * 顺序与信息均从 registry.c 的有序注册表派生，此处不再维护平行结构。 */

#define TM_CHANNEL_TRY_N (tm_registry_count())

static void tm_seed_random_once(void) {
  static int seeded = 0;
  if (!seeded) {
    srand((unsigned int)time(NULL) ^ (unsigned int)clock());
    seeded = 1;
  }
}

/*
 * tm_list_channels 返回全部渠道显示信息
 * 顺序与信息均从 registry.c 的有序注册表派生（惰性初始化缓存）
 */
const tm_channel_info_t *tm_list_channels(int *count) {
  static tm_channel_info_t *cache = NULL;
  static int cache_count = 0;

  if (!cache) {
    int n = 0;
    const tm_channel_spec_t *specs = tm_registry_all(&n);
    tm_channel_info_t *buf =
        (tm_channel_info_t *)malloc(sizeof(tm_channel_info_t) * (size_t)n);
    if (buf) {
      for (int i = 0; i < n; i++) {
        buf[i].channel = specs[i].channel;
        buf[i].name = specs[i].name;
        buf[i].website = specs[i].website;
      }
      cache = buf;
      cache_count = n;
    } else if (count) {
      /* 分配失败时退化为仅返回数量 */
      *count = n;
      return NULL;
    }
  }

  if (count)
    *count = cache_count;
  return cache;
}

/* 设置 EmailInfo 的渠道标识并原样返回（非 static，供 registry.c 的 thunk 调用） */
tm_email_info_t *tm_with_channel(tm_email_info_t *info, tm_channel_t channel) {
  if (info)
    info->channel = channel;
  return info;
}

/*
 * tm_try_generate 在指定渠道上尝试创建邮箱（含重试）
 * 成功返回 EmailInfo 指针，失败返回 NULL
 */
static tm_email_info_t *tm_try_generate(tm_channel_t channel, int duration,
                                        const char *domain,
                                        const tm_retry_config_t *retry_cfg,
                                        int *out_attempts) {
  tm_retry_config_t cfg = retry_cfg ? *retry_cfg : tm_default_retry_config();
  tm_email_info_t *result = NULL;

  /* 查注册表获取该渠道的创建 thunk（未注册或未实现直接失败） */
  const tm_channel_spec_t *spec = tm_registry_find(channel);
  if (!spec || !spec->generate) {
    if (out_attempts)
      *out_attempts = 0;
    return NULL;
  }

  for (int attempt = 0; attempt <= cfg.max_retries; attempt++) {
    tm_gen_ctx_t ctx = {duration, domain};
    result = spec->generate(&ctx);
    if (result) {
      if (out_attempts)
        *out_attempts = attempt + 1;
      return result;
    }

    if (attempt < cfg.max_retries) {
      int delay = cfg.initial_delay_ms * (1 << attempt);
      if (delay > cfg.max_delay_ms)
        delay = cfg.max_delay_ms;
      tm_sleep_ms(delay);
    }
  }
  if (out_attempts)
    *out_attempts = cfg.max_retries + 1;
  return NULL;
}

/*
 * tm_build_channel_order 构建渠道尝试顺序（Fisher-Yates 洗牌）
 * 指定渠道时优先尝试该渠道，其余渠道打乱追加
 * 未指定（CHANNEL_RANDOM）时打乱全部渠道
 * 返回值是本端运行时随机顺序，不作为跨 SDK 一致性约束
 * 调用者需 free 返回的数组
 */
static tm_channel_t *tm_build_channel_order(tm_channel_t preferred,
                                            int *out_count) {
  *out_count = TM_CHANNEL_TRY_N;
  tm_channel_t *order =
      (tm_channel_t *)malloc(sizeof(tm_channel_t) * (size_t)TM_CHANNEL_TRY_N);
  if (!order)
    return NULL;

  /* 初始顺序取自有序注册表（即 baseline / listChannels 顺序） */
  int reg_count = 0;
  const tm_channel_spec_t *specs = tm_registry_all(&reg_count);
  (void)reg_count;
  for (int i = 0; i < TM_CHANNEL_TRY_N; i++)
    order[i] = specs[i].channel;

  for (int i = TM_CHANNEL_TRY_N - 1; i > 0; i--) {
    int j = rand() % (i + 1);
    tm_channel_t tmp = order[i];
    order[i] = order[j];
    order[j] = tmp;
  }

  if (preferred != CHANNEL_RANDOM && preferred >= 0 &&
      preferred < CHANNEL_COUNT) {
    for (int i = 0; i < TM_CHANNEL_TRY_N; i++) {
      if (order[i] == preferred) {
        tm_channel_t tmp = order[0];
        order[0] = order[i];
        order[i] = tmp;
        break;
      }
    }
  }
  return order;
}

/*
 * tm_generate_email 创建临时邮箱
 *
 * 错误处理策略:
 * - 指定渠道失败时，自动尝试其他可用渠道（打乱顺序逐个尝试）
 * - 未指定渠道时，打乱全部渠道逐个尝试，直到成功
 * - 所有渠道均不可用时返回 NULL（不崩溃）
 */
tm_email_info_t *tm_generate_email(const tm_generate_options_t *options) {
  tm_channel_t preferred = options ? options->channel : CHANNEL_RANDOM;
  int duration = (options && options->duration > 0) ? options->duration : 30;
  const char *domain = options ? options->domain : NULL;
  const tm_retry_config_t *retry_cfg = options ? options->retry : NULL;

  tm_seed_random_once();

  int count = 0;
  tm_channel_t *try_order = tm_build_channel_order(preferred, &count);
  if (!try_order)
    return NULL;

  int channels_tried = 0;
  for (int i = 0; i < count; i++) {
    channels_tried++;
    tm_channel_t ch = try_order[i];
    TM_LOG_INF("创建临时邮箱, 渠道: %s", tm_channel_name(ch));

    int attempts = 0;
    tm_email_info_t *result =
        tm_try_generate(ch, duration, domain, retry_cfg, &attempts);
    if (result) {
      TM_LOG_INF("邮箱创建成功: %s (渠道: %s)", result->email,
                 tm_channel_name(ch));
      tm_telemetry_report("generate_email", tm_channel_name(ch), true, attempts,
                          channels_tried, NULL);
      free(try_order);
      return result;
    }

    TM_LOG_WARN("渠道 %s 不可用，尝试下一个渠道", tm_channel_name(ch));
  }

  free(try_order);
  TM_LOG_ERR("所有渠道均不可用，创建邮箱失败");
  tm_telemetry_report("generate_email", "", false, 0, channels_tried,
                      "all channels failed");
  return NULL;
}

tm_get_emails_result_t *tm_get_emails(const tm_email_info_t *email_info,
                                      const tm_get_emails_options_t *options) {
  if (!email_info) {
    tm_telemetry_report("get_emails", "", false, 0, 0, "email_info is NULL");
    return NULL;
  }

  tm_get_emails_result_t *result =
      (tm_get_emails_result_t *)calloc(1, sizeof(tm_get_emails_result_t));
  if (!result)
    return NULL;

  result->channel = email_info->channel;
  result->email = tm_strdup(email_info->email);

  TM_LOG_DBG("获取邮件, 渠道: %s, 邮箱: %s",
             tm_channel_name(email_info->channel), email_info->email);

  tm_retry_config_t cfg =
      (options && options->retry) ? *options->retry : tm_default_retry_config();
  int count = 0;
  tm_email_t *emails = NULL;

  /* 查注册表获取该渠道的收信 thunk（token 校验/参数在 thunk 内处理） */
  const tm_channel_spec_t *spec = tm_registry_find(email_info->channel);

  for (int attempt = 0; attempt <= cfg.max_retries; attempt++) {
    if (spec && spec->get_emails) {
      emails =
          spec->get_emails(email_info->email, email_info->token, &count);
    } else {
      count = -1;
    }

    /* count >= 0 表示请求成功（即使邮件为空） */
    if (count >= 0 && (emails || count == 0)) {
      if (attempt > 0)
        TM_LOG_INF("第 %d 次尝试成功", attempt + 1);
      result->emails = emails;
      result->email_count = count;
      result->success = true;
      if (count > 0) {
        TM_LOG_INF("获取到 %d 封邮件, 渠道: %s", count,
                   tm_channel_name(email_info->channel));
      } else {
        TM_LOG_DBG("暂无邮件, 渠道: %s", tm_channel_name(email_info->channel));
      }
      tm_telemetry_report("get_emails", tm_channel_name(email_info->channel),
                          true, attempt + 1, 0, NULL);
      return result;
    }

    if (attempt < cfg.max_retries) {
      int delay = cfg.initial_delay_ms * (1 << attempt);
      if (delay > cfg.max_delay_ms)
        delay = cfg.max_delay_ms;
      TM_LOG_WARN("请求失败，%dms 后第 %d 次重试...", delay, attempt + 2);
      tm_sleep_ms(delay);
    }
  }

  TM_LOG_ERR("获取邮件失败, 渠道: %s", tm_channel_name(email_info->channel));
  result->success = false;
  result->error = tm_strdup("重试耗尽后仍失败");
  tm_telemetry_report("get_emails", tm_channel_name(email_info->channel), false,
                      cfg.max_retries + 1, 0, result->error);
  return result;
}
