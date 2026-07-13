/**
 * tmail-link — https://tmail.link
 * Django 临时邮箱服务（CSRF token 认证）
 * 流程:
 *   1. GET / → HTML 中包含随机邮箱地址 (xxx@tmail.link)
 *   2. GET /inbox/{email}/ → 从 Set-Cookie 提取 csrftoken
 *   3. POST /inbox/{email}/ + format=json + csrfmiddlewaretoken → JSON 邮件列表
 * Token 格式: csrftoken 字符串
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TMAIL_LINK_BASE "https://tmail.link"
#define TMAIL_LINK_DOMAIN "@tmail.link"
#define TMAIL_LINK_MAX_MAILS 128

/* 公共 UA */
static const char *tmail_link_ua =
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36";

/**
 * 判断字符是否属于邮箱 local part 字符集 [a-zA-Z0-9._%+-]
 */
static int tmail_link_is_local_char(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '%' ||
         c == '+' || c == '-';
}

/**
 * 从 HTML 中提取 [a-zA-Z0-9._%+-]+@tmail.link 邮箱地址
 * 定位 "@tmail.link" 后向前回溯 local part（对应 Go 正则）
 *
 * @param html 页面 HTML
 * @param out  输出缓冲区
 * @param cap  缓冲区容量
 * @return 成功返回 0，失败返回 -1
 */
static int tmail_link_extract_email(const char *html, char *out, size_t cap) {
  const char *at = html;
  while ((at = strstr(at, TMAIL_LINK_DOMAIN)) != NULL) {
    /* 从 @ 位置向前回溯 local part 起点 */
    const char *start = at;
    while (start > html && tmail_link_is_local_char(start[-1])) {
      start--;
    }
    if (start < at) {
      /* local part 非空，组装完整地址 */
      size_t len = (size_t)(at - start) + strlen(TMAIL_LINK_DOMAIN);
      if (len < cap) {
        memcpy(out, start, len);
        out[len] = '\0';
        return 0;
      }
    }
    at += strlen(TMAIL_LINK_DOMAIN);
  }
  return -1;
}

/**
 * 从合并后的 Cookie 串（"name=value; name=value"）中提取指定名称的值
 *
 * @param cookies Set-Cookie 合并串，可能为 NULL
 * @param name    Cookie 名称（如 "csrftoken"）
 * @param out     输出缓冲区
 * @param cap     缓冲区容量
 * @return 成功返回 0，未找到返回 -1
 */
static int tmail_link_cookie_value(const char *cookies, const char *name,
                                   char *out, size_t cap) {
  if (!cookies || !name)
    return -1;
  size_t name_len = strlen(name);
  const char *p = cookies;
  while ((p = strstr(p, name)) != NULL) {
    /* 校验为独立 cookie 名（前面是串首或 "; "，紧跟 '='） */
    int boundary_ok = (p == cookies) || (p[-1] == ' ') || (p[-1] == ';');
    if (boundary_ok && p[name_len] == '=') {
      const char *v = p + name_len + 1;
      const char *end = v;
      while (*end && *end != ';')
        end++;
      size_t len = (size_t)(end - v);
      if (len == 0)
        return -1;
      if (len >= cap)
        len = cap - 1;
      memcpy(out, v, len);
      out[len] = '\0';
      return 0;
    }
    p += name_len;
  }
  return -1;
}

/**
 * 创建 tmail.link 临时邮箱
 */
tm_email_info_t *tm_provider_tmail_link_generate(void) {
  int timeout =
      tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

  const char *html_hdrs[] = {tmail_link_ua, "Accept: text/html", NULL};

  /* 步骤1: GET / 获取随机邮箱地址 */
  tm_http_response_t *r1 = tm_http_request(TM_HTTP_GET, TMAIL_LINK_BASE "/",
                                           html_hdrs, NULL, timeout);
  if (!r1 || r1->status != 200 || !r1->body) {
    TM_LOG_ERR("[tmail-link] 获取首页失败, status=%ld", r1 ? r1->status : 0);
    tm_http_response_free(r1);
    return NULL;
  }

  char email[256];
  int rc = tmail_link_extract_email(r1->body, email, sizeof(email));
  tm_http_response_free(r1);
  if (rc != 0) {
    TM_LOG_WARN("[tmail-link] 无法从响应中解析邮箱地址");
    return NULL;
  }

  /* 步骤2: GET /inbox/{email}/ 获取 csrftoken */
  char inbox_url[512];
  snprintf(inbox_url, sizeof(inbox_url), TMAIL_LINK_BASE "/inbox/%s/", email);

  tm_http_response_t *r2 =
      tm_http_request(TM_HTTP_GET, inbox_url, html_hdrs, NULL, timeout);
  if (!r2) {
    TM_LOG_ERR("[tmail-link] inbox 请求失败");
    return NULL;
  }

  char csrf[256];
  rc = tmail_link_cookie_value(r2->cookies, "csrftoken", csrf, sizeof(csrf));
  tm_http_response_free(r2);
  if (rc != 0) {
    TM_LOG_WARN("[tmail-link] 未获取到 csrftoken");
    return NULL;
  }

  tm_email_info_t *info = tm_email_info_new();
  if (!info)
    return NULL;
  info->channel = CHANNEL_TMAIL_LINK;
  info->email = tm_strdup(email);
  info->token = tm_strdup(csrf);
  if (!info->email || !info->token) {
    free(info->email);
    free(info->token);
    free(info);
    return NULL;
  }

  TM_LOG_DBG("[tmail-link] 创建邮箱成功: %s", info->email);
  return info;
}

