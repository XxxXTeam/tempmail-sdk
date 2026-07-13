/**
 * dropmail.click 渠道
 *
 * 免注册、无鉴权的临时邮箱，token 即邮箱地址本身。
 *
 * 创建邮箱:
 *   POST https://dropmail.click/api/v1/public/mailbox
 *   返回: {"address","created_at","expires_at"}，邮箱有效期约 10 分钟。
 *
 * 获取邮件:
 *   GET https://dropmail.click/api/v1/public/mailbox/<email>
 *   返回: {"messages":[{"id","address","from","subject","text","html"}]}
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DROPMAIL_CLICK_BASE "https://dropmail.click"
#define DROPMAIL_CLICK_MAX_MAILS 128

/* 公共 UA */
static const char *dropmail_click_ua =
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36";

/* URL 编码单个字符（RFC3986 未保留字符原样保留，其余百分号编码） */
static int dropmail_click_url_encode_char(char c, char *out) {
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
static char *dropmail_click_url_encode(const char *s) {
  if (!s)
    return NULL;
  size_t len = strlen(s);
  char *out = (char *)malloc(len * 3 + 1);
  if (!out)
    return NULL;
  size_t o = 0;
  for (size_t i = 0; i < len; i++) {
    o += (size_t)dropmail_click_url_encode_char(s[i], out + o);
  }
  out[o] = '\0';
  return out;
}

/**
 * 创建 dropmail.click 临时邮箱
 */
tm_email_info_t *tm_provider_dropmail_click_generate(void) {
  int timeout =
      tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

  const char *hdrs[] = {dropmail_click_ua, "Accept: application/json", NULL};

  tm_http_response_t *resp = tm_http_request(
      TM_HTTP_POST, DROPMAIL_CLICK_BASE "/api/v1/public/mailbox", hdrs, "",
      timeout);
  if (!resp || resp->status < 200 || resp->status >= 300 || !resp->body) {
    TM_LOG_ERR("[dropmail-click] 创建邮箱失败, status=%ld",
               resp ? resp->status : 0);
    tm_http_response_free(resp);
    return NULL;
  }

  cJSON *json = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!json) {
    TM_LOG_ERR("[dropmail-click] 创建响应解析失败");
    return NULL;
  }

  const cJSON *j_address = cJSON_GetObjectItemCaseSensitive(json, "address");
  if (!cJSON_IsString(j_address) || !j_address->valuestring ||
      !j_address->valuestring[0]) {
    TM_LOG_ERR("[dropmail-click] 无效响应，缺少 address");
    cJSON_Delete(json);
    return NULL;
  }

  tm_email_info_t *info = tm_email_info_new();
  if (!info) {
    cJSON_Delete(json);
    return NULL;
  }
  info->channel = CHANNEL_DROPMAIL_CLICK;
  info->email = tm_strdup(j_address->valuestring);
  info->token = tm_strdup(j_address->valuestring); /* token 即邮箱地址 */
  if (!info->email || !info->token) {
    tm_free_email_info(info);
    cJSON_Delete(json);
    return NULL;
  }

  info->created_at = tm_strdup("");
  info->expires_at = 0;

  cJSON_Delete(json);

  TM_LOG_DBG("[dropmail-click] 创建邮箱成功: %s", info->email);
  return info;
}

/**
 * 获取 dropmail.click 邮件列表
 */
