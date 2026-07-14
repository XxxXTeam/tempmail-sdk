/**
 * neighbours-sh — https://neighbours.sh
 * 公共收件箱模式，任意 @neighbours.sh 地址直接收信
 *
 * 获取邮件列表: GET /api/v1/inbox/{address}
 * 获取邮件详情: GET /api/v1/inbox/{address}/{uid}
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NEIGHBOURS_BASE "https://neighbours.sh/api/v1"
#define NEIGHBOURS_DOMAIN "neighbours.sh"

static const char *nb_headers[] = {"Accept: application/json", NULL};

static void nb_random_username(char *buf, size_t len) {
  static const char charset[] = "abcdefghijklmnopqrstuvwxyz0123456789";
  srand((unsigned)time(NULL));
  /* 前缀 sdk */
  buf[0] = 's';
  buf[1] = 'd';
  buf[2] = 'k';
  for (size_t i = 3; i < len; i++) {
    buf[i] = charset[rand() % (sizeof(charset) - 1)];
  }
  buf[len] = '\0';
}

tm_email_info_t *tm_provider_neighbours_sh_generate(void) {
  char username[14]; /* sdk + 10 + \0 */
  nb_random_username(username, 13);

  char email[64];
  snprintf(email, sizeof(email), "%s@%s", username, NEIGHBOURS_DOMAIN);

  tm_email_info_t *info = tm_email_info_new();
  info->channel = CHANNEL_NEIGHBOURS_SH;
  info->email = tm_strdup(email);
  info->token = tm_strdup(email);
  return info;
}

tm_email_t *tm_provider_neighbours_sh_get_emails(const char *token,
                                                 const char *email,
                                                 int *count) {
  *count = -1;
  const char *addr = (token && token[0]) ? token : email;
  if (!addr || !addr[0])
    return NULL;

  /* 获取邮件列表 */
  char list_url[512];
  snprintf(list_url, sizeof(list_url), NEIGHBOURS_BASE "/inbox/%s", addr);

  tm_http_response_t *list_resp =
      tm_http_request(TM_HTTP_GET, list_url, nb_headers, NULL, 15);
  if (!list_resp || list_resp->status != 200) {
    tm_http_response_free(list_resp);
    *count = 0;
    return NULL;
  }

  cJSON *list_root = cJSON_Parse(list_resp->body);
  tm_http_response_free(list_resp);
  if (!list_root) {
    *count = 0;
    return NULL;
  }

  cJSON *data = cJSON_GetObjectItemCaseSensitive(list_root, "data");
  int n = cJSON_IsArray(data) ? cJSON_GetArraySize(data) : 0;
  *count = n;
  if (n == 0) {
    cJSON_Delete(list_root);
    return NULL;
  }

  tm_email_t *out = tm_emails_new(n);
  for (int i = 0; i < n; i++) {
    cJSON *row = cJSON_GetArrayItem(data, i);
    cJSON *uid_item = cJSON_GetObjectItemCaseSensitive(row, "uid");
    if (!cJSON_IsNumber(uid_item)) {
      cJSON *empty = cJSON_CreateObject();
      out[i] = tm_normalize_email(empty, email);
      cJSON_Delete(empty);
      continue;
    }

    /* 获取单封邮件详情 */
    int uid = uid_item->valueint;
    char detail_url[512];
    snprintf(detail_url, sizeof(detail_url), NEIGHBOURS_BASE "/inbox/%s/%d",
             addr, uid);

    tm_http_response_t *detail_resp =
        tm_http_request(TM_HTTP_GET, detail_url, nb_headers, NULL, 15);
    if (!detail_resp || detail_resp->status != 200) {
      tm_http_response_free(detail_resp);
      cJSON *empty = cJSON_CreateObject();
      out[i] = tm_normalize_email(empty, email);
      cJSON_Delete(empty);
      continue;
    }

    cJSON *detail_root = cJSON_Parse(detail_resp->body);
    tm_http_response_free(detail_resp);
    if (!detail_root) {
      cJSON *empty = cJSON_CreateObject();
      out[i] = tm_normalize_email(empty, email);
      cJSON_Delete(empty);
      continue;
    }

    cJSON *detail_data =
        cJSON_GetObjectItemCaseSensitive(detail_root, "data");

    /* 构建标准化中间对象 */
    cJSON *flat = cJSON_CreateObject();

    /* id */
    char uid_str[32];
    snprintf(uid_str, sizeof(uid_str), "%d", uid);
    cJSON_AddStringToObject(flat, "id", uid_str);

    /* from：从 data.from.value[0].address 或 data.from.text 取 */
    const char *from_str = "";
    cJSON *from_obj = cJSON_GetObjectItemCaseSensitive(detail_data, "from");
    if (cJSON_IsObject(from_obj)) {
      cJSON *from_val = cJSON_GetObjectItemCaseSensitive(from_obj, "value");
      if (cJSON_IsArray(from_val) && cJSON_GetArraySize(from_val) > 0) {
        cJSON *first = cJSON_GetArrayItem(from_val, 0);
        const char *a = cJSON_GetStringValue(
            cJSON_GetObjectItemCaseSensitive(first, "address"));
        if (a && a[0])
          from_str = a;
      }
      if (!from_str[0]) {
        const char *ft =
            cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(from_obj, "text"));
        if (ft)
          from_str = ft;
      }
    }
    cJSON_AddStringToObject(flat, "from", from_str);

    /* to */
    cJSON *to_obj = cJSON_GetObjectItemCaseSensitive(detail_data, "to");
    const char *to_str = email;
    if (cJSON_IsObject(to_obj)) {
      const char *tt =
          cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(to_obj, "text"));
      if (tt && tt[0])
        to_str = tt;
    }
    cJSON_AddStringToObject(flat, "to", to_str);

    /* subject, text, html, date */
    const char *subj = cJSON_GetStringValue(
        cJSON_GetObjectItemCaseSensitive(detail_data, "subject"));
    const char *text = cJSON_GetStringValue(
        cJSON_GetObjectItemCaseSensitive(detail_data, "text"));
    const char *html = cJSON_GetStringValue(
        cJSON_GetObjectItemCaseSensitive(detail_data, "html"));
    const char *date = cJSON_GetStringValue(
        cJSON_GetObjectItemCaseSensitive(detail_data, "date"));

    cJSON_AddStringToObject(flat, "subject", subj ? subj : "");
    cJSON_AddStringToObject(flat, "text", text ? text : "");
    cJSON_AddStringToObject(flat, "html", html ? html : "");
    cJSON_AddStringToObject(flat, "date", date ? date : "");

    out[i] = tm_normalize_email(flat, email);
    cJSON_Delete(flat);
    cJSON_Delete(detail_root);
  }

  cJSON_Delete(list_root);
  return out;
}