/**
 * 获取 tmail.link 邮件列表
 *
 * @param token csrftoken 字符串
 * @param email 完整邮箱地址
 * @param count 输出邮件数量；-1 表示请求失败
 */
tm_email_t *tm_provider_tmail_link_get_emails(const char *token,
                                              const char *email, int *count) {
  *count = -1;
  if (!token || !token[0] || !email || !email[0])
    return NULL;

  int timeout =
      tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

  char inbox_url[512];
  snprintf(inbox_url, sizeof(inbox_url), TMAIL_LINK_BASE "/inbox/%s/", email);

  /* 步骤1: GET inbox 页面刷新 csrftoken */
  char cookie_hdr[512];
  snprintf(cookie_hdr, sizeof(cookie_hdr), "Cookie: csrftoken=%s", token);

  const char *get_hdrs[] = {tmail_link_ua, "Accept: text/html", cookie_hdr,
                            NULL};

  tm_http_response_t *r1 =
      tm_http_request(TM_HTTP_GET, inbox_url, get_hdrs, NULL, timeout);
  if (!r1) {
    TM_LOG_ERR("[tmail-link] GET inbox 请求失败");
    return NULL;
  }

  /* 从响应 Cookie 中提取新的 csrftoken，失败则沿用旧 token */
  char fresh[256];
  if (tmail_link_cookie_value(r1->cookies, "csrftoken", fresh, sizeof(fresh)) !=
      0) {
    snprintf(fresh, sizeof(fresh), "%s", token);
  }
  tm_http_response_free(r1);

  /* 步骤2: POST inbox 获取 JSON 邮件列表 */
  char body[512];
  snprintf(body, sizeof(body), "format=json&csrfmiddlewaretoken=%s", fresh);

  char cookie_hdr2[512];
  snprintf(cookie_hdr2, sizeof(cookie_hdr2), "Cookie: csrftoken=%s", fresh);
  char xcsrf_hdr[512];
  snprintf(xcsrf_hdr, sizeof(xcsrf_hdr), "X-CSRFToken: %s", fresh);
  char referer_hdr[576];
  snprintf(referer_hdr, sizeof(referer_hdr), "Referer: %s", inbox_url);

  const char *post_hdrs[] = {tmail_link_ua,
                             "Content-Type: application/x-www-form-urlencoded",
                             "Accept: application/json",
                             cookie_hdr2,
                             xcsrf_hdr,
                             referer_hdr,
                             NULL};

  tm_http_response_t *r2 =
      tm_http_request(TM_HTTP_POST, inbox_url, post_hdrs, body, timeout);
  if (!r2 || r2->status < 200 || r2->status >= 300 || !r2->body) {
    TM_LOG_ERR("[tmail-link] POST inbox 请求失败, status=%ld",
               r2 ? r2->status : 0);
    tm_http_response_free(r2);
    return NULL;
  }

  cJSON *root = cJSON_Parse(r2->body);
  tm_http_response_free(r2);
  if (!root) {
    TM_LOG_ERR("[tmail-link] 邮件响应解析失败");
    return NULL;
  }

  cJSON *arr = cJSON_GetObjectItemCaseSensitive(root, "messages");
  if (!cJSON_IsArray(arr)) {
    TM_LOG_ERR("[tmail-link] 响应中未找到 messages 数组");
    cJSON_Delete(root);
    return NULL;
  }

  int n = cJSON_GetArraySize(arr);
  if (n == 0) {
    cJSON_Delete(root);
    *count = 0;
    return NULL;
  }
  if (n > TMAIL_LINK_MAX_MAILS)
    n = TMAIL_LINK_MAX_MAILS;

  tm_email_t *emails = tm_emails_new(n);
  if (!emails) {
    cJSON_Delete(root);
    return NULL;
  }

  int valid = 0;
  for (int i = 0; i < n; i++) {
    const cJSON *msg = cJSON_GetArrayItem(arr, i);
    if (!cJSON_IsObject(msg))
      continue;

    cJSON *raw = cJSON_CreateObject();
    if (!raw)
      continue;

    const char *id_keys[] = {"key"};
    char *id_str = tm_json_get_str(msg, id_keys, 1);
    cJSON_AddStringToObject(raw, "id", id_str);
    free(id_str);

    const char *from_keys[] = {"sender", "from"};
    char *from_str = tm_json_get_str(msg, from_keys, 2);
    cJSON_AddStringToObject(raw, "from", from_str);
    free(from_str);

    cJSON_AddStringToObject(raw, "to", email);

    const char *subj_keys[] = {"subject"};
    char *subj_str = tm_json_get_str(msg, subj_keys, 1);
    cJSON_AddStringToObject(raw, "subject", subj_str);
    free(subj_str);

    const char *text_keys[] = {"body", "text"};
    char *text_str = tm_json_get_str(msg, text_keys, 2);
    cJSON_AddStringToObject(raw, "text", text_str);
    free(text_str);

    const char *html_keys[] = {"html", "body"};
    char *html_str = tm_json_get_str(msg, html_keys, 2);
    cJSON_AddStringToObject(raw, "html", html_str);
    free(html_str);

    const char *date_keys[] = {"date", "created_at"};
    char *date_str = tm_json_get_str(msg, date_keys, 2);
    cJSON_AddStringToObject(raw, "created_at", date_str);
    free(date_str);

    emails[valid] = tm_normalize_email(raw, email);
    cJSON_Delete(raw);
    valid++;
  }

  cJSON_Delete(root);

  if (valid == 0) {
    free(emails);
    *count = 0;
    return NULL;
  }

  *count = valid;
  return emails;
}
