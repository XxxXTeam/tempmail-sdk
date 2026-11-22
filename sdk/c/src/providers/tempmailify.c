/**
 * Tempmailify 渠道 — https://tempmailify.com
 *
 * 创建: POST /api/inbox body {} → {address,token,createdAt,expiresAt}。
 * 列表 GET /api/inbox/{address}（Bearer）→ {messages:[...]}。
 * 详情 GET /api/inbox/{address}/messages/{id}（Bearer）→
 *   {from:{address,name},subject,text,html,receivedAt,...}；详情失败回退摘要。
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TIF_BASE "https://tempmailify.com"

static const char *tif_post_headers[] = {"Content-Type: application/json",
                                         "Accept: application/json", NULL};

static char **tif_auth_headers(const char *token) {
  const char **h = (const char **)calloc(4, sizeof(char *));
  if (!h)
    return NULL;
  h[0] = "Accept: application/json";
  size_t len = strlen(token) + 32;
  char *auth = (char *)malloc(len);
  if (!auth) {
    free(h);
    return NULL;
  }
  snprintf(auth, len, "Authorization: Bearer %s", token);
  h[1] = auth;
  h[2] = NULL;
  return (char **)h;
}

static void tif_free_headers(char **h) {
  if (!h)
    return;
  free(h[1]);
  free(h);
}

/** 创建 Tempmailify 临时邮箱 */
tm_email_info_t *tm_provider_tempmailify_generate(void) {
  char url[128];
  snprintf(url, sizeof(url), "%s/api/inbox", TIF_BASE);
  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_POST, url, tif_post_headers, "{}", 20);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    TM_LOG_ERR("tempmailify: 建箱失败 http %ld", resp ? resp->status : -1);
    tm_http_response_free(resp);
    return NULL;
  }
  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!root)
    return NULL;

  const char *address =
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "address"), "");
  const char *token =
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "token"), "");
  if (!address[0] || !token[0]) {
    TM_LOG_ERR("tempmailify: 建箱响应不完整");
    cJSON_Delete(root);
    return NULL;
  }

  tm_email_info_t *info = tm_email_info_new();
  if (!info) {
    cJSON_Delete(root);
    return NULL;
  }
  info->channel = CHANNEL_TEMPMAILIFY;
  info->email = tm_strdup(address);
  info->token = tm_strdup(token);
  cJSON_Delete(root);
  return info;
}

static void tif_add_from(cJSON *flat, cJSON *source) {
  cJSON *from = cJSON_GetObjectItemCaseSensitive(source, "from");
  if (cJSON_IsObject(from)) {
    cJSON_AddItemToObject(
        flat, "from",
        cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(from, "address"), 1));
  } else {
    cJSON_AddItemToObject(flat, "from", cJSON_Duplicate(from, 1));
  }
}

/** 读取 Tempmailify 邮件，详情提供完整 text/html */
tm_email_t *tm_provider_tempmailify_get_emails(const char *email,
                                                const char *token, int *count) {
  *count = 0;
  if (!email || !email[0] || !token || !token[0])
    return NULL;

  size_t need = strlen(TIF_BASE) + strlen(email) + 32;
  char *url = (char *)malloc(need);
  if (!url)
    return NULL;
  snprintf(url, need, "%s/api/inbox/%s", TIF_BASE, email);
  char **h = tif_auth_headers(token);
  if (!h) {
    free(url);
    return NULL;
  }
  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, (const char **)h, NULL, 20);
  tif_free_headers(h);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    TM_LOG_ERR("tempmailify: 获取邮件列表失败 http %ld",
               resp ? resp->status : -1);
    tm_http_response_free(resp);
    free(url);
    return NULL;
  }
  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!root) {
    free(url);
    return NULL;
  }

  cJSON *rows = cJSON_GetObjectItemCaseSensitive(root, "messages");
  if (!cJSON_IsArray(rows) || cJSON_GetArraySize(rows) == 0) {
    cJSON_Delete(root);
    free(url);
    return NULL;
  }

  int n = cJSON_GetArraySize(rows);
  *count = n;
  tm_email_t *emails = tm_emails_new(n);
  if (!emails) {
    *count = -1;
    cJSON_Delete(root);
    free(url);
    return NULL;
  }

  for (int i = 0; i < n; i++) {
    cJSON *item = cJSON_GetArrayItem(rows, i);
    const char *msg_id =
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(item, "id"), "");
    cJSON *detail = NULL;
    if (msg_id[0]) {
      size_t need2 = strlen(url) + strlen(msg_id) + 16;
      char *detail_url = (char *)malloc(need2);
      char **h2 = tif_auth_headers(token);
      if (detail_url && h2) {
        snprintf(detail_url, need2, "%s/messages/%s", url, msg_id);
        tm_http_response_t *detail_resp =
            tm_http_request(TM_HTTP_GET, detail_url, (const char **)h2, NULL,
                            20);
        if (detail_resp && detail_resp->status >= 200 &&
            detail_resp->status < 300) {
          detail = cJSON_Parse(detail_resp->body);
        }
        tm_http_response_free(detail_resp);
      }
      free(detail_url);
      tif_free_headers(h2);
    }

    if (detail) {
      cJSON *flat = cJSON_CreateObject();
      cJSON_AddItemToObject(
          flat, "id",
          cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(detail, "id"), 1));
      tif_add_from(flat, detail);
      cJSON_AddItemToObject(
          flat, "to",
          cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(detail, "to"), 1));
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
          cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(detail, "receivedAt"),
                          1));
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

    cJSON *flat = cJSON_CreateObject();
    cJSON_AddStringToObject(flat, "id", msg_id);
    tif_add_from(flat, item);
    cJSON_AddStringToObject(flat, "to", email);
    cJSON_AddItemToObject(
        flat, "subject",
        cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(item, "subject"), 1));
    cJSON_AddItemToObject(
        flat, "text",
        cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(item, "preview"), 1));
    cJSON_AddItemToObject(
        flat, "date",
        cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(item, "receivedAt"),
                        1));
    cJSON_AddItemToObject(
        flat, "seen",
        cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(item, "seen"), 1));
    emails[i] = tm_normalize_email(flat, email);
    cJSON_Delete(flat);
  }

  cJSON_Delete(root);
  free(url);
  return emails;
}
