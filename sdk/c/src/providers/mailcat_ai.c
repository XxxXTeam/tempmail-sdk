/**
 * mailcat.ai 渠道实现
 *
 * 流程：
 * 1. POST /mailboxes 创建邮箱（无 body）
 * 2. GET /inbox 获取邮件列表（需要 Authorization: Bearer {token}）
 *
 * token 在创建时返回，后续读信必须携带
 */
#include "tempmail_internal.h"

#define MAILCAT_BASE "https://api.mailcat.ai"

tm_email_info_t *tm_provider_mailcat_ai_generate(void) {
  const char *headers[] = {
      "Accept: application/json",
      "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
      "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
      NULL};

  tm_http_response_t *resp = tm_http_request(
      TM_HTTP_POST, MAILCAT_BASE "/mailboxes", headers, NULL, 15);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }

  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!root)
    return NULL;

  /* 提取 data.email 和 data.token */
  cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
  if (!data) {
    cJSON_Delete(root);
    return NULL;
  }

  cJSON *email_item = cJSON_GetObjectItemCaseSensitive(data, "email");
  cJSON *token_item = cJSON_GetObjectItemCaseSensitive(data, "token");

  const char *email_str = cJSON_GetStringValue(email_item);
  const char *token_str = cJSON_GetStringValue(token_item);

  if (!email_str || !email_str[0] || !token_str || !token_str[0]) {
    cJSON_Delete(root);
    return NULL;
  }

  tm_email_info_t *info = tm_email_info_new();
  info->channel = CHANNEL_MAILCAT_AI;
  info->email = tm_strdup(email_str);
  info->token = tm_strdup(token_str);

  cJSON_Delete(root);
  return info;
}

tm_email_t *tm_provider_mailcat_ai_get_emails(const char *token,
                                              const char *email, int *count) {
  *count = -1;
  if (!token || !token[0] || !email || !email[0])
    return NULL;

  /* 构建 Authorization header */
  char auth_hdr[512];
  snprintf(auth_hdr, sizeof(auth_hdr), "Authorization: Bearer %s", token);

  const char *headers[] = {auth_hdr, "Accept: application/json",
                           "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; "
                           "x64) AppleWebKit/537.36",
                           NULL};

  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, MAILCAT_BASE "/inbox", headers, NULL, 15);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }

  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!root)
    return NULL;

  /* 提取 data 数组 */
  cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
  if (!data || !cJSON_IsArray(data)) {
    *count = 0;
    cJSON_Delete(root);
    return NULL;
  }

  int n = cJSON_GetArraySize(data);
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
    cJSON *msg = cJSON_GetArrayItem(data, i);
    emails[i] = tm_normalize_email(msg, email);
  }

  cJSON_Delete(root);
  return emails;
}
