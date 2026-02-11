/**
 * 通用重试工具
 */

#include "tempmail_internal.h"

#ifdef _WIN32
#include <windows.h>
#define tm_sleep_ms(ms) Sleep(ms)
#else
#include <unistd.h>
#define tm_sleep_ms(ms) usleep((ms) * 1000)
#endif

static int should_retry(const char *err) {
    if (!err) return 0;
    const char *keywords[] = {
        "connection", "timeout", "timed out", "dns",
        "eof", "broken pipe", "refused", "reset",
        "ssl", "network", ": 4", ": 5",
    };
    /* 简单的大小写无关子串匹配 */
    for (int i = 0; i < (int)(sizeof(keywords)/sizeof(keywords[0])); i++) {
        if (strstr(err, keywords[i])) return 1;
    }
    return 0;
}

tm_email_info_t* tm_retry_generate(
    tm_email_info_t* (*fn)(void),
    const tm_retry_config_t *cfg
) {
    tm_retry_config_t c = cfg ? *cfg : tm_default_retry_config();

    for (int attempt = 0; attempt <= c.max_retries; attempt++) {
        tm_email_info_t *result = fn();
        if (result) {
            if (attempt > 0) {
                TM_LOG_INF("第 %d 次尝试成功", attempt + 1);
            }
            return result;
        }
        if (attempt >= c.max_retries) {
            TM_LOG_ERR("重试 %d 次后仍失败", c.max_retries);
            return NULL;
        }

        int delay = c.initial_delay_ms * (1 << attempt);
        if (delay > c.max_delay_ms) delay = c.max_delay_ms;
        TM_LOG_WARN("请求失败，%dms 后第 %d 次重试...", delay, attempt + 2);
        tm_sleep_ms(delay);
    }
    return NULL;
}
