/**
 * nimail.cn 渠道
 *
 * 简单 POST 表单接口，无需认证，token 即邮箱地址本身。
 *
 * 创建邮箱:
 *   POST https://www.nimail.cn/api/applymail
 *   Content-Type: application/x-www-form-urlencoded
 *   Body: mail=<name>@nimail.cn（前缀随机 10 位小写字母数字）
 *   响应:
 * {"delay":"10:00","tips":"","user":"xxx@nimail.cn","success":"true","time":1783855108}
 *
 * 获取邮件:
 *   POST https://www.nimail.cn/api/getmails
 *   Body: mail=<email>&time=0
 *   响应:
 * {"to":"xxx@nimail.cn","mail":[...],"success":"true","time":1783855172}
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NIMAIL_BASE "https://www.nimail.cn"
#define NIMAIL_DOMAIN "nimail.cn"
#define NIMAIL_MAX_MAILS 128

/* 公共 UA */
static const char *nimail_ua =
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36";

/* URL 编码单个字符（RFC3986 未保留字符原样保留，其余百分号编码） */
static int nimail_url_encode_char(char c, char *out) {
  static const char hex[] = "0123456789ABCDEF";
  unsigned char uc = (unsigned char)c;
  if ((uc >= 'A' && uc <= 'Z') || (uc >= 'a' && uc <= 'z') ||
      (uc >= '0' && uc <= '9') || uc == '-' || uc == '_' || uc == '.' ||
      uc == '~') {
    out[0] = c;
    return 1;
  }
  out[0] = '%';
  out[1] = hex[(uc >> 4) & 0x0F];
  out[2] = hex[uc & 0x0F];
  return 3;
}

/* URL 编码字符串（堆分配，调用者 free） */
static char *nimail_url_encode(const char *s) {
  if (!s)
    return NULL;
  size_t len = strlen(s);
  char *out = (char *)malloc(len * 3 + 1);
  if (!out)
    return NULL;
  size_t o = 0;
  for (size_t i = 0; i < len; i++) {
    o += (size_t)nimail_url_encode_char(s[i], out + o);
  }
  out[o] = '\0';
  return out;
}

/* 随机生成邮箱前缀（小写字母数字） */
static void nimail_random_local(char *out, size_t cap) {
  static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
  if (!out || cap < 2)
    return;
  size_t n = 10;
  if (n >= cap)
    n = cap - 1;
  for (size_t i = 0; i < n; i++) {
    out[i] = chars[rand() % (int)(sizeof(chars) - 1)];
  }
  out[n] = '\0';
}

/**
 * 创建 nimail.cn 临时邮箱
 */
tm_email_info_t *tm_provider_nimail_generate(void) {
  int timeout =
      tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

  char local[16];
  nimail_random_local(local, sizeof(local));
  char email_addr_buf[64];
  snprintf(email_addr_buf, sizeof(email_addr_buf), "%s@%s", local,
           NIMAIL_DOMAIN);

  char *encoded_mail = nimail_url_encode(email_addr_buf);
  if (!encoded_mail)
    return NULL;

  char post_body[128];
  snprintf(post_body, sizeof(post_body), "mail=%s", encoded_mail);
  free(encoded_mail);

  const char *hdrs[] = {
      nimail_ua,
      "Accept: application/json, text/javascript, */*; q=0.01",
      "Content-Type: application/x-www-form-urlencoded",
      "Origin: " NIMAIL_BASE,
      "Referer: " NIMAIL_BASE "/",
      NULL};

  tm_http_response_t *resp = tm_http_request(
      TM_HTTP_POST, NIMAIL_BASE "/api/applymail", hdrs, post_body, timeout);
  if (!resp || resp->status != 200 || !resp->body) {
    TM_LOG_ERR("[nimail] 申请邮箱失败, status=%ld", resp ? resp->status : 0);
    tm_http_response_free(resp);
    return NULL;
  }

  cJSON *json = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!json) {
    TM_LOG_ERR("[nimail] 创建响应非 JSON");
    return NULL;
  }

  const cJSON *j_success = cJSON_GetObjectItemCaseSensitive(json, "success");
  const cJSON *j_user = cJSON_GetObjectItemCaseSensitive(json, "user");
  if (!cJSON_IsString(j_success) ||
      strcmp(j_success->valuestring, "true") != 0 || !cJSON_IsString(j_user) ||
      !j_user->valuestring || !j_user->valuestring[0]) {
    TM_LOG_WARN("[nimail] 创建邮箱失败，无效响应");
    cJSON_Delete(json);
    return NULL;
  }

  char *user = tm_strdup(j_user->valuestring);
  cJSON_Delete(json);
  if (!user)
    return NULL;

  tm_email_info_t *info = tm_email_info_new();
  if (!info) {
    free(user);
    return NULL;
  }
  info->channel = CHANNEL_NIMAIL;
  info->email = user;
  info->token = tm_strdup(user); /* token 即邮箱地址 */
  if (!info->token) {
    tm_free_email_info(info);
    return NULL;
  }
  info->expires_at = 0;
  info->created_at = tm_strdup("");

  TM_LOG_DBG("[nimail] 创建邮箱成功: %s", info->email);
  return info;
}

