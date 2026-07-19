/**
 * ten-minute-mail-net 渠道 — https://10minutemail.net
 *
 * PHP 会话模式：首次访问 /address.api.php 通过 Set-Cookie 分配会话，
 * 返回邮箱地址。后续带 Cookie 请求同一接口获取邮件列表。
 * token 存储 PHP session cookie。
 */

#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEN_MIN_MAIL_NET_BASE "https://10minutemail.net"

static const char *ten_min_headers[] = {
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
    "Accept: application/json",
    NULL};

/* 从 Set-Cookie 头中提取 PHPSESSID 值 */
static int ten_min_extract_session(const char *cookies, char *out,
                                   size_t out_cap) {
  if (!cookies)
    return -1;
  const char *p = strstr(cookies, "PHPSESSID=");
  if (!p)
    return -1;
  p += 10; /* strlen("PHPSESSID=") */
  const char *end = p;
  while (*end && *end != ';' && *end != ' ')
    end++;
  size_t len = (size_t)(end - p);
  if (len == 0 || len >= out_cap)
    return -1;
  memcpy(out, p, len);
  out[len] = '\0';
  return 0;
}

/**
 * 创建 10minutemail.net 临时邮箱
 */
tm_email_info_t *tm_provider_ten_minute_mail_net_generate(void) {
  char url[128];
  snprintf(url, sizeof(url), "%s/address.api.php", TEN_MIN_MAIL_NET_BASE);

  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, ten_min_headers, NULL, 15);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    TM_LOG_ERR("ten-minute-mail-net: 获取邮箱失败");
    tm_http_response_free(resp);
    return NULL;
  }

  /* 提取 session cookie */
  char session_id[128];
  if (ten_min_extract_session(resp->cookies, session_id, sizeof(session_id)) <
      0) {
    TM_LOG_ERR("ten-minute-mail-net: 未找到 PHPSESSID");
    tm_http_response_free(resp);
    return NULL;
  }

  /* 解析响应 JSON 获取邮箱地址 */
  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!root) {
    TM_LOG_ERR("ten-minute-mail-net: 解析响应 JSON 失败");
    return NULL;
  }

  const char *mail_addr =
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "mail_get_mail"), "");
  if (!mail_addr[0]) {
    /* 尝试备选字段 */
    mail_addr = TM_JSON_STR(
        cJSON_GetObjectItemCaseSensitive(root, "mail_get_user"), "");
  }

  if (!mail_addr[0]) {
    TM_LOG_ERR("ten-minute-mail-net: 响应中无邮箱地址");
    cJSON_Delete(root);
    return NULL;
  }

  tm_email_info_t *info = tm_email_info_new();
  if (!info) {
    cJSON_Delete(root);
    return NULL;
  }

  info->email = tm_strdup(mail_addr);
  info->token = tm_strdup(session_id);
  cJSON_Delete(root);
  return info;
}

/**
 * 获取 10minutemail.net 邮件列表
 */
tm_email_t *tm_provider_ten_minute_mail_net_get_emails(const char *token,
                                                       const char *email,
                                                       int *count) {
  *count = 0;
  if (!token || !token[0])
    return NULL;

  /* 构造带 Cookie 的请求 */
  char cookie_hdr[256];
  snprintf(cookie_hdr, sizeof(cookie_hdr), "Cookie: PHPSESSID=%s", token);

  const char *headers[] = {
      "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
      "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
      "Accept: application/json", cookie_hdr, NULL};

  char url[128];
  snprintf(url, sizeof(url), "%s/address.api.php", TEN_MIN_MAIL_NET_BASE);

  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, headers, NULL, 15);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }

  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!root)
    return NULL;

  /* 邮件列表在 mail_get_mail_list 字段 */
  cJSON *mail_list =
      cJSON_GetObjectItemCaseSensitive(root, "mail_get_mail_list");
  if (!cJSON_IsArray(mail_list) || cJSON_GetArraySize(mail_list) == 0) {
    cJSON_Delete(root);
    return NULL;
  }

  int n = cJSON_GetArraySize(mail_list);
  *count = n;
  tm_email_t *emails = tm_emails_new(n);
  if (!emails) {
    *count = -1;
    cJSON_Delete(root);
    return NULL;
  }

  for (int i = 0; i < n; i++) {
    cJSON *msg = cJSON_GetArrayItem(mail_list, i);
    cJSON *raw = cJSON_CreateObject();
    cJSON_AddStringToObject(
        raw, "id",
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(msg, "mail_id"), ""));
    cJSON_AddStringToObject(
        raw, "from",
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(msg, "mail_from"), ""));
    cJSON_AddStringToObject(raw, "to", email ? email : "");
    cJSON_AddStringToObject(
        raw, "subject",
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(msg, "mail_subject"), ""));
    cJSON_AddStringToObject(
        raw, "text",
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(msg, "mail_text"), ""));
    cJSON_AddStringToObject(
        raw, "html",
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(msg, "mail_html"), ""));
    cJSON_AddStringToObject(
        raw, "date",
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(msg, "mail_timestamp"),
                    ""));

    emails[i] = tm_normalize_email(raw, email);
    cJSON_Delete(raw);
  }

  cJSON_Delete(root);
  return emails;
}
