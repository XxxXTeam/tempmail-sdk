/**
 * tempmails.cc 渠道 — https://tempmails.cc
 *
 * 地址即邮箱：在公开免费域名上生成地址，无需注册。
 * 域名列表 GET /api/domains → {domainInfo:[{domain}]}。
 * 列表 GET /api/inbox/{email} → {messages:[...]}。
 * 详情 GET /api/message/{id}?address={email}&mark_read=false →
 *   {from,to,subject,textBody,htmlBody,date}；详情失败回退摘要。
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TMC_BASE "https://tempmails.cc"

static const char *tmc_headers[] = {"Accept: application/json", NULL};

static void tmc_random_local(char *buf, size_t cap) {
  static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
  if (cap < 2)
    return;
  size_t n = 16;
  if (n + 1 > cap)
    n = cap - 1;
  for (size_t i = 0; i < n; i++)
    buf[i] = chars[rand() % (sizeof(chars) - 1)];
  buf[n] = '\0';
}

/** 在公开免费域名上生成地址 */
tm_email_info_t *tm_provider_tempmails_cc_generate(void) {
  char url[64];
  snprintf(url, sizeof(url), "%s/api/domains", TMC_BASE);
  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, tmc_headers, NULL, 20);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }
  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!root)
    return NULL;

  cJSON *items = cJSON_GetObjectItemCaseSensitive(root, "domainInfo");
  cJSON *first = cJSON_IsArray(items) ? cJSON_GetArrayItem(items, 0) : NULL;
  const char *domain =
      first ? TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(first, "domain"), "")
            : "";
  if (!domain[0]) {
    TM_LOG_ERR("tempmails-cc: 无可用公开域名");
    cJSON_Delete(root);
    return NULL;
  }

  char local[24];
  tmc_random_local(local, sizeof(local));
  char address[256];
  snprintf(address, sizeof(address), "%s@%s", local, domain);

  tm_email_info_t *info = tm_email_info_new();
  if (!info) {
    cJSON_Delete(root);
    return NULL;
  }
  info->channel = CHANNEL_TEMPMAILS_CC;
  info->email = tm_strdup(address);
  info->token = tm_strdup("");
  cJSON_Delete(root);
  return info;
}

/** 跳过选域，在指定固定域名上生成地址 */
tm_email_info_t *tm_provider_tempmails_cc_generate_domain(const char *domain) {
  if (!domain || !domain[0])
    return NULL;
  char local[24];
  tmc_random_local(local, sizeof(local));
  char address[256];
  snprintf(address, sizeof(address), "%s@%s", local, domain);
  tm_email_info_t *info = tm_email_info_new();
  if (!info)
    return NULL;
  info->channel = CHANNEL_TEMPMAILS_CC;
  info->email = tm_strdup(address);
  info->token = tm_strdup("");
  return info;
}

/** 读取 tempmails.cc 邮件，详情正文位于 textBody / htmlBody */
tm_email_t *tm_provider_tempmails_cc_get_emails(const char *email, int *count) {
  *count = 0;
  if (!email || !email[0])
    return NULL;

  size_t need = strlen(TMC_BASE) + strlen(email) + 32;
  char *url = (char *)malloc(need);
  if (!url)
    return NULL;
  snprintf(url, need, "%s/api/inbox/%s", TMC_BASE, email);
  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, tmc_headers, NULL, 20);
  free(url);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    TM_LOG_ERR("tempmails-cc: 获取邮件列表失败 http %ld",
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
      size_t need2 = strlen(TMC_BASE) + strlen(msg_id) + strlen(email) + 64;
      char *detail_url = (char *)malloc(need2);
      if (detail_url) {
        snprintf(detail_url, need2,
                 "%s/api/message/%s?address=%s&mark_read=false", TMC_BASE,
                 msg_id, email);
        tm_http_response_t *detail_resp =
            tm_http_request(TM_HTTP_GET, detail_url, tmc_headers, NULL, 20);
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
      cJSON_AddItemToObject(
          flat, "from",
          cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(detail, "from"), 1));
      cJSON *to = cJSON_GetObjectItemCaseSensitive(detail, "to");
      if (cJSON_IsArray(to) && cJSON_GetArraySize(to) > 0)
        to = cJSON_GetArrayItem(to, 0);
      cJSON_AddItemToObject(flat, "to", cJSON_Duplicate(to, 1));
      cJSON_AddItemToObject(
          flat, "subject",
          cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(detail, "subject"),
                          1));
      cJSON_AddItemToObject(
          flat, "text",
          cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(detail, "textBody"),
                          1));
      cJSON_AddItemToObject(
          flat, "html",
          cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(detail, "htmlBody"),
                          1));
      cJSON_AddItemToObject(
          flat, "date",
          cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(detail, "date"), 1));
      cJSON_AddItemToObject(
          flat, "seen",
          cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(detail, "isRead"),
                          1));
      emails[i] = tm_normalize_email(flat, email);
      cJSON_Delete(flat);
      cJSON_Delete(detail);
      continue;
    }
    cJSON_Delete(detail);

    cJSON *flat = cJSON_CreateObject();
    cJSON_AddStringToObject(flat, "id", msg_id);
    cJSON_AddItemToObject(
        flat, "from",
        cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(item, "from"), 1));
    cJSON_AddStringToObject(flat, "to", email);
    cJSON_AddItemToObject(
        flat, "subject",
        cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(item, "subject"), 1));
    cJSON_AddItemToObject(
        flat, "date",
        cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(item, "date"), 1));
    cJSON_AddItemToObject(
        flat, "seen",
        cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(item, "isRead"), 1));
    emails[i] = tm_normalize_email(flat, email);
    cJSON_Delete(flat);
  }

  cJSON_Delete(root);
  return emails;
}
