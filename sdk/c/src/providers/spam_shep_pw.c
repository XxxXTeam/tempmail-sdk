/**
 * Mailinator 官方姊妹域名 spam.shep.pw
 * MX 指向 mail.mailinator.com，复用 mailinator provider 的读信逻辑
 */

#include "tempmail_internal.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SPAM_SHEP_PW_DOMAIN "spam.shep.pw"

static void spam_shep_pw_random_local(char *buf, int len) {
  static int seeded = 0;
  if (!seeded) {
    srand((unsigned)time(NULL) ^ (unsigned)clock());
    seeded = 1;
  }
  const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
  for (int i = 0; i < len; i++) {
    buf[i] = chars[rand() % (int)(sizeof(chars) - 1)];
  }
  buf[len] = '\0';
}

tm_email_info_t *tm_provider_spam_shep_pw_generate(void) {
  char local[13];
  spam_shep_pw_random_local(local, 12);

  tm_email_info_t *info = tm_email_info_new();
  if (!info) {
    return NULL;
  }
  info->channel = CHANNEL_SPAM_SHEP_PW;
  {
    size_t cap = strlen(local) + strlen("@" SPAM_SHEP_PW_DOMAIN) + 1;
    info->email = (char *)malloc(cap);
    if (!info->email) {
      tm_free_email_info(info);
      return NULL;
    }
    snprintf(info->email, cap, "%s@%s", local, SPAM_SHEP_PW_DOMAIN);
  }
  return info;
}

tm_email_t *tm_provider_spam_shep_pw_get_emails(const char *email, int *count) {
  return tm_provider_mailinator_get_emails(email, count);
}
