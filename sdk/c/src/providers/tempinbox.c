/**
 * TempInbox — https://www.tempinbox.xyz
 * 无需认证、无token、无cookie
 */
#include "tempmail_internal.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef _WIN32
#include <strings.h>
#endif

#define TI_BASE "https://endpoint.tempinbox.xyz"

/* 硬编码域名列表 */
static const char *ti_domains[] = {
    "tempinbox.xyz",
    "thepiratebay.cloud",
    "cryptoblad.nl",
};
#define TI_DOMAIN_COUNT ((int)(sizeof(ti_domains) / sizeof(ti_domains[0])))

static const char *ti_headers[] = {
    "Accept: application/json, text/plain, */*",
    "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8",
    "Cache-Control: no-cache",
    "DNT: 1",
    "Pragma: no-cache",
    "Referer: https://www.tempinbox.xyz/",
    "sec-ch-ua: \"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft "
    "Edge\";v=\"146\"",
    "sec-ch-ua-mobile: ?0",
    "sec-ch-ua-platform: \"Windows\"",
    "sec-fetch-dest: empty",
    "sec-fetch-mode: cors",
    "sec-fetch-site: cross-site",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    NULL};

static int ti_stricmp(const char *a, const char *b) {
#ifdef _WIN32
  return _stricmp(a ? a : "", b ? b : "");
#else
  return strcasecmp(a ? a : "", b ? b : "");
#endif
}

/**
 * 生成随机用户名（8~12 位小写字母+数字）
 */
static void ti_random_user(char *buf, size_t cap) {
  static const char charset[] = "abcdefghijklmnopqrstuvwxyz0123456789";
  int len = 8 + rand() % 5; /* 8~12 */
  if ((size_t)len >= cap)
    len = (int)cap - 1;
  for (int i = 0; i < len; i++) {
    buf[i] = charset[rand() % (sizeof(charset) - 1)];
  }
  buf[len] = '\0';
}

/**
 * 从返回的带引号字符串中提取邮箱地址
 * API 返回格式为 "user@domain"（含引号）
 */
static char *ti_strip_quotes(const char *raw) {
  if (!raw || !raw[0])
    return NULL;
  size_t len = strlen(raw);
  /* 去除首尾引号 */
  const char *start = raw;
  const char *end = raw + len;
  if (*start == '"')
    start++;
  if (end > start && *(end - 1) == '"')
    end--;
  if (end <= start)
    return NULL;
  size_t slen = (size_t)(end - start);
  char *result = (char *)malloc(slen + 1);
  if (!result)
    return NULL;
  memcpy(result, start, slen);
  result[slen] = '\0';
  return result;
}

tm_email_info_t *tm_provider_tempinbox_generate(const char *domain) {
  srand((unsigned)time(NULL));

  char url[768];

  if (domain && domain[0]) {
    /* 用户指定了域名，验证域名合法性 */
    int valid = 0;
    for (int i = 0; i < TI_DOMAIN_COUNT; i++) {
      if (ti_stricmp(ti_domains[i], domain) == 0) {
        valid = 1;
        break;
      }
    }
    if (!valid)
      return NULL;

    /* 生成随机用户名，拼接指定域名 */
    char user[32];
    ti_random_user(user, sizeof(user));
    snprintf(url, sizeof(url), "%s/email/%s@%s", TI_BASE, user, domain);
  } else {
    /* 未指定域名，使用随机邮箱 API */
    snprintf(url, sizeof(url), "%s/email/Random", TI_BASE);
  }

  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, ti_headers, NULL, 15);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }

  /* 响应体为纯字符串（可能带引号），如 "user@domain" */
  char *addr = ti_strip_quotes(resp->body);
  tm_http_response_free(resp);

  if (!addr || !addr[0]) {
    free(addr);
    return NULL;
  }

  /* 验证返回的地址包含 @ */
  if (!strchr(addr, '@')) {
    free(addr);
    return NULL;
  }

  tm_email_info_t *info = tm_email_info_new();
  info->channel = CHANNEL_TEMPINBOX;
  info->email = addr;
  info->token = NULL;
  return info;
}

tm_email_t *tm_provider_tempinbox_get_emails(const char *email, int *count) {
  *count = -1;
  if (!email || !email[0])
    return NULL;

  char url[1200];
  snprintf(url, sizeof(url), "%s/messages/%s", TI_BASE, email);

  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, ti_headers, NULL, 15);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }

  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!root)
    return NULL;

  /* 响应体直接是邮件数组 */
  if (!cJSON_IsArray(root)) {
    *count = 0;
    cJSON_Delete(root);
    return NULL;
  }

  int n = cJSON_GetArraySize(root);
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
    cJSON *raw = cJSON_GetArrayItem(root, i);
    emails[i] = tm_normalize_email(raw, email);
  }
  cJSON_Delete(root);
  return emails;
}
