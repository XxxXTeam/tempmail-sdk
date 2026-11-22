/**
 * Tsukaisute Mail 渠道 — https://tsukaisutemail.com
 *
 * 无需密钥；创建随机地址后，地址即读取凭据。
 *   - 建地址 POST /api/v1/address → {address,domain,expiresInHours}。
 *   - 列表 GET /api/v1/inbox/{address} → {messages:[...]}。
 *   - 详情 GET /api/v1/message/{id} →
 *     {message:{text_body,html_body,from_address,subject,received_at}}；
 *     详情失败回退摘要。
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TSUKA_BASE "https://tsukaisutemail.com/api/v1"

static const char *tsuka_headers[] = {"Accept: application/json", NULL};

/** 创建 Tsukaisute Mail 临时地址 */
tm_email_info_t *tm_provider_tsukaisutemail_generate(void) {
  char url[128];
  snprintf(url, sizeof(url), "%s/address", TSUKA_BASE);
  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_POST, url, tsuka_headers, NULL, 20);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    TM_LOG_ERR("tsukaisutemail: 建地址失败 http %ld", resp ? resp->status : -1);
    tm_http_response_free(resp);
    return NULL;
  }
  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!root)
    return NULL;
  const char *address =
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "address"), "");
  if (!address[0]) {
    TM_LOG_ERR("tsukaisutemail: 建地址响应缺少地址");
    cJSON_Delete(root);
    return NULL;
  }
  tm_email_info_t *info = tm_email_info_new();
  if (!info) {
    cJSON_Delete(root);
    return NULL;
  }
  info->channel = CHANNEL_TSUKAISUTEMAIL;
  info->email = tm_strdup(address);
  info->token = tm_strdup("");
  cJSON_Delete(root);
  return info;
}

static tm_email_t tsuka_flatten(cJSON *message, const char *recipient) {
  cJSON *flat = cJSON_CreateObject();
  cJSON_AddItemToObject(
      flat, "id",
      cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(message, "id"), 1));
  cJSON_AddItemToObject(
      flat, "from",
      cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(message, "from_address"),
                      1));
  cJSON_AddStringToObject(flat, "to", recipient);
  cJSON_AddItemToObject(
      flat, "subject",
      cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(message, "subject"), 1));
  cJSON_AddItemToObject(
      flat, "text",
      cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(message, "text_body"), 1));
  cJSON_AddItemToObject(
      flat, "html",
      cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(message, "html_body"), 1));
  cJSON_AddItemToObject(
      flat, "date",
      cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(message, "received_at"),
                      1));
  tm_email_t email = tm_normalize_email(flat, recipient);
  cJSON_Delete(flat);
  return email;
}

/** 读取 Tsukaisute Mail 邮件，逐条补全正文 */
tm_email_t *tm_provider_tsukaisutemail_get_emails(const char *email,
                                                   int *count) {
  *count = 0;
  if (!email || !email[0])
    return NULL;

  size_t need = strlen(TSUKA_BASE) + strlen(email) + 16;
  char *url = (char *)malloc(need);
  if (!url)
    return NULL;
  snprintf(url, need, "%s/inbox/%s", TSUKA_BASE, email);
  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, tsuka_headers, NULL, 20);
  free(url);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    TM_LOG_ERR("tsukaisutemail: 获取邮件列表失败 http %ld",
               resp ? resp->status : -1);
    tm_http_response_free(resp);
    return NULL;
  }
  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!root)
    return NULL;

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
    cJSON *detail = NULL;
    if (msg_id[0]) {
      size_t need2 = strlen(TSUKA_BASE) + strlen(msg_id) + 16;
      char *detail_url = (char *)malloc(need2);
      if (detail_url) {
        snprintf(detail_url, need2, "%s/message/%s", TSUKA_BASE, msg_id);
        tm_http_response_t *detail_resp =
            tm_http_request(TM_HTTP_GET, detail_url, tsuka_headers, NULL, 20);
        free(detail_url);
        if (detail_resp && detail_resp->status >= 200 &&
            detail_resp->status < 300) {
          cJSON *parsed = cJSON_Parse(detail_resp->body);
          if (parsed) {
            cJSON *nested =
                cJSON_GetObjectItemCaseSensitive(parsed, "message");
            if (cJSON_IsObject(nested))
              detail = cJSON_Duplicate(nested, 1);
            cJSON_Delete(parsed);
          }
        }
        tm_http_response_free(detail_resp);
      }
    }

    if (detail) {
      emails[i] = tsuka_flatten(detail, email);
      cJSON_Delete(detail);
      continue;
    }

    cJSON *flat = cJSON_CreateObject();
    cJSON_AddStringToObject(flat, "id", msg_id);
    cJSON_AddItemToObject(
        flat, "from",
        cJSON_Duplicate(
            cJSON_GetObjectItemCaseSensitive(item, "from_address"), 1));
    cJSON_AddStringToObject(flat, "to", email);
    cJSON_AddItemToObject(
        flat, "subject",
        cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(item, "subject"), 1));
    cJSON_AddItemToObject(
        flat, "text",
        cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(item, "snippet"), 1));
    cJSON_AddItemToObject(
        flat, "date",
        cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(item, "received_at"),
                        1));
    emails[i] = tm_normalize_email(flat, email);
    cJSON_Delete(flat);
  }

  cJSON_Delete(root);
  return emails;
}
