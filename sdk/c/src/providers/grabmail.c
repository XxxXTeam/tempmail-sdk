/**
 * GrabMail 渠道 — https://grabmail.io
 *
 * 地址即邮箱：首封邮件到达时自动存在，无需创建。
 * 列表 GET /api/v1/mailbox?address={email} → {messages:[...]}。
 * 详情 GET /api/v1/message/{id}?mailbox={email} →
 *   {from,from_name,subject,text,html,date,attachments}；详情失败回退摘要。
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GRAB_BASE "https://grabmail.io/api/v1"

static const char *grab_headers[] = {"Accept: application/json", NULL};

/** 生成 16 位随机本地部分 */
static void grab_random_local(char *buf, size_t cap) {
  static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
  if (cap < 5)
    return;
  memcpy(buf, "sdk", 3);
  size_t n = 16;
  if (n + 4 > cap)
    n = cap - 4;
  for (size_t i = 0; i < n; i++)
    buf[3 + i] = chars[rand() % (sizeof(chars) - 1)];
  buf[3 + n] = '\0';
}

/** 生成 GrabMail 地址（无需请求服务端） */
tm_email_info_t *tm_provider_grabmail_generate(void) {
  char local[24];
  grab_random_local(local, sizeof(local));

  tm_email_info_t *info = tm_email_info_new();
  if (!info)
    return NULL;
  char address[64];
  snprintf(address, sizeof(address), "%s@grabmail.io", local);
  info->channel = CHANNEL_GRABMAIL;
  info->email = tm_strdup(address);
  info->token = tm_strdup("");
  return info;
}

static void grab_add_from(cJSON *flat, cJSON *source) {
  const char *addr =
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(source, "from"), "");
  const char *name =
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(source, "from_name"), "");
  if (name[0] && addr[0]) {
    char composed[512];
    snprintf(composed, sizeof(composed), "%s <%s>", name, addr);
    cJSON_AddStringToObject(flat, "from", composed);
  } else {
    cJSON_AddStringToObject(flat, "from", addr);
  }
}

/** 读取 GrabMail 邮件，逐条补全正文 */
tm_email_t *tm_provider_grabmail_get_emails(const char *email, int *count) {
  *count = 0;
  if (!email || !email[0])
    return NULL;

  size_t need = strlen(GRAB_BASE) + strlen(email) + 48;
  char *url = (char *)malloc(need);
  if (!url)
    return NULL;
  snprintf(url, need, "%s/mailbox?address=%s", GRAB_BASE, email);
  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, grab_headers, NULL, 20);
  free(url);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    TM_LOG_ERR("grabmail: 获取邮件列表失败 http %ld", resp ? resp->status : -1);
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
      size_t need2 = strlen(GRAB_BASE) + strlen(msg_id) + strlen(email) + 48;
      char *detail_url = (char *)malloc(need2);
      if (detail_url) {
        snprintf(detail_url, need2, "%s/message/%s?mailbox=%s", GRAB_BASE,
                 msg_id, email);
        tm_http_response_t *detail_resp =
            tm_http_request(TM_HTTP_GET, detail_url, grab_headers, NULL, 20);
        free(detail_url);
        if (detail_resp && detail_resp->status >= 200 &&
            detail_resp->status < 300) {
          detail = cJSON_Parse(detail_resp->body);
        }
        tm_http_response_free(detail_resp);
      }
    }

    if (detail && cJSON_IsObject(detail)) {
      cJSON *flat = cJSON_CreateObject();
      cJSON_AddItemToObject(
          flat, "id",
          cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(detail, "id"), 1));
      grab_add_from(flat, detail);
      cJSON_AddStringToObject(flat, "to", email);
      cJSON_AddItemToObject(
          flat, "subject",
          cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(detail, "subject"),
                          1));
      cJSON_AddItemToObject(
          flat, "text",
          cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(detail, "text"), 1));
      cJSON_AddItemToObject(
          flat, "html",
          cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(detail, "html"), 1));
      cJSON_AddItemToObject(
          flat, "date",
          cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(detail, "date"), 1));
      cJSON_AddItemToObject(
          flat, "seen",
          cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(detail, "seen"), 1));
      cJSON_AddItemToObject(
          flat, "attachments",
          cJSON_Duplicate(
              cJSON_GetObjectItemCaseSensitive(detail, "attachments"), 1));
      emails[i] = tm_normalize_email(flat, email);
      cJSON_Delete(flat);
      cJSON_Delete(detail);
      continue;
    }
    cJSON_Delete(detail);

    cJSON *flat = cJSON_CreateObject();
    cJSON_AddStringToObject(flat, "id", msg_id);
    grab_add_from(flat, item);
    cJSON_AddStringToObject(flat, "to", email);
    cJSON_AddItemToObject(
        flat, "subject",
        cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(item, "subject"), 1));
    cJSON_AddItemToObject(
        flat, "text",
        cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(item, "preview"), 1));
    cJSON_AddItemToObject(
        flat, "date",
        cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(item, "date"), 1));
    cJSON_AddItemToObject(
        flat, "seen",
        cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(item, "seen"), 1));
    emails[i] = tm_normalize_email(flat, email);
    cJSON_Delete(flat);
  }

  cJSON_Delete(root);
  return emails;
}