/**
 * 获取 nimail.cn 邮件列表
 */
tm_email_t *tm_provider_nimail_get_emails(const char *token, const char *email,
                                          int *count) {
  *count = -1;
  const char *addr = (token && token[0]) ? token : email;
  if (!addr || !addr[0]) {
    TM_LOG_WARN("[nimail] 缺少邮箱地址");
    return NULL;
  }

  int timeout =
      tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

  char *encoded_mail = nimail_url_encode(addr);
  if (!encoded_mail)
    return NULL;

  size_t body_cap = strlen(encoded_mail) + 32;
  char *post_body = (char *)malloc(body_cap);
  if (!post_body) {
    free(encoded_mail);
    return NULL;
  }
  snprintf(post_body, body_cap, "mail=%s&time=0", encoded_mail);
  free(encoded_mail);

  const char *hdrs[] = {
      nimail_ua,
      "Accept: application/json, text/javascript, */*; q=0.01",
      "Content-Type: application/x-www-form-urlencoded",
      "Origin: " NIMAIL_BASE,
      "Referer: " NIMAIL_BASE "/",
      NULL};

  tm_http_response_t *resp = tm_http_request(
      TM_HTTP_POST, NIMAIL_BASE "/api/getmails", hdrs, post_body, timeout);
  free(post_body);
  if (!resp || resp->status != 200 || !resp->body) {
    TM_LOG_ERR("[nimail] 获取邮件失败, status=%ld", resp ? resp->status : 0);
    tm_http_response_free(resp);
    return NULL;
  }

  cJSON *json = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!json) {
    TM_LOG_ERR("[nimail] 邮件响应解析失败");
    return NULL;
  }

  /* success 非 "true" 或 mail 非数组视为暂无邮件 */
  const cJSON *j_success = cJSON_GetObjectItemCaseSensitive(json, "success");
  const cJSON *j_mail = cJSON_GetObjectItemCaseSensitive(json, "mail");
  if (!cJSON_IsString(j_success) ||
      strcmp(j_success->valuestring, "true") != 0 || !cJSON_IsArray(j_mail)) {
    cJSON_Delete(json);
    *count = 0;
    return NULL;
  }

  int n = cJSON_GetArraySize(j_mail);
  if (n == 0) {
    cJSON_Delete(json);
    *count = 0;
    return NULL;
  }
  if (n > NIMAIL_MAX_MAILS)
    n = NIMAIL_MAX_MAILS;

  tm_email_t *emails = tm_emails_new(n);
  if (!emails) {
    cJSON_Delete(json);
    return NULL;
  }

  int valid = 0;
  for (int i = 0; i < n; i++) {
    const cJSON *msg = cJSON_GetArrayItem(j_mail, i);
    if (!cJSON_IsObject(msg))
      continue;
    emails[valid] = tm_normalize_email(msg, addr);
    valid++;
  }

  cJSON_Delete(json);

  if (valid == 0) {
    free(emails);
    *count = 0;
    return NULL;
  }

  *count = valid;
  return emails;
}
