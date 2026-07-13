/**
 * Mail Sunls -- https://mail.sunls.de
 * 无需认证、无需 Token 的简单 REST API 渠道
 * 创建邮箱: 本地随机生成用户名 + 从域名列表中随机选取域名
 * 获取邮件: GET /api/fetch?to={email}
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MS_BASE "https://mail.sunls.de"

/* 备用域名列表，当域名 API 不可用时使用 */
static const char *ms_fallback_domains[] = {
    "isco.eu.org",
    "sunix.eu.org",
    "chato.eu.org",
};
#define MS_FALLBACK_DOMAIN_COUNT 3

static const char *ms_headers[] = {
    "Accept: application/json, text/plain, */*",
    "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    NULL};

/**
 * 生成随机本地部分（小写字母 + 数字）
 *
 * @param buf  输出缓冲区
 * @param len  生成字符数
 */
static void ms_random_local(char *buf, size_t len) {
  static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
  for (size_t i = 0; i < len; i++) {
    buf[i] = chars[rand() % (sizeof(chars) - 1)];
  }
  buf[len] = '\0';
}

/**
 * 从 API 获取可用域名列表，失败则返回备用列表
 *
 * @param domains    输出域名数组（固定大小缓冲区）
 * @param max_count  数组容量
 * @return           实际域名数量
 */
static int ms_fetch_domains(char domains[][128], int max_count) {
  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, MS_BASE "/api/domain", ms_headers, NULL, 15);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    /* 使用备用域名 */
    int n = MS_FALLBACK_DOMAIN_COUNT < max_count ? MS_FALLBACK_DOMAIN_COUNT
                                                 : max_count;
    for (int i = 0; i < n; i++) {
      strncpy(domains[i], ms_fallback_domains[i], 127);
      domains[i][127] = '\0';
    }
    return n;
  }

  cJSON *arr = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!arr || !cJSON_IsArray(arr)) {
    cJSON_Delete(arr);
    /* 使用备用域名 */
    int n = MS_FALLBACK_DOMAIN_COUNT < max_count ? MS_FALLBACK_DOMAIN_COUNT
                                                 : max_count;
    for (int i = 0; i < n; i++) {
      strncpy(domains[i], ms_fallback_domains[i], 127);
      domains[i][127] = '\0';
    }
    return n;
  }

  int size = cJSON_GetArraySize(arr);
  int n = 0;
  for (int i = 0; i < size && n < max_count; i++) {
    cJSON *item = cJSON_GetArrayItem(arr, i);
    if (cJSON_IsString(item) && item->valuestring && item->valuestring[0]) {
      strncpy(domains[n], item->valuestring, 127);
      domains[n][127] = '\0';
      n++;
    }
  }
  cJSON_Delete(arr);

  if (n == 0) {
    /* API 返回空数组，使用备用域名 */
    n = MS_FALLBACK_DOMAIN_COUNT < max_count ? MS_FALLBACK_DOMAIN_COUNT
                                             : max_count;
    for (int i = 0; i < n; i++) {
      strncpy(domains[i], ms_fallback_domains[i], 127);
      domains[i][127] = '\0';
    }
  }
  return n;
}

tm_email_info_t *tm_provider_mail_sunls_generate(void) {
  srand((unsigned)time(NULL));

  /* 获取可用域名 */
  char domains[16][128];
  int domain_count = ms_fetch_domains(domains, 16);
  if (domain_count <= 0)
    return NULL;

  /* 随机选取域名 */
  const char *domain = domains[rand() % domain_count];

  /* 生成随机本地部分（10 个字符） */
  char local[16];
  ms_random_local(local, 10);

  /* 拼接邮箱地址 */
  char email[256];
  snprintf(email, sizeof(email), "%s@%s", local, domain);

  tm_email_info_t *info = tm_email_info_new();
  if (!info)
    return NULL;
  info->channel = CHANNEL_MAIL_SUNLS;
  info->email = tm_strdup(email);
  info->token = NULL;
  return info;
}

tm_email_t *tm_provider_mail_sunls_get_emails(const char *email, int *count) {
  *count = -1;
  if (!email || !email[0])
    return NULL;

  /* 构造获取邮件列表的 URL */
  char url[512];
  snprintf(url, sizeof(url), MS_BASE "/api/fetch?to=%s", email);

  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, ms_headers, NULL, 15);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }

  cJSON *arr = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!arr || !cJSON_IsArray(arr)) {
    cJSON_Delete(arr);
    return NULL;
  }

  int n = cJSON_GetArraySize(arr);
  *count = n;
  if (n == 0) {
    cJSON_Delete(arr);
    return NULL;
  }

  tm_email_t *emails = tm_emails_new(n);
  if (!emails) {
    cJSON_Delete(arr);
    *count = -1;
    return NULL;
  }

  for (int i = 0; i < n; i++) {
    cJSON *raw = cJSON_GetArrayItem(arr, i);

    /* 尝试获取邮件 ID，用于拉取单封邮件详情 */
    const char *id =
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(raw, "id"), "");
    if (!id[0]) {
      id = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(raw, "_id"), "");
    }

    /* 如果列表中没有正文内容，尝试通过单封邮件 API 获取 */
    cJSON *html_item = cJSON_GetObjectItemCaseSensitive(raw, "html");
    cJSON *text_item = cJSON_GetObjectItemCaseSensitive(raw, "text");
    int has_body = (cJSON_IsString(html_item) && html_item->valuestring &&
                    html_item->valuestring[0]) ||
                   (cJSON_IsString(text_item) && text_item->valuestring &&
                    text_item->valuestring[0]);

    if (!has_body && id[0]) {
      /* 拉取单封邮件详情 */
      char detail_url[512];
      snprintf(detail_url, sizeof(detail_url), MS_BASE "/api/fetch/%s", id);
      tm_http_response_t *detail_resp =
          tm_http_request(TM_HTTP_GET, detail_url, ms_headers, NULL, 15);
      if (detail_resp && detail_resp->status >= 200 &&
          detail_resp->status < 300) {
        cJSON *detail = cJSON_Parse(detail_resp->body);
        if (detail && cJSON_IsObject(detail)) {
          /* 将详情中的字段合并到列表项 */
          const char *dhtml =
              TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(detail, "html"), "");
          const char *dtext =
              TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(detail, "text"), "");
          if (dhtml[0])
            cJSON_AddStringToObject(raw, "html", dhtml);
          if (dtext[0])
            cJSON_AddStringToObject(raw, "text", dtext);
          cJSON_Delete(detail);
        }
      }
      tm_http_response_free(detail_resp);
    }

    emails[i] = tm_normalize_email(raw, email);
  }

  cJSON_Delete(arr);
  return emails;
}
