/**
 * DummyInbox 渠道 — https://dummyinbox.com
 *
 * 无需密钥；创建随机地址后，地址本身即读取凭据。
 *   - 建箱 POST /api/v1/inboxes → {address,domain,webUrl}。
 *   - 列表 GET /api/v1/inboxes/{address}/messages?limit=50 →
 *     {messages:[{id,from,subject,snippet,receivedAt}],total}。
 *   - 详情 GET /api/v1/inboxes/{address}/messages/{id} →
 *     {message:{text,html,attachments,...}}；详情失败回退摘要。
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DUMMY_BASE "https://dummyinbox.com/api/v1"

static const char *dummy_headers[] = {"Accept: application/json", NULL};

/** 创建 DummyInbox 临时邮箱 */
tm_email_info_t *tm_provider_dummyinbox_generate(void) {
  char url[128];
  snprintf(url, sizeof(url), "%s/inboxes", DUMMY_BASE);

  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_POST, url, dummy_headers, NULL, 20);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    TM_LOG_ERR("dummyinbox: 建箱失败 http %ld", resp ? resp->status : -1);
    tm_http_response_free(resp);
    return NULL;
  }
  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!root) {
    TM_LOG_ERR("dummyinbox: 解析建箱响应失败");
    return NULL;
  }
  const char *address =
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "address"), "");
  if (!address[0]) {
    TM_LOG_ERR("dummyinbox: 建箱响应缺少地址");
    cJSON_Delete(root);
    return NULL;
  }

  tm_email_info_t *info = tm_email_info_new();
  if (!info) {
    cJSON_Delete(root);
    return NULL;
  }
  info->channel = CHANNEL_DUMMYINBOX;
  info->email = tm_strdup(address);
  info->token = tm_strdup("");
  cJSON_Delete(root);
  return info;
}

/** 将详情 message 对象映射为标准字段 */
static tm_email_t dummy_flatten(cJSON *message, const char *recipient) {
  cJSON *flat = cJSON_CreateObject();
  cJSON_AddItemToObject(flat, "id",
                        cJSON_Duplicate(
                            cJSON_GetObjectItemCaseSensitive(message, "id"), 1));
  cJSON_AddItemToObject(
      flat, "from",
      cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(message, "from"), 1));
  cJSON_AddStringToObject(flat, "to", recipient);
  cJSON_AddItemToObject(
      flat, "subject",
      cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(message, "subject"), 1));
  cJSON_AddItemToObject(
      flat, "text",
      cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(message, "text"), 1));
  cJSON_AddItemToObject(
      flat, "html",
      cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(message, "html"), 1));
  cJSON_AddItemToObject(
      flat, "date",
      cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(message, "receivedAt"),
                      1));
  cJSON_AddItemToObject(
      flat, "attachments",
      cJSON_Duplicate(
          cJSON_GetObjectItemCaseSensitive(message, "attachments"), 1));
  tm_email_t email = tm_normalize_email(flat, recipient);
  cJSON_Delete(flat);
  return email;
}

/** 读取 DummyInbox 邮件列表，逐封补全正文 */
tm_email_t *tm_provider_dummyinbox_get_emails(const char *email, int *count) {
  *count = 0;
  if (!email || !email[0])
    return NULL;

  size_t need = strlen(DUMMY_BASE) + strlen(email) + 48;
  char *url = (char *)malloc(need);
  if (!url)
    return NULL;
  snprintf(url, need, "%s/inboxes/%s/messages?limit=50", DUMMY_BASE, email);
  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, dummy_headers, NULL, 20);
  free(url);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    TM_LOG_ERR("dummyinbox: 获取邮件列表失败 http %ld",
               resp ? resp->status : -1);
    tm_http_response_free(resp);
    return NULL;
  }
  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!root) {
    TM_LOG_ERR("dummyinbox: 解析邮件列表失败");
    return NULL;
  }

  cJSON *rows = cJSON_GetObjectItemCaseSensitive(root, "messages");
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
    const char *msg_id =
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(item, "id"), "");
    cJSON *message = NULL;
    if (msg_id[0]) {
      size_t need2 = strlen(DUMMY_BASE) + strlen(email) + strlen(msg_id) + 48;
      char *detail_url = (char *)malloc(need2);
      if (detail_url) {
        snprintf(detail_url, need2, "%s/inboxes/%s/messages/%s", DUMMY_BASE,
                 email, msg_id);
        tm_http_response_t *detail_resp =
            tm_http_request(TM_HTTP_GET, detail_url, dummy_headers, NULL, 20);
        free(detail_url);
        if (detail_resp && detail_resp->status >= 200 &&
            detail_resp->status < 300) {
          cJSON *detail = cJSON_Parse(detail_resp->body);
          if (detail) {
            cJSON *nested =
                cJSON_GetObjectItemCaseSensitive(detail, "message");
            if (cJSON_IsObject(nested))
              message = cJSON_Duplicate(nested, 1);
            cJSON_Delete(detail);
          }
        }
        tm_http_response_free(detail_resp);
      }
    }

    if (message) {
      emails[i] = dummy_flatten(message, email);
      cJSON_Delete(message);
      continue;
    }

    /* 详情失败时退回摘要 */
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
    cJSON_AddItemToObject(
        flat, "text",
        cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(item, "snippet"), 1));
    cJSON_AddItemToObject(
        flat, "date",
        cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(item, "receivedAt"),
                        1));
    emails[i] = tm_normalize_email(flat, email);
    cJSON_Delete(flat);
  }

  cJSON_Delete(root);
  return emails;
}
