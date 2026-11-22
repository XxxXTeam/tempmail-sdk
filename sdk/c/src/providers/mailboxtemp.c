/**
 * MailboxTemp 渠道 — https://mailboxtemp.com
 *
 * 无需密钥；创建免费邮箱后，地址即读取凭据。
 *   - 建箱 POST /api/inbox/generate → {ok,address,expires,inboxId}。
 *   - 列表 GET /api/inbox/{address}/emails → {ok,emails:[...]}。
 *   - 详情 GET /api/email/{address}/{id} →
 *     {ok,email:{body_text,body_html,...},attachments:[]}；详情失败回退摘要。
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MT_BASE "https://mailboxtemp.com"

static const char *mt_headers[] = {"Accept: application/json", NULL};

/** 创建 MailboxTemp 临时邮箱 */
tm_email_info_t *tm_provider_mailboxtemp_generate(void) {
  char url[128];
  snprintf(url, sizeof(url), "%s/api/inbox/generate", MT_BASE);
  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_POST, url, mt_headers, NULL, 20);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    TM_LOG_ERR("mailboxtemp: 建箱失败 http %ld", resp ? resp->status : -1);
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
    TM_LOG_ERR("mailboxtemp: 建箱响应缺少地址");
    cJSON_Delete(root);
    return NULL;
  }
  tm_email_info_t *info = tm_email_info_new();
  if (!info) {
    cJSON_Delete(root);
    return NULL;
  }
  info->channel = CHANNEL_MAILBOXTEMP;
  info->email = tm_strdup(address);
  info->token = tm_strdup("");
  cJSON_Delete(root);
  return info;
}

static tm_email_t mt_flatten(cJSON *message, cJSON *attachments,
                             const char *recipient) {
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
      cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(message, "body_text"), 1));
  cJSON_AddItemToObject(
      flat, "html",
      cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(message, "body_html"), 1));
  cJSON_AddItemToObject(
      flat, "date",
      cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(message, "received_at"),
                      1));
  cJSON_AddItemToObject(
      flat, "read",
      cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(message, "read"), 1));
  cJSON_AddItemToObject(flat, "attachments",
                        cJSON_Duplicate(attachments, 1));
  tm_email_t email = tm_normalize_email(flat, recipient);
  cJSON_Delete(flat);
  return email;
}

/** 读取 MailboxTemp 邮件，逐条补全正文 */
tm_email_t *tm_provider_mailboxtemp_get_emails(const char *email, int *count) {
  *count = 0;
  if (!email || !email[0])
    return NULL;

  size_t need = strlen(MT_BASE) + strlen(email) + 48;
  char *url = (char *)malloc(need);
  if (!url)
    return NULL;
  snprintf(url, need, "%s/api/inbox/%s/emails", MT_BASE, email);
  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, mt_headers, NULL, 20);
  free(url);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    TM_LOG_ERR("mailboxtemp: 获取邮件列表失败 http %ld",
               resp ? resp->status : -1);
    tm_http_response_free(resp);
    return NULL;
  }
  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!root)
    return NULL;

  cJSON *rows = cJSON_GetObjectItemCaseSensitive(root, "emails");
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
      size_t need2 = strlen(MT_BASE) + strlen(email) + strlen(msg_id) + 48;
      char *detail_url = (char *)malloc(need2);
      if (detail_url) {
        snprintf(detail_url, need2, "%s/api/email/%s/%s", MT_BASE, email,
                 msg_id);
        tm_http_response_t *detail_resp =
            tm_http_request(TM_HTTP_GET, detail_url, mt_headers, NULL, 20);
        free(detail_url);
        if (detail_resp && detail_resp->status >= 200 &&
            detail_resp->status < 300) {
          detail = cJSON_Parse(detail_resp->body);
        }
        tm_http_response_free(detail_resp);
      }
    }

    if (detail) {
      cJSON *message = cJSON_GetObjectItemCaseSensitive(detail, "email");
      if (cJSON_IsObject(message)) {
        emails[i] = mt_flatten(
            message, cJSON_GetObjectItemCaseSensitive(detail, "attachments"),
            email);
        cJSON_Delete(detail);
        continue;
      }
      cJSON_Delete(detail);
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
        flat, "date",
        cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(item, "received_at"),
                        1));
    cJSON_AddItemToObject(
        flat, "read",
        cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(item, "read"), 1));
    emails[i] = tm_normalize_email(flat, email);
    cJSON_Delete(flat);
  }

  cJSON_Delete(root);
  return emails;
}
