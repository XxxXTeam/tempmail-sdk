/**
 * 24mail-chacuo — http://24mail.chacuo.net
 * POST 注册/刷新同一接口。HTTP only（该站点不支持 HTTPS）。
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CHACUO_BASE "http://24mail.chacuo.net" /* lgtm[cpp/non-https-url] 该站点不支持 HTTPS */

static const char *chacuo_domains[] = {"chacuo.net", "027168.com"};
#define CHACUO_DOMAIN_COUNT 2

static const char *chacuo_hdrs[] = {
    "Accept: application/json, text/javascript, */*; q=0.01",
    "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
    "Content-Type: application/x-www-form-urlencoded; charset=UTF-8",
    "Origin: http://24mail.chacuo.net",
    "Referer: http://24mail.chacuo.net/",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    "x-requested-with: XMLHttpRequest",
    NULL};

static void chacuo_random_local(char *buf, int length) {
  static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
  for (int i = 0; i < length; i++) {
    buf[i] = chars[rand() % (int)(sizeof(chars) - 1)];
  }
  buf[length] = '\0';
}

tm_email_info_t *tm_provider_24mail_chacuo_generate(void) {
  char name[16];
  chacuo_random_local(name, 10);
  const char *domain = chacuo_domains[rand() % CHACUO_DOMAIN_COUNT];

  char body[512];
  snprintf(body, sizeof(body), "data=%s%%40%s&type=refresh&arg=", name, domain);

  tm_http_response_t *r =
      tm_http_request(TM_HTTP_POST, CHACUO_BASE "/", chacuo_hdrs, body, 15);
  if (!r || r->status < 200 || r->status >= 300) {
    tm_http_response_free(r);
    return NULL;
  }

  cJSON *cj = cJSON_Parse(r->body ? r->body : "");
  tm_http_response_free(r);
  if (!cj || !cJSON_IsObject(cj)) {
    cJSON_Delete(cj);
    return NULL;
  }

  cJSON *status = cJSON_GetObjectItemCaseSensitive(cj, "status");
  if (!status || !cJSON_IsNumber(status) || status->valueint != 1) {
    cJSON_Delete(cj);
    return NULL;
  }

  char email[128];
  snprintf(email, sizeof(email), "%s@%s", name, domain);

  tm_email_info_t *info = tm_email_info_new();
  info->channel = CHANNEL_24MAIL_CHACUO;
  info->email = tm_strdup(email);
  info->token = tm_strdup(email);

  cJSON_Delete(cj);
  return info;
}

tm_email_t *tm_provider_24mail_chacuo_get_emails(const char *email,
                                                 int *count) {
  *count = -1;
  if (!email)
    return NULL;

  /* 分割 email 为 name@domain */
  const char *at = strchr(email, '@');
  char name[128], domain[128];
  if (at && at > email) {
    size_t name_len = (size_t)(at - email);
    if (name_len >= sizeof(name))
      name_len = sizeof(name) - 1;
    memcpy(name, email, name_len);
    name[name_len] = '\0';
    strncpy(domain, at + 1, sizeof(domain) - 1);
    domain[sizeof(domain) - 1] = '\0';
  } else {
    strncpy(name, email, sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
    strncpy(domain, chacuo_domains[0], sizeof(domain) - 1);
    domain[sizeof(domain) - 1] = '\0';
  }

  char body[512];
  snprintf(body, sizeof(body), "data=%s%%40%s&type=refresh&arg=", name, domain);

  tm_http_response_t *r =
      tm_http_request(TM_HTTP_POST, CHACUO_BASE "/", chacuo_hdrs, body, 15);
  if (!r || r->status < 200 || r->status >= 300) {
    tm_http_response_free(r);
    return NULL;
  }

  cJSON *cj = cJSON_Parse(r->body ? r->body : "");
  tm_http_response_free(r);
  if (!cj || !cJSON_IsObject(cj)) {
    cJSON_Delete(cj);
    return NULL;
  }

  cJSON *status_item = cJSON_GetObjectItemCaseSensitive(cj, "status");
  if (!status_item || !cJSON_IsNumber(status_item) ||
      status_item->valueint != 1) {
    cJSON_Delete(cj);
    *count = 0;
    return NULL;
  }

  cJSON *data_arr = cJSON_GetObjectItemCaseSensitive(cj, "data");
  if (!data_arr || !cJSON_IsArray(data_arr) ||
      cJSON_GetArraySize(data_arr) == 0) {
    cJSON_Delete(cj);
    *count = 0;
    return NULL;
  }

  cJSON *entry = cJSON_GetArrayItem(data_arr, 0);
  cJSON *list = cJSON_GetObjectItemCaseSensitive(entry, "list");
  if (!list || !cJSON_IsArray(list)) {
    cJSON_Delete(cj);
    *count = 0;
    return NULL;
  }

  int n = cJSON_GetArraySize(list);
  *count = n;
  if (n == 0) {
    cJSON_Delete(cj);
    return NULL;
  }

  tm_email_t *emails = tm_emails_new(n);
  for (int i = 0; i < n; i++) {
    cJSON *item = cJSON_GetArrayItem(list, i);
    const char *mid =
        cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(item, "MID"));
    const char *from =
        cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(item, "FROM"));
    const char *subj =
        cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(item, "SUBJECT"));
    const char *content =
        cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(item, "CONTENT"));
    const char *time_str =
        cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(item, "TIME"));

    cJSON *one = cJSON_CreateObject();
    cJSON_AddStringToObject(one, "id", mid ? mid : "");
    cJSON_AddStringToObject(one, "from", from ? from : "");
    cJSON_AddStringToObject(one, "to", email);
    cJSON_AddStringToObject(one, "subject", subj ? subj : "");
    cJSON_AddStringToObject(one, "body", content ? content : "");
    cJSON_AddStringToObject(one, "date", time_str ? time_str : "");
    cJSON_AddBoolToObject(one, "isRead", 0);
    emails[i] = tm_normalize_email(one, email);
    cJSON_Delete(one);
  }

  cJSON_Delete(cj);
  return emails;
}
