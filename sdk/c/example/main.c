/**
 * 示例: 使用 C SDK 创建临时邮箱并获取邮件
 */

#include <stdio.h>
#include "tempmail_sdk.h"

int main(void) {
    /* 初始化 SDK */
    tm_init();
    tm_set_log_level(TM_LOG_INFO);

    /* 创建临时邮箱 */
    tm_generate_options_t gen_opts = {0};
    gen_opts.channel = CHANNEL_GUERRILLAMAIL;

    printf("正在创建临时邮箱...\n");
    tm_email_info_t *info = tm_generate_email(&gen_opts);
    if (!info) {
        fprintf(stderr, "创建邮箱失败\n");
        tm_cleanup();
        return 1;
    }

    printf("创建成功: %s\n", info->email);
    printf("Token: %s\n", info->token ? info->token : "(none)");

    /* 获取邮件 */
    tm_get_emails_options_t get_opts = {0};
    get_opts.channel = info->channel;
    get_opts.email = info->email;
    get_opts.token = info->token;

    tm_get_emails_result_t *result = tm_get_emails(&get_opts);
    if (result) {
        printf("获取邮件 success=%s count=%d\n",
            result->success ? "true" : "false",
            result->email_count);

        for (int i = 0; i < result->email_count; i++) {
            printf("  [%d] from=%s subject=%s\n",
                i, result->emails[i].from_addr, result->emails[i].subject);
        }
        tm_free_get_emails_result(result);
    }

    tm_free_email_info(info);
    tm_cleanup();
    return 0;
}
