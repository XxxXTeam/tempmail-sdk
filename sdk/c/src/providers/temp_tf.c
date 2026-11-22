/**
 * temp.tf 渠道 — https://temp.tf
 *
 * 创建 GET /api/account?plus=1 → {email}。
 * 收件 POST /api/check {email} → {data:[...]}。
 * body 按 bodyContentType 归位为 text 或 html。
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TTF_BASE "https://temp.tf/api"

static const char *ttf_headers[] = {"Accept: application/json", NULL};
static const char *ttf_post_headers[] = {"Accept: application/json",
                                         "Content-Type: application/json", NULL};

/** 获取一个一次性地址 */
tm_email_info_t *tm_provider_temp_tf_generate(void) {
  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, TTF_BASE "/account?plus=1", ttf_headers, NULL, 20);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }
  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!root)
    return NULL;
  const char *email =
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "email"), "");
  if (!email[0]) {
    cJSON_Delete(root);
    return NULL;
  }
  tm_email_info_t *info = tm_email_info_new();
  if (!info) {
    cJSON_Delete(root);
    return NULL;
  }
  info->channel = CHANNEL_TEMP_TF;
  info->email = tm_strdup(email);
  info->token = tm_strdup("");
  cJSON_Delete(root);
  return info;
}

/** 读取 temp.tf 邮件，body 按 bodyContentType 归位 */
tm_email_t *tm_provider_temp_tf_get_emails(const char *email, int *count) {
  *count = 0;
  if (!email || !email[0])
    return NULL;

  size_t need = strlen(email) + 64;
  char *body = (char *)malloc(need);
  if (!body)
    return NULL;
  snprintf(body, need, "{\"email\":\"%s\"}", email);
  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_POST, TTF_BASE "/check", ttf_post_headers, body, 20);
  free(body);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }
  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!root)
    return NULL;

  cJSON *rows = cJSON_GetObjectItemCaseSensitive(root, "data");
  if (!cJSON_IsArray(rows) || cJSON_GetArraySize(rows) == 0) {
    cJSON_Delete(root);
    return NULL;
  }

  int n = cJSON_GetArraySize(rows);
  *count = n;
  tm_email_t *emails = tm_emails_new(n);
  if (!emails) {
    *count = -1;
    cJSON_Delete(root);
    return NULL;
  }

  for (int i = 0; i < n; i++) {
    cJSON *item = cJSON_GetArrayItem(rows, i);
    const char *content_type =
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(item, "bodyContentType"), "");
    cJSON *flat = cJSON_CreateObject();
    cJSON_AddItemToObject(
        flat, "id",
        cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(item, "id"), 1));
    cJSON_AddItemToObject(
        flat, "from",
        cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(item, "from"), 1));
    cJSON_AddStringToObject(flat, "to", email);
    cJSON_AddItemToObject(
        flat, "subject",
        cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(item, "subject"), 1));
    if (strcasecmp(content_type, "text") == 0) {
      cJSON_AddItemToObject(
          flat, "text",
          cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(item, "body"), 1));
    } else {
      cJSON_AddItemToObject(
          flat, "html",
          cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(item, "body"), 1));
    }
    cJSON_AddItemToObject(
        flat, "date",
        cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(item, "date"), 1));
    cJSON_AddItemToObject(
        flat, "attachments",
        cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(item, "attachments"), 1));
    emails[i] = tm_normalize_email(flat, email);
    cJSON_Delete(flat);
  }

  cJSON_Delete(root);
  return emails;
}
