/**
 * gonebox.email 渠道实现
 *
 * 流程：
 * 1. POST /api/v1/inboxes 创建邮箱（指定域名 gonebox.email）
 * 2. GET /api/v1/inboxes/{address}/messages 获取邮件列表
 *
 * 无需认证，无 token
 */
#include "tempmail_internal.h"

#define GONEBOX_BASE "https://api.gonebox.email"

static const char *gonebox_headers[] = {
    "Content-Type: application/json",
    "Accept: application/json",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
    NULL};

tm_email_info_t *tm_provider_gonebox_email_generate(void) {
  const char *body = "{\"domain\":\"gonebox.email\"}";

  tm_http_response_t *resp = tm_http_request(
      TM_HTTP_POST, GONEBOX_BASE "/api/v1/inboxes", gonebox_headers, body, 15);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }

  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!root)
    return NULL;

  /* 检查 success 字段 */
  cJSON *success = cJSON_GetObjectItemCaseSensitive(root, "success");
  if (!cJSON_IsTrue(success)) {
    cJSON_Delete(root);
    return NULL;
  }

  /* 提取 data.address */
  cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
  if (!data) {
    cJSON_Delete(root);
    return NULL;
  }

  cJSON *address = cJSON_GetObjectItemCaseSensitive(data, "address");
  const char *addr_str = cJSON_GetStringValue(address);
  if (!addr_str || !addr_str[0]) {
    cJSON_Delete(root);
    return NULL;
  }

  tm_email_info_t *info = tm_email_info_new();
  info->channel = CHANNEL_GONEBOX_EMAIL;
  info->email = tm_strdup(addr_str);
  info->token = NULL;

  cJSON_Delete(root);
  return info;
}

tm_email_t *tm_provider_gonebox_email_get_emails(const char *email, int *count) {
  *count = -1;
  if (!email || !email[0])
    return NULL;

  char url[512];
  snprintf(url, sizeof(url), GONEBOX_BASE "/api/v1/inboxes/%s/messages", email);

  const char *get_headers[] = {"Accept: application/json",
                               "User-Agent: Mozilla/5.0 (Windows NT 10.0; "
                               "Win64; x64) AppleWebKit/537.36",
                               NULL};

  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, get_headers, NULL, 15);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }

  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!root)
    return NULL;

  /* 检查 success 字段 */
  cJSON *success = cJSON_GetObjectItemCaseSensitive(root, "success");
  if (!cJSON_IsTrue(success)) {
    cJSON_Delete(root);
    return NULL;
  }

  /* 提取 data.messages 数组 */
  cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
  if (!data) {
    cJSON_Delete(root);
    return NULL;
  }

  cJSON *messages = cJSON_GetObjectItemCaseSensitive(data, "messages");
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
