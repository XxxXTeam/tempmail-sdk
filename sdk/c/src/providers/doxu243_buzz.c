/**
 * mailmomy 域名变体 doxu243.buzz
 * 固定域名，读信复用 mailmomy provider（token 即邮箱地址）
 */

#include "tempmail_internal.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CHANNEL_DOXU243_BUZZ_DOMAIN "doxu243.buzz"

static void doxu243_buzz_random_local(char *buf, int len) {
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

tm_email_info_t *tm_provider_doxu243_buzz_generate(void) {
  char local[11];
  doxu243_buzz_random_local(local, 10);

  tm_email_info_t *info = tm_email_info_new();
  if (!info) {
    return NULL;
  }
  info->channel = CHANNEL_DOXU243_BUZZ;
  {
    size_t cap = strlen(local) + strlen("@" CHANNEL_DOXU243_BUZZ_DOMAIN) + 1;
    info->email = (char *)malloc(cap);
    if (!info->email) {
      tm_free_email_info(info);
      return NULL;
    }
    snprintf(info->email, cap, "%s@%s", local, CHANNEL_DOXU243_BUZZ_DOMAIN);
  }
  /* token 即邮箱地址 */
  info->token = tm_strdup(info->email);
  if (!info->token) {
    tm_free_email_info(info);
    return NULL;
  }
  info->created_at = tm_strdup("");
  return info;
}

tm_email_t *tm_provider_doxu243_buzz_get_emails(const char *email, int *count) {
  /* 复用 mailmomy 读信逻辑，token 即邮箱地址 */
  return tm_provider_mailmomy_get_emails(email, email, count);
}
