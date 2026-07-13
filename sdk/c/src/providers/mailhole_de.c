/**
 * MailholeDe — https://mailhole.de
 * 公共临时邮箱，无需认证。
 * 流程:
 *   1. GET /api/random → HTML，正则提取 [a-z0-9.]+@mailhole.de
 *   2. GET /json/{email} → JSON 数组获取邮件
 * Token 即邮箱地址本身。
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAILHOLE_DE_BASE "https://mailhole.de"
#define MAILHOLE_DE_DOMAIN "@mailhole.de"

static const char *mailhole_de_html_headers[] = {"Accept: text/html", NULL};

static const char *mailhole_de_json_headers[] = {"Accept: application/json",
                                                 NULL};

/**
 * 从 HTML 中提取 [a-z0-9.]+@mailhole.de 邮箱地址
 * 定位 "@mailhole.de" 后向前回溯 local part（对应 Go 正则
 * ([a-z0-9.]+@mailhole\.de)）
 * @param html 页面 HTML
 * @param out  输出缓冲区
 * @param cap  缓冲区容量
 * @return 成功返回 0，失败返回 -1
 */
static int mailhole_de_extract_email(const char *html, char *out, size_t cap) {
  const char *at = html;
  while ((at = strstr(at, MAILHOLE_DE_DOMAIN)) != NULL) {
    /* 从 @ 位置向前回溯 local part 起点 */
    const char *start = at;
    while (start > html) {
      char c = start[-1];
      int ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.';
      if (!ok)
        break;
      start--;
    }
    if (start < at) {
      /* local part 非空，组装完整地址 */
      size_t len = (size_t)(at - start) + strlen(MAILHOLE_DE_DOMAIN);
      if (len < cap) {
        memcpy(out, start, len);
        out[len] = '\0';
        return 0;
      }
    }
    at += strlen(MAILHOLE_DE_DOMAIN);
  }
  return -1;
}

/**
 * 创建 mailhole.de 临时邮箱
 */
tm_email_info_t *tm_provider_mailhole_de_generate(void) {
  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, MAILHOLE_DE_BASE "/api/random",
                      mailhole_de_html_headers, NULL, 15);
  if (!resp || resp->status != 200 || !resp->body) {
    tm_http_response_free(resp);
    return NULL;
  }

  char email[256];
  int rc = mailhole_de_extract_email(resp->body, email, sizeof(email));
  tm_http_response_free(resp);
  if (rc != 0) {
    TM_LOG_WARN("mailhole-de: 无法从响应中解析邮箱地址");
    return NULL;
  }

  tm_email_info_t *info = tm_email_info_new();
  if (!info)
    return NULL;
  info->channel = CHANNEL_MAILHOLE_DE;
  info->email = tm_strdup(email);
  info->token = tm_strdup(email);
  if (!info->email || !info->token) {
    free(info->email);
    free(info->token);
    free(info);
    return NULL;
  }
  return info;
}

/**
 * 按优先级从消息对象提取首个非空字符串字段
 */
static const char *mailhole_de_first_str(const cJSON *msg, const char *k1,
                                         const char *k2) {
  const cJSON *v = cJSON_GetObjectItemCaseSensitive((cJSON *)msg, k1);
  if (cJSON_IsString(v) && v->valuestring && v->valuestring[0])
    return v->valuestring;
  v = cJSON_GetObjectItemCaseSensitive((cJSON *)msg, k2);
  if (cJSON_IsString(v) && v->valuestring && v->valuestring[0])
    return v->valuestring;
  return "";
}

/**
 * 获取 mailhole.de 邮件列表
 * @param token 邮箱地址（即 token），为空时回退 email
 * @param email 邮箱地址
 * @param count 输出邮件数量；-1 表示请求失败
 */
tm_email_t *tm_provider_mailhole_de_get_emails(const char *token,
                                               const char *email, int *count) {
  *count = -1;
  const char *addr = (token && token[0]) ? token : email;
  if (!addr || !addr[0])
    return NULL;

  size_t cap = strlen(MAILHOLE_DE_BASE) + strlen(addr) + 8;
  char *url = (char *)malloc(cap);
  if (!url)
    return NULL;
  snprintf(url, cap, "%s/json/%s", MAILHOLE_DE_BASE, addr);

  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, mailhole_de_json_headers, NULL, 15);
  free(url);
  if (!resp)
    return NULL;
  if (resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }
  if (!resp->body || resp->size == 0) {
    tm_http_response_free(resp);
    *count = 0;
    return NULL;
  }

  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!root)
    return NULL;
  if (!cJSON_IsArray(root)) {
    cJSON_Delete(root);
    return NULL;
  }

  int n = cJSON_GetArraySize(root);
  if (n == 0) {
    cJSON_Delete(root);
    *count = 0;
    return NULL;
  }

  tm_email_t *emails = tm_emails_new(n);
  if (!emails) {
    cJSON_Delete(root);
    return NULL;
  }

  for (int i = 0; i < n; i++) {
    cJSON *msg = cJSON_GetArrayItem(root, i);
    cJSON *raw = cJSON_CreateObject();
    if (!raw) {
      emails[i] = tm_normalize_email(msg, addr);
      continue;
    }
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(msg, "id");
    if (id)
      cJSON_AddItemToObject(raw, "id", cJSON_Duplicate((cJSON *)id, 1));
    const cJSON *subject =
        cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "subject");
    if (subject)
      cJSON_AddItemToObject(raw, "subject",
                            cJSON_Duplicate((cJSON *)subject, 1));
    cJSON_AddStringToObject(raw, "from",
                            mailhole_de_first_str(msg, "sender", "from"));
    cJSON_AddStringToObject(raw, "to", addr);
    cJSON_AddStringToObject(raw, "text",
                            mailhole_de_first_str(msg, "body", "text"));
    cJSON_AddStringToObject(raw, "html",
                            mailhole_de_first_str(msg, "html", "body"));
    cJSON_AddStringToObject(raw, "created_at",
                            mailhole_de_first_str(msg, "date", "received"));
    emails[i] = tm_normalize_email(raw, addr);
    cJSON_Delete(raw);
  }

  cJSON_Delete(root);
  *count = n;
  return emails;
}
