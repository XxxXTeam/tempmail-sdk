/**
 * Nextmail 渠道 — https://nextmail.sh
 *
 * 创建收件箱返回内部令牌；内部令牌格式为 “收件箱ID|读取令牌”。
 *   - 建箱 POST /temp-mail/inboxes body {} → {id,address,token,...}。
 *   - 列表 GET /temp-mail/inboxes/{id}/messages?limit=50（Bearer）→ {data:[...]}。
 *   - 详情 GET /temp-mail/inboxes/{id}/messages/{messageId}（Bearer）→
 *     顶层 {from:{email},to,subject,receivedAt} + message.decodedBody（HTML）。
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NM_BASE "https://api.nextmail.sh"

static const char *nm_post_headers[] = {"Content-Type: application/json",
                                        "Accept: application/json", NULL};

static char **nm_auth_headers(const char *token) {
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

static void nm_free_headers(char **h) {
  if (!h)
    return;
  free(h[1]);
  free(h);
}

static int nm_split_token(const char *token, char *id, size_t id_cap,
                          char *bearer, size_t bearer_cap) {
  if (!token)
    return -1;
  const char *sep = strchr(token, '|');
  if (!sep || sep == token || !sep[1])
    return -1;
  size_t id_len = (size_t)(sep - token);
  if (id_len >= id_cap)
    return -1;
  memcpy(id, token, id_len);
  id[id_len] = '\0';
  snprintf(bearer, bearer_cap, "%s", sep + 1);
  return 0;
}

/** 租用 Nextmail 临时收件箱 */
tm_email_info_t *tm_provider_nextmail_generate(void) {
  char url[128];
  snprintf(url, sizeof(url), "%s/temp-mail/inboxes", NM_BASE);
  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_POST, url, nm_post_headers, "{}", 20);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    TM_LOG_ERR("nextmail: 建箱失败 http %ld", resp ? resp->status : -1);
    tm_http_response_free(resp);
    return NULL;
  }
  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!root)
    return NULL;

  const char *address =
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "address"), "");
  const char *id = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "id"), "");
  const char *token =
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "token"), "");
  if (!address[0] || !id[0] || !token[0]) {
    TM_LOG_ERR("nextmail: 建箱响应不完整");
    cJSON_Delete(root);
    return NULL;
  }

  size_t packed_len = strlen(id) + strlen(token) + 2;
  char *packed = (char *)malloc(packed_len);
  if (!packed) {
    cJSON_Delete(root);
    return NULL;
  }
  snprintf(packed, packed_len, "%s|%s", id, token);

  tm_email_info_t *info = tm_email_info_new();
  if (!info) {
    free(packed);
    cJSON_Delete(root);
    return NULL;
  }
  info->channel = CHANNEL_NEXTMAIL;
  info->email = tm_strdup(address);
  info->token = packed;
  cJSON_Delete(root);
  return info;
}

/** 将详情映射为标准字段（HTML 位于 message.decodedBody） */
static tm_email_t nm_flatten(cJSON *detail, const char *recipient) {
  cJSON *flat = cJSON_CreateObject();
  cJSON_AddItemToObject(
      flat, "id",
      cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(detail, "id"), 1));
  cJSON *from = cJSON_GetObjectItemCaseSensitive(detail, "from");
  if (cJSON_IsObject(from)) {
    cJSON_AddItemToObject(
        flat, "from",
        cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(from, "email"), 1));
  }
  cJSON_AddItemToObject(
      flat, "to",
      cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(detail, "to"), 1));
  cJSON_AddItemToObject(
      flat, "subject",
      cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(detail, "subject"), 1));
  cJSON_AddItemToObject(
      flat, "date",
      cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(detail, "receivedAt"),
                      1));
  cJSON_AddItemToObject(
      flat, "seen",
      cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(detail, "seen"), 1));
  cJSON *message = cJSON_GetObjectItemCaseSensitive(detail, "message");
  if (cJSON_IsObject(message)) {
    cJSON_AddItemToObject(
        flat, "html",
        cJSON_Duplicate(
            cJSON_GetObjectItemCaseSensitive(message, "decodedBody"), 1));
  }
  tm_email_t email = tm_normalize_email(flat, recipient);
  cJSON_Delete(flat);
  return email;
}

/** 读取 Nextmail 邮件，详情正文位于 message.decodedBody */
tm_email_t *tm_provider_nextmail_get_emails(const char *email, const char *token,
                                            int *count) {
  *count = 0;
  if (!email || !email[0] || !token || !token[0])
    return NULL;

  char box_id[256], bearer[1024];
  if (nm_split_token(token, box_id, sizeof(box_id), bearer, sizeof(bearer)) !=
      0) {
    TM_LOG_ERR("nextmail: 内部令牌格式错误");
    return NULL;
  }

  size_t need = strlen(NM_BASE) + strlen(box_id) + 48;
  char *url = (char *)malloc(need);
  if (!url)
    return NULL;
  snprintf(url, need, "%s/temp-mail/inboxes/%s/messages", NM_BASE, box_id);
  char **h = nm_auth_headers(bearer);
  if (!h) {
    free(url);
    return NULL;
  }
  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, (const char **)h, NULL, 20);
  nm_free_headers(h);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    TM_LOG_ERR("nextmail: 获取邮件列表失败 http %ld", resp ? resp->status : -1);
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

  cJSON *rows = cJSON_GetObjectItemCaseSensitive(root, "data");
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
      size_t need2 = strlen(url) + strlen(msg_id) + 2;
      char *detail_url = (char *)malloc(need2);
      char **h2 = nm_auth_headers(bearer);
      if (detail_url && h2) {
        snprintf(detail_url, need2, "%s/%s", url, msg_id);
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
      nm_free_headers(h2);
    }

    if (detail) {
      emails[i] = nm_flatten(detail, email);
      cJSON_Delete(detail);
      continue;
    }

    cJSON *flat = cJSON_CreateObject();
    cJSON_AddStringToObject(flat, "id", msg_id);
    cJSON *from = cJSON_GetObjectItemCaseSensitive(item, "from");
    if (cJSON_IsObject(from)) {
      cJSON_AddItemToObject(
          flat, "from",
          cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(from, "email"), 1));
    }
    cJSON_AddItemToObject(
        flat, "to",
        cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(item, "to"), 1));
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
