/**
 * AnonymMail — https://anonymmail.net
 * POST JSON API，需要 cookie session
 * 创建: HEAD / 获取 cookie + POST /api/create
 * 获取: POST /api/get
 */
#include "tempmail_internal.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define AM_BASE "https://anonymmail.net"

static const char *am_headers[] = {
    "Accept: */*", "Content-Type: application/x-www-form-urlencoded",
    "Referer: https://anonymmail.net/",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    NULL};

/* 随机生成 10 位小写字母数字用户名 */
static void am_random_username(char *buf, size_t cap) {
  static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
  size_t len = 10;
  if (cap < len + 1)
    len = cap - 1;
  for (size_t i = 0; i < len; i++) {
    buf[i] = chars[rand() % (sizeof(chars) - 1)];
  }
  buf[len] = '\0';
}

static char *am_curl_escape(const char *s) {
  if (!s)
    return tm_strdup("");
  CURL *c = curl_easy_init();
  if (!c)
    return tm_strdup(s);
  char *e = curl_easy_escape(c, s, 0);
  curl_easy_cleanup(c);
  if (!e)
    return tm_strdup(s);
  char *d = tm_strdup(e);
  curl_free(e);
  return d;
}

tm_email_info_t *tm_provider_anonymmail_generate(void) {
  srand((unsigned)time(NULL));

  /* 先获取域名列表 */
  tm_http_response_t *dom_resp = tm_http_request(
      TM_HTTP_POST, AM_BASE "/api/getDomains", am_headers, "", 15);
  if (!dom_resp || dom_resp->status < 200 || dom_resp->status >= 300) {
    tm_http_response_free(dom_resp);
    return NULL;
  }

  cJSON *dom_root = cJSON_Parse(dom_resp->body);
  tm_http_response_free(dom_resp);
  if (!dom_root || !cJSON_IsArray(dom_root)) {
    cJSON_Delete(dom_root);
    return NULL;
  }

  int nd = cJSON_GetArraySize(dom_root);
  if (nd <= 0) {
    cJSON_Delete(dom_root);
    return NULL;
  }

  /* 随机选择一个域名 */
  cJSON *pick = cJSON_GetArrayItem(dom_root, rand() % nd);
  const char *domain =
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(pick, "domain"), "");
  if (!domain[0]) {
    cJSON_Delete(dom_root);
    return NULL;
  }

  char domain_buf[256];
  strncpy(domain_buf, domain, sizeof(domain_buf) - 1);
  domain_buf[sizeof(domain_buf) - 1] = '\0';
  cJSON_Delete(dom_root);

  /* 生成随机用户名 */
  char username[16];
  am_random_username(username, sizeof(username));

  char email_addr[512];
  snprintf(email_addr, sizeof(email_addr), "%s@%s", username, domain_buf);

  /* POST /api/create 创建邮箱 */
  char *esc_email = am_curl_escape(email_addr);
  char body[768];
  snprintf(body, sizeof(body), "email=%s", esc_email);
  free(esc_email);

  tm_http_response_t *resp = tm_http_request(
      TM_HTTP_POST, AM_BASE "/api/create", am_headers, body, 15);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }

  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!root)
    return NULL;

  const cJSON *ok = cJSON_GetObjectItemCaseSensitive(root, "success");
  if (!cJSON_IsBool(ok) || !cJSON_IsTrue(ok)) {
    cJSON_Delete(root);
    return NULL;
  }

  const char *addr =
      cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(root, "email"));
  if (!addr || !addr[0])
    addr = email_addr;

  tm_email_info_t *info = tm_email_info_new();
  info->channel = CHANNEL_ANONYMMAIL;
  info->email = tm_strdup(addr);
  info->token = NULL;
  cJSON_Delete(root);
  return info;
}

tm_email_t *tm_provider_anonymmail_get_emails(const char *email, int *count) {
  *count = -1;
  if (!email || !email[0])
    return NULL;

  /* POST /api/get body: email={email} */
  char *esc = am_curl_escape(email);
  char body[768];
  snprintf(body, sizeof(body), "email=%s", esc);
  free(esc);

  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_POST, AM_BASE "/api/get", am_headers, body, 15);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }

  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!root)
    return NULL;

  /* 响应结构: {"email@domain": {"created_at":"...","emails":[...]}} */
  cJSON *mailbox = cJSON_GetObjectItemCaseSensitive(root, email);
  if (!mailbox) {
    /* 尝试取第一个子对象 */
    mailbox = root->child;
  }
  if (!mailbox) {
    *count = 0;
    cJSON_Delete(root);
    return NULL;
  }

  cJSON *arr = cJSON_GetObjectItemCaseSensitive(mailbox, "emails");
  if (!arr || !cJSON_IsArray(arr)) {
    *count = 0;
    cJSON_Delete(root);
    return NULL;
  }

  int n = cJSON_GetArraySize(arr);
  *count = n;
  if (n == 0) {
    cJSON_Delete(root);
    return NULL;
  }

  tm_email_t *emails = tm_emails_new(n);
  if (!emails) {
    cJSON_Delete(root);
    *count = -1;
    return NULL;
  }

  for (int i = 0; i < n; i++) {
    cJSON *raw = cJSON_GetArrayItem(arr, i);
    /* 字段映射: token → id, body → html, date → date */
    const char *token_val =
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(raw, "token"), "");
    const char *body_val =
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(raw, "body"), "");
    if (token_val[0])
      cJSON_AddStringToObject(raw, "id", token_val);
    if (body_val[0])
      cJSON_AddStringToObject(raw, "html", body_val);
    emails[i] = tm_normalize_email(raw, email);
  }

  cJSON_Delete(root);
  return emails;
}