tm_email_t *tm_provider_dropmail_click_get_emails(const char *token,
                                                  const char *email,
                                                  int *count) {
  *count = -1;
  const char *addr = (token && token[0]) ? token : email;
  if (!addr || !addr[0]) {
    TM_LOG_WARN("[dropmail-click] 缺少邮箱地址");
    return NULL;
  }

  int timeout =
      tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

  char *enc_addr = dropmail_click_url_encode(addr);
  if (!enc_addr)
    return NULL;

  size_t url_cap = strlen(DROPMAIL_CLICK_BASE) + strlen(enc_addr) + 64;
  char *url = (char *)malloc(url_cap);
  if (!url) {
    free(enc_addr);
    return NULL;
  }
  snprintf(url, url_cap, DROPMAIL_CLICK_BASE "/api/v1/public/mailbox/%s",
           enc_addr);
  free(enc_addr);

  const char *hdrs[] = {dropmail_click_ua, "Accept: application/json", NULL};

  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, hdrs, NULL, timeout);
  free(url);
  if (!resp || resp->status != 200 || !resp->body) {
    TM_LOG_ERR("[dropmail-click] 获取邮件失败, status=%ld",
               resp ? resp->status : 0);
    tm_http_response_free(resp);
    return NULL;
  }

  cJSON *json = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!json) {
    TM_LOG_ERR("[dropmail-click] 邮件响应解析失败");
    return NULL;
  }

  const cJSON *j_messages = cJSON_GetObjectItemCaseSensitive(json, "messages");
  if (!cJSON_IsArray(j_messages)) {
    cJSON_Delete(json);
    *count = 0;
    return NULL;
  }

  int n = cJSON_GetArraySize(j_messages);
  if (n == 0) {
    cJSON_Delete(json);
    *count = 0;
    return NULL;
  }
  if (n > DROPMAIL_CLICK_MAX_MAILS)
    n = DROPMAIL_CLICK_MAX_MAILS;

  tm_email_t *emails = tm_emails_new(n);
  if (!emails) {
    cJSON_Delete(json);
    return NULL;
  }

  int valid = 0;
  for (int i = 0; i < n; i++) {
    const cJSON *msg = cJSON_GetArrayItem(j_messages, i);
    if (!cJSON_IsObject(msg))
      continue;

    /*
     * 构造规整对象供 tm_normalize_email 消费：
     *   id/from/subject/text/html 直接沿用，to 取 address（缺失时回退收件地址）
     */
    const cJSON *j_id = cJSON_GetObjectItemCaseSensitive(msg, "id");
    const cJSON *j_from = cJSON_GetObjectItemCaseSensitive(msg, "from");
    const cJSON *j_addr = cJSON_GetObjectItemCaseSensitive(msg, "address");
    const cJSON *j_subject = cJSON_GetObjectItemCaseSensitive(msg, "subject");
    const cJSON *j_text = cJSON_GetObjectItemCaseSensitive(msg, "text");
    const cJSON *j_html = cJSON_GetObjectItemCaseSensitive(msg, "html");
    const cJSON *j_received =
        cJSON_GetObjectItemCaseSensitive(msg, "received_at");
    const cJSON *j_date = cJSON_GetObjectItemCaseSensitive(msg, "date");

    cJSON *mapped = cJSON_CreateObject();
    if (!mapped)
      continue;
    if (cJSON_IsString(j_id) && j_id->valuestring)
      cJSON_AddStringToObject(mapped, "id", j_id->valuestring);
    else if (cJSON_IsNumber(j_id))
      cJSON_AddNumberToObject(mapped, "id", j_id->valuedouble);
    if (cJSON_IsString(j_from) && j_from->valuestring)
      cJSON_AddStringToObject(mapped, "from", j_from->valuestring);
    if (cJSON_IsString(j_addr) && j_addr->valuestring && j_addr->valuestring[0])
      cJSON_AddStringToObject(mapped, "to", j_addr->valuestring);
    if (cJSON_IsString(j_subject) && j_subject->valuestring)
      cJSON_AddStringToObject(mapped, "subject", j_subject->valuestring);
    if (cJSON_IsString(j_text) && j_text->valuestring)
      cJSON_AddStringToObject(mapped, "text", j_text->valuestring);
    if (cJSON_IsString(j_html) && j_html->valuestring)
      cJSON_AddStringToObject(mapped, "html", j_html->valuestring);
    if (cJSON_IsString(j_received) && j_received->valuestring)
      cJSON_AddStringToObject(mapped, "received_at", j_received->valuestring);
    else if (cJSON_IsString(j_date) && j_date->valuestring)
      cJSON_AddStringToObject(mapped, "date", j_date->valuestring);

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
