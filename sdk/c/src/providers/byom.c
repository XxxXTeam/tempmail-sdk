/**
 * Byom — https://byom.de
 * 最简单的 provider，纯 GET 无认证
 * 域名固定 byom.de，创建邮箱无需 API 调用
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BYOM_API "https://api.byom.de"

static const char *byom_headers[] = {
    "Accept: application/json, text/plain, */*",
    "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    NULL};

/* 随机生成 10 位小写字母数字用户名 */
static void byom_random_username(char *buf, size_t cap) {
  static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
  size_t len = 10;
  if (cap < len + 1)
    len = cap - 1;
  for (size_t i = 0; i < len; i++) {
    buf[i] = chars[rand() % (sizeof(chars) - 1)];
  }
  buf[len] = '\0';
}

tm_email_info_t *tm_provider_byom_generate(void) {
  srand((unsigned)time(NULL));

  char username[16];
  byom_random_username(username, sizeof(username));

  char email[128];
  snprintf(email, sizeof(email), "%s@byom.de", username);

  tm_email_info_t *info = tm_email_info_new();
  info->channel = CHANNEL_BYOM;
  info->email = tm_strdup(email);
  info->token = NULL;
  return info;
}

tm_email_t *tm_provider_byom_get_emails(const char *email, int *count) {
  *count = -1;
  if (!email || !email[0])
    return NULL;

  /* 从邮箱地址提取用户名 */
  char username[256];
  const char *at = strchr(email, '@');
  if (at && at > email) {
    size_t len = (size_t)(at - email);
    if (len >= sizeof(username))
      len = sizeof(username) - 1;
    memcpy(username, email, len);
    username[len] = '\0';
  } else {
    strncpy(username, email, sizeof(username) - 1);
    username[sizeof(username) - 1] = '\0';
  }

  char url[512];
  snprintf(url, sizeof(url), "%s/mails/%s", BYOM_API, username);

  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, byom_headers, NULL, 15);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }

  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!root)
    return NULL;

  /* 响应为 JSON 数组 */
  if (!cJSON_IsArray(root)) {
    *count = 0;
    cJSON_Delete(root);
    return NULL;
  }

  int n = cJSON_GetArraySize(root);
  *count = n;
  if (n == 0) {
    cJSON_Delete(root);
    return NULL;
  }

  tm_email_t *emails = tm_emails_new(n);
  if (!emails) {
    cJSON_Delete(root);
    *count = -1;
    return NULL;
  }

  for (int i = 0; i < n; i++) {
    cJSON *raw = cJSON_GetArrayItem(root, i);
    /* 字段映射: content → html, text → text, created_at → date */
    const char *content =
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(raw, "content"), "");
    const char *text =
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(raw, "text"), "");
    const char *created =
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(raw, "created_at"), "");
    if (content[0])
      cJSON_AddStringToObject(raw, "html", content);
    if (text[0] && !cJSON_GetObjectItemCaseSensitive(raw, "text")) {
      /* text 字段已存在，normalize 会直接取 */
    }
    if (created[0])
      cJSON_AddStringToObject(raw, "date", created);
    emails[i] = tm_normalize_email(raw, email);
  }

  cJSON_Delete(root);
  return emails;
}
