/**
 * mailmomy.com 渠道
 *
 * 免注册、无鉴权、无 CAPTCHA 的纯 GET JSON 临时邮箱，token 即邮箱地址本身。
 *
 * 域名池（可选拉取）:
 *   GET https://mailmomy.com/api/domains/active
 *   返回 JSON 字符串数组，如 ["mailmomy.com","282mail.com",...]
 *   从中随机选一个域名；请求失败或为空时回退到默认 "mailmomy.com"。
 *
 * 创建邮箱:
 *   本地随机 10 位 [a-z0-9] local part，email = local@<域名>，token = email。
 *
 * 获取邮件:
 *   GET https://mailmomy.com/api/mail/messages?to=<email>&page=1&limit=20
 *   返回:
 * {"emails":[{"id","recipient","from","subject","message","bodyText","receivedAt"}],
 * ...}
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAILMOMY_BASE "https://mailmomy.com"
#define MAILMOMY_DEFAULT_DOMAIN "mailmomy.com"
#define MAILMOMY_MAX_MAILS 128

/* 公共 UA */
static const char *mailmomy_ua =
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36";

/* URL 编码单个字符（RFC3986 未保留字符原样保留，其余百分号编码） */
static int mailmomy_url_encode_char(char c, char *out) {
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
static char *mailmomy_url_encode(const char *s) {
  if (!s)
    return NULL;
  size_t len = strlen(s);
  char *out = (char *)malloc(len * 3 + 1);
  if (!out)
    return NULL;
  size_t o = 0;
  for (size_t i = 0; i < len; i++) {
    o += (size_t)mailmomy_url_encode_char(s[i], out + o);
  }
  out[o] = '\0';
  return out;
}

/* 随机生成邮箱前缀（小写字母数字） */
static void mailmomy_random_local(char *out, size_t cap) {
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
 * 拉取域名池并随机挑选一个域名（堆分配，调用者 free）。
 * 请求失败、非数组或为空时回退到默认 "mailmomy.com"。
 */
static char *mailmomy_pick_domain(int timeout) {
  const char *hdrs[] = {mailmomy_ua, "Accept: application/json", NULL};

  tm_http_response_t *resp = tm_http_request(
      TM_HTTP_GET, MAILMOMY_BASE "/api/domains/active", hdrs, NULL, timeout);
  if (!resp || resp->status != 200 || !resp->body) {
    TM_LOG_WARN("[mailmomy] 获取域名列表失败, status=%ld，回退默认域名",
                resp ? resp->status : 0);
    tm_http_response_free(resp);
    return tm_strdup(MAILMOMY_DEFAULT_DOMAIN);
  }

  cJSON *json = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!json || !cJSON_IsArray(json)) {
    TM_LOG_WARN("[mailmomy] 域名列表非数组，回退默认域名");
    if (json)
      cJSON_Delete(json);
    return tm_strdup(MAILMOMY_DEFAULT_DOMAIN);
  }

  int n = cJSON_GetArraySize(json);
  if (n <= 0) {
    cJSON_Delete(json);
    return tm_strdup(MAILMOMY_DEFAULT_DOMAIN);
  }

  /* 收集有效（非空字符串）域名下标 */
  int *pool = (int *)malloc((size_t)n * sizeof(int));
  if (!pool) {
    cJSON_Delete(json);
    return tm_strdup(MAILMOMY_DEFAULT_DOMAIN);
  }

  int pool_n = 0;
  for (int i = 0; i < n; i++) {
    const cJSON *d = cJSON_GetArrayItem(json, i);
    if (cJSON_IsString(d) && d->valuestring && d->valuestring[0]) {
      pool[pool_n++] = i;
    }
  }

  if (pool_n == 0) {
    free(pool);
    cJSON_Delete(json);
    return tm_strdup(MAILMOMY_DEFAULT_DOMAIN);
  }

  int picked = pool[rand() % pool_n];
  free(pool);

  const cJSON *chosen = cJSON_GetArrayItem(json, picked);
  char *domain = tm_strdup(chosen->valuestring);
  cJSON_Delete(json);
  return domain ? domain : tm_strdup(MAILMOMY_DEFAULT_DOMAIN);
}

/**
 * 创建 mailmomy.com 临时邮箱
 */
tm_email_info_t *tm_provider_mailmomy_generate(void) {
  int timeout =
      tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

  char *domain = mailmomy_pick_domain(timeout);
  if (!domain)
    return NULL;

  char local[16];
  mailmomy_random_local(local, sizeof(local));

  size_t email_cap = strlen(local) + strlen(domain) + 2;
  char *email_addr = (char *)malloc(email_cap);
  if (!email_addr) {
    free(domain);
    return NULL;
  }
  snprintf(email_addr, email_cap, "%s@%s", local, domain);
  free(domain);

  tm_email_info_t *info = tm_email_info_new();
  if (!info) {
    free(email_addr);
    return NULL;
  }
  info->channel = CHANNEL_MAILMOMY;
  info->email = email_addr;
  info->token = tm_strdup(email_addr); /* token 即邮箱地址 */
  if (!info->token) {
    tm_free_email_info(info);
    return NULL;
  }
  info->expires_at = 0;
  info->created_at = tm_strdup("");

  TM_LOG_DBG("[mailmomy] 创建邮箱成功: %s", info->email);
  return info;
}

/**
 * 获取 mailmomy.com 邮件列表
 */
tm_email_t *tm_provider_mailmomy_get_emails(const char *token,
                                            const char *email, int *count) {
  *count = -1;
  const char *addr = (token && token[0]) ? token : email;
  if (!addr || !addr[0]) {
    TM_LOG_WARN("[mailmomy] 缺少邮箱地址");
    return NULL;
  }

  int timeout =
      tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

  char *enc_addr = mailmomy_url_encode(addr);
  if (!enc_addr)
    return NULL;

  size_t url_cap = strlen(MAILMOMY_BASE) + strlen(enc_addr) + 64;
  char *url = (char *)malloc(url_cap);
  if (!url) {
    free(enc_addr);
    return NULL;
  }
  snprintf(url, url_cap,
           MAILMOMY_BASE "/api/mail/messages?to=%s&page=1&limit=20", enc_addr);
  free(enc_addr);

  const char *hdrs[] = {mailmomy_ua, "Accept: application/json", NULL};

  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, hdrs, NULL, timeout);
  free(url);
  if (!resp || resp->status != 200 || !resp->body) {
    TM_LOG_ERR("[mailmomy] 获取邮件失败, status=%ld", resp ? resp->status : 0);
    tm_http_response_free(resp);
    return NULL;
  }

  cJSON *json = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!json) {
    TM_LOG_ERR("[mailmomy] 邮件响应解析失败");
    return NULL;
  }

  const cJSON *j_emails = cJSON_GetObjectItemCaseSensitive(json, "emails");
  if (!cJSON_IsArray(j_emails)) {
    cJSON_Delete(json);
    *count = 0;
    return NULL;
  }

  int n = cJSON_GetArraySize(j_emails);
  if (n == 0) {
    cJSON_Delete(json);
    *count = 0;
    return NULL;
  }
  if (n > MAILMOMY_MAX_MAILS)
    n = MAILMOMY_MAX_MAILS;

  tm_email_t *emails = tm_emails_new(n);
  if (!emails) {
    cJSON_Delete(json);
    return NULL;
  }

  int valid = 0;
  for (int i = 0; i < n; i++) {
    const cJSON *msg = cJSON_GetArrayItem(j_emails, i);
    if (!cJSON_IsObject(msg))
      continue;

    /*
     * mailmomy 字段名与 normalize 候选 key 不完全一致：
     *   message -> html，bodyText -> text（bodyText 缺失时退回 message 作正文）
     * 构造一个规整对象供 tm_normalize_email 消费；id/from/subject/receivedAt
     * 直接沿用。
     */
    const cJSON *j_id = cJSON_GetObjectItemCaseSensitive(msg, "id");
    const cJSON *j_from = cJSON_GetObjectItemCaseSensitive(msg, "from");
    const cJSON *j_recipient =
        cJSON_GetObjectItemCaseSensitive(msg, "recipient");
    const cJSON *j_subject = cJSON_GetObjectItemCaseSensitive(msg, "subject");
    const cJSON *j_message = cJSON_GetObjectItemCaseSensitive(msg, "message");
    const cJSON *j_bodytext = cJSON_GetObjectItemCaseSensitive(msg, "bodyText");
    const cJSON *j_received =
        cJSON_GetObjectItemCaseSensitive(msg, "receivedAt");

    cJSON *mapped = cJSON_CreateObject();
    if (!mapped)
      continue;
    if (cJSON_IsString(j_id) && j_id->valuestring)
      cJSON_AddStringToObject(mapped, "id", j_id->valuestring);
    if (cJSON_IsString(j_from) && j_from->valuestring)
      cJSON_AddStringToObject(mapped, "from", j_from->valuestring);
    if (cJSON_IsString(j_recipient) && j_recipient->valuestring)
      cJSON_AddStringToObject(mapped, "to", j_recipient->valuestring);
    if (cJSON_IsString(j_subject) && j_subject->valuestring)
      cJSON_AddStringToObject(mapped, "subject", j_subject->valuestring);
    if (cJSON_IsString(j_message) && j_message->valuestring)
      cJSON_AddStringToObject(mapped, "html", j_message->valuestring);
    if (cJSON_IsString(j_bodytext) && j_bodytext->valuestring &&
        j_bodytext->valuestring[0])
      cJSON_AddStringToObject(mapped, "text", j_bodytext->valuestring);
    else if (cJSON_IsString(j_message) && j_message->valuestring)
      cJSON_AddStringToObject(mapped, "text", j_message->valuestring);
    if (cJSON_IsString(j_received) && j_received->valuestring)
      cJSON_AddStringToObject(mapped, "receivedAt", j_received->valuestring);

    emails[valid] = tm_normalize_email(mapped, addr);
    cJSON_Delete(mapped);
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
