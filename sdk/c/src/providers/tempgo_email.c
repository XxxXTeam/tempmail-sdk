/**
 * tempgo-email 渠道实现
 *
 * 流程：
 * 1. POST /api/generate 创建邮箱（无 body，不发送 Content-Type）
 * 2. GET /api/inbox?email={email}&mailbox_id={token} 获取邮件列表
 *
 * token 存储为 mailbox_id
 */
#include "tempmail_internal.h"

#define TEMPGO_BASE "https://tempgo.email"

tm_email_info_t *tm_provider_tempgo_email_generate(void) {
  const char *headers[] = {
      "Accept: application/json",
      "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
      "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
      NULL};

  tm_http_response_t *resp = tm_http_request(
      TM_HTTP_POST, TEMPGO_BASE "/api/generate", headers, NULL, 15);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }

  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!root)
    return NULL;

  cJSON *email_item = cJSON_GetObjectItemCaseSensitive(root, "email");
  cJSON *mailbox_id_item =
      cJSON_GetObjectItemCaseSensitive(root, "mailbox_id");

  const char *email_str = cJSON_GetStringValue(email_item);
  const char *mailbox_id_str = cJSON_GetStringValue(mailbox_id_item);

  if (!email_str || !email_str[0] || !mailbox_id_str || !mailbox_id_str[0]) {
    cJSON_Delete(root);
    return NULL;
  }

  tm_email_info_t *info = tm_email_info_new();
  info->channel = CHANNEL_TEMPGO_EMAIL;
  info->email = tm_strdup(email_str);
  info->token = tm_strdup(mailbox_id_str);

  cJSON_Delete(root);
  return info;
}

tm_email_t *tm_provider_tempgo_email_get_emails(const char *token,
                                                const char *email,
                                                int *count) {
  *count = -1;
  if (!token || !token[0] || !email || !email[0])
    return NULL;

  /* 构建请求 URL */
  char url[1024];
  snprintf(url, sizeof(url),
           TEMPGO_BASE "/api/inbox?email=%s&mailbox_id=%s", email, token);

  const char *headers[] = {
      "Accept: application/json",
      "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
      "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
      NULL};

  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, headers, NULL, 15);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }

  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!root)
    return NULL;

  /* 提取 messages 数组 */
  cJSON *messages = cJSON_GetObjectItemCaseSensitive(root, "messages");
  if (!messages || !cJSON_IsArray(messages)) {
    *count = 0;
    cJSON_Delete(root);
    return NULL;
  }

  int n = cJSON_GetArraySize(messages);
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
    cJSON *msg = cJSON_GetArrayItem(messages, i);
    emails[i] = tm_normalize_email(msg, email);
  }

  cJSON_Delete(root);
  return emails;
}
