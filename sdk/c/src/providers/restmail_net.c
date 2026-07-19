/**
 * restmail-net 渠道 — https://restmail.net
 *
 * Mozilla 开源项目，无需创建邮箱，ad-hoc 模式。
 * 随机生成 username，邮箱即为 username@restmail.net。
 * 收件: GET https://restmail.net/mail/{username}
 */

#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define RESTMAIL_BASE "https://restmail.net"

static const char *restmail_headers[] = {
    "Accept: application/json",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
    NULL};

/**
 * 生成 10-12 位随机用户名
 */
static void restmail_random_username(char *buf, size_t cap) {
  static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
  if (cap < 11)
    return;
  int len = 10 + (rand() % 3); /* 10, 11, 或 12 */
  if ((size_t)len >= cap)
    len = (int)(cap - 1);
  for (int i = 0; i < len; i++) {
    buf[i] = chars[rand() % (sizeof(chars) - 1)];
  }
  buf[len] = '\0';
}

/**
 * 创建 restmail.net 临时邮箱
 * 无需请求服务端，随机生成 username 即可使用
 */
tm_email_info_t *tm_provider_restmail_net_generate(void) {
  char username[16];
  restmail_random_username(username, sizeof(username));

  tm_email_info_t *info = tm_email_info_new();
  if (!info)
    return NULL;

  /* 构造邮箱地址 */
  char email[64];
  snprintf(email, sizeof(email), "%s@restmail.net", username);

  info->email = tm_strdup(email);
  info->token = tm_strdup(""); /* ad-hoc 模式，无 token */
  return info;
}

/**
 * 获取 restmail.net 邮件列表
 * GET /mail/{username}，返回 JSON 数组
 */
tm_email_t *tm_provider_restmail_net_get_emails(const char *email, int *count) {
  *count = 0;
  if (!email || !email[0])
    return NULL;

  /* 从邮箱地址中提取 username（@前面部分） */
  const char *at = strchr(email, '@');
  size_t ulen = at ? (size_t)(at - email) : strlen(email);
  if (ulen == 0)
    return NULL;

  char username[64];
  if (ulen >= sizeof(username))
    ulen = sizeof(username) - 1;
  memcpy(username, email, ulen);
  username[ulen] = '\0';

  /* 构造请求 URL */
  char url[256];
  snprintf(url, sizeof(url), "%s/mail/%s", RESTMAIL_BASE, username);

  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, restmail_headers, NULL, 15);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }

  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!root || !cJSON_IsArray(root)) {
    cJSON_Delete(root);
    return NULL;
  }

  int n = cJSON_GetArraySize(root);
  if (n == 0) {
    cJSON_Delete(root);
    return NULL;
  }

  *count = n;
  tm_email_t *emails = tm_emails_new(n);
  if (!emails) {
    *count = -1;
    cJSON_Delete(root);
    return NULL;
  }

  for (int i = 0; i < n; i++) {
    cJSON *msg = cJSON_GetArrayItem(root, i);

    /* 提取发件人：优先 from[0].address，其次 headers.from */
    const char *from_addr = "";
    cJSON *from_arr = cJSON_GetObjectItemCaseSensitive(msg, "from");
    if (cJSON_IsArray(from_arr) && cJSON_GetArraySize(from_arr) > 0) {
      cJSON *first = cJSON_GetArrayItem(from_arr, 0);
      if (cJSON_IsObject(first)) {
        const char *addr =
            TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(first, "address"), "");
        if (addr[0])
          from_addr = addr;
      }
    }
    if (!from_addr[0]) {
      cJSON *headers = cJSON_GetObjectItemCaseSensitive(msg, "headers");
      if (cJSON_IsObject(headers)) {
        from_addr =
            TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(headers, "from"), "");
      }
    }

    /* 邮件正文：优先 html，其次 text */
    const char *html =
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(msg, "html"), "");
    const char *text =
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(msg, "text"), "");

    /* 构造标准化输入 */
    cJSON *raw = cJSON_CreateObject();
    cJSON_AddStringToObject(raw, "id", "");
    cJSON_AddStringToObject(raw, "from", from_addr);
    cJSON_AddStringToObject(raw, "to", email);
    cJSON_AddStringToObject(
        raw, "subject",
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(msg, "subject"), ""));
    cJSON_AddStringToObject(raw, "text", text);
    cJSON_AddStringToObject(raw, "html", html);
    cJSON_AddStringToObject(
        raw, "date",
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(msg, "receivedAt"), ""));

    emails[i] = tm_normalize_email(raw, email);
    cJSON_Delete(raw);
  }

  cJSON_Delete(root);
  return emails;
}
