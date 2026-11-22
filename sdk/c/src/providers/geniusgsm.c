/**
 * GeniusGSM 渠道 — https://tempmail.geniusgsm.com
 *
 * 列表 GET /api/domains/ → {results:[{id,name,is_active}]}。
 * 创建 POST /api/inboxes/create_inbox/ {domain_id,ttl} →
 *   {id,email,expires_at,...}；内部令牌即收件箱 ID。
 * 收件 GET /api/inboxes/{id}/emails/ → 数组，条目已含 body_text/body_html。
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GGSM_BASE "https://tempmail.geniusgsm.com/api"

static const char *ggsm_headers[] = {"Accept: application/json", NULL};
static const char *ggsm_post_headers[] = {"Accept: application/json",
                                         "Content-Type: application/json", NULL};

/** 选择第一个激活域名并创建收件箱 */
tm_email_info_t *tm_provider_geniusgsm_generate(void) {
  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, GGSM_BASE "/domains/", ggsm_headers, NULL, 20);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    TM_LOG_ERR("geniusgsm: 获取域名失败 http %ld", resp ? resp->status : -1);
    tm_http_response_free(resp);
    return NULL;
  }
  cJSON *domains = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!domains)
    return NULL;

  cJSON *results = cJSON_GetObjectItemCaseSensitive(domains, "results");
  if (!cJSON_IsArray(results)) {
    cJSON_Delete(domains);
    return NULL;
  }

  int domain_id = 0;
  cJSON *item = NULL;
  cJSON_ArrayForEach(item, results) {
    cJSON *active = cJSON_GetObjectItemCaseSensitive(item, "is_active");
    cJSON *id = cJSON_GetObjectItemCaseSensitive(item, "id");
    if (cJSON_IsTrue(active) && cJSON_IsNumber(id)) {
      domain_id = id->valueint;
      break;
    }
  }
  cJSON_Delete(domains);
  if (domain_id == 0) {
    TM_LOG_ERR("geniusgsm: 无可用激活域名");
    return NULL;
  }

  char body[96];
  snprintf(body, sizeof(body), "{\"domain_id\":%d,\"ttl\":3600}", domain_id);
  resp = tm_http_request(TM_HTTP_POST, GGSM_BASE "/inboxes/create_inbox/",
                         ggsm_post_headers, body, 20);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    TM_LOG_ERR("geniusgsm: 创建邮箱失败 http %ld", resp ? resp->status : -1);
    tm_http_response_free(resp);
    return NULL;
  }
  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!root)
    return NULL;

  const char *id = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "id"), "");
  const char *email =
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "email"), "");
  if (!id[0] || !email[0]) {
    TM_LOG_ERR("geniusgsm: 创建响应不完整");
    cJSON_Delete(root);
    return NULL;
  }

  tm_email_info_t *info = tm_email_info_new();
  if (!info) {
    cJSON_Delete(root);
    return NULL;
  }
  info->channel = CHANNEL_GENIUSGSM;
  info->email = tm_strdup(email);
  info->token = tm_strdup(id);
  cJSON_Delete(root);
  return info;
}

/** 列表响应已包含完整正文 */
tm_email_t *tm_provider_geniusgsm_get_emails(const char *email, const char *token,
                                             int *count) {
  *count = 0;
  if (!token || !token[0])
    return NULL;

  size_t need = strlen(GGSM_BASE) + strlen(token) + 32;
  char *url = (char *)malloc(need);
  if (!url)
    return NULL;
  snprintf(url, need, "%s/inboxes/%s/emails/", GGSM_BASE, token);
  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, ggsm_headers, NULL, 20);
  free(url);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    TM_LOG_ERR("geniusgsm: 获取邮件失败 http %ld", resp ? resp->status : -1);
    tm_http_response_free(resp);
    return NULL;
  }
  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!root)
    return NULL;

  cJSON *rows = root;
  if (!cJSON_IsArray(rows))
    rows = cJSON_GetObjectItemCaseSensitive(root, "results");
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
    const char *from_addr =
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(item, "from_email"), "");
    const char *from_name =
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(item, "from_name"), "");
    cJSON *flat = cJSON_CreateObject();
    cJSON_AddItemToObject(
        flat, "id",
        cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(item, "id"), 1));
    if (from_name[0] && from_addr[0]) {
      char composed[512];
      snprintf(composed, sizeof(composed), "%s <%s>", from_name, from_addr);
      cJSON_AddStringToObject(flat, "from", composed);
    } else {
      cJSON_AddStringToObject(flat, "from", from_addr);
    }
    cJSON *to = cJSON_GetObjectItemCaseSensitive(item, "inbox_email");
    if (!cJSON_IsString(to) || !to->valuestring[0])
      cJSON_AddStringToObject(flat, "to", email ? email : "");
    else
      cJSON_AddItemToObject(flat, "to", cJSON_Duplicate(to, 1));
    cJSON_AddItemToObject(
        flat, "subject",
        cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(item, "subject"), 1));
    cJSON_AddItemToObject(
        flat, "text",
        cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(item, "body_text"), 1));
    cJSON_AddItemToObject(
        flat, "html",
        cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(item, "body_html"), 1));
    cJSON_AddItemToObject(
        flat, "date",
        cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(item, "received_at"), 1));
    cJSON_AddItemToObject(
        flat, "read",
        cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(item, "is_read"), 1));
    cJSON_AddItemToObject(
        flat, "attachments",
        cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(item, "attachments"), 1));
    emails[i] = tm_normalize_email(flat, email ? email : "");
    cJSON_Delete(flat);
  }

  cJSON_Delete(root);
  return emails;
}
