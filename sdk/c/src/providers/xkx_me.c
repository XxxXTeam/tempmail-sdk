/**
 * xkx.me 渠道实现
 *
 * 流程：
 * 1. GET 首页提取 CSRF token 和 Cookie
 * 2. POST /mailbox/create/random 创建随机邮箱
 * 3. 从 Location 或响应体提取邮箱地址
 * 4. GET /mailbox/{email}/messages 获取邮件列表
 */
#include "tempmail_internal.h"

#define XKX_BASE "https://xkx.me"

static const char *xkx_ua =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36 Edg/144.0.0.0";

/**
 * 从 HTML 中提取 CSRF token
 * 查找 csrf-token" content="..." 模式
 */
static char *xkx_extract_csrf(const char *html) {
  const char *needle = "csrf-token\" content=\"";
  char *pos = strstr(html, needle);
  if (!pos)
    return NULL;
  pos += strlen(needle);
  char *end = strchr(pos, '"');
  if (!end)
    return NULL;
  size_t len = (size_t)(end - pos);
  char *token = (char *)malloc(len + 1);
  if (!token)
    return NULL;
  memcpy(token, pos, len);
  token[len] = '\0';
  return token;
}

/**
 * 从文本中提取 xkx.me 邮箱地址
 * 查找 mailbox/xxx@xkx.me 模式
 */
static char *xkx_extract_email(const char *text) {
  const char *needle = "mailbox/";
  char *pos = strstr(text, needle);
  while (pos) {
    pos += strlen(needle);
    /* 查找 @ 和域名 */
    char *at = strchr(pos, '@');
    if (at) {
      /* 检查域名是否为 xkx.me */
      if (strncmp(at + 1, "xkx.me", 6) == 0) {
        /* 确定邮箱地址边界 */
        char *end = at + 7; /* 跳过 "xkx.me" */
        /* 确保后面是终结符 */
        if (*end == '\0' || *end == '"' || *end == '\'' || *end == '<' ||
            *end == '>' || *end == '/' || *end == ' ' || *end == '\n' ||
            *end == '\r') {
          size_t len = (size_t)(end - pos);
          char *email = (char *)malloc(len + 1);
          if (!email)
            return NULL;
          memcpy(email, pos, len);
          email[len] = '\0';
          return email;
        }
      }
    }
    /* 继续查找下一个 mailbox/ */
    pos = strstr(pos, needle);
  }
  return NULL;
}

tm_email_info_t *tm_provider_xkx_me_generate(void) {
  /* 第一步：获取首页，提取 CSRF token 和 Cookie */
  const char *get_headers[] = {"Accept: text/html", NULL};

  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, XKX_BASE, get_headers, NULL, 15);
  if (!resp || resp->status != 200) {
    tm_http_response_free(resp);
    return NULL;
  }

  char *csrf = xkx_extract_csrf(resp->body);
  if (!csrf) {
    tm_http_response_free(resp);
    return NULL;
  }

  /* 保存 cookie */
  char *cookies = resp->cookies ? tm_strdup(resp->cookies) : NULL;
  tm_http_response_free(resp);

  if (!cookies) {
    free(csrf);
    return NULL;
  }

  /* 第二步：POST 创建随机邮箱 */
  char body[512];
  snprintf(body, sizeof(body), "_token=%s", csrf);
  free(csrf);

  char cookie_hdr[1024];
  snprintf(cookie_hdr, sizeof(cookie_hdr), "Cookie: %s", cookies);

  char ua_hdr[256];
  snprintf(ua_hdr, sizeof(ua_hdr), "User-Agent: %s", xkx_ua);

  const char *post_headers[] = {
      cookie_hdr, "Content-Type: application/x-www-form-urlencoded", ua_hdr,
      NULL};

  tm_http_response_t *post_resp = tm_http_request(
      TM_HTTP_POST, XKX_BASE "/mailbox/create/random", post_headers, body, 15);
  if (!post_resp) {
    free(cookies);
    return NULL;
  }

  /* 从响应体中提取邮箱地址（重定向后的页面或 Location） */
  char *email = NULL;
  if (post_resp->body) {
    email = xkx_extract_email(post_resp->body);
  }

  /* 更新 cookie（如果服务端返回了新的） */
  if (post_resp->cookies && post_resp->cookies[0] != '\0') {
    free(cookies);
    cookies = tm_strdup(post_resp->cookies);
  }

  tm_http_response_free(post_resp);

  if (!email) {
    free(cookies);
    return NULL;
  }

  tm_email_info_t *info = tm_email_info_new();
  info->channel = CHANNEL_XKX_ME;
  info->email = email;
  info->token = cookies;
  return info;
}

tm_email_t *tm_provider_xkx_me_get_emails(const char *token, const char *email,
                                           int *count) {
  *count = -1;
  if (!email || !email[0])
    return NULL;

  /* 构建请求 URL */
  char url[512];
  snprintf(url, sizeof(url), XKX_BASE "/mailbox/%s/messages", email);

  /* 构建带 Cookie 的 headers */
  char cookie_hdr[1024];
  snprintf(cookie_hdr, sizeof(cookie_hdr), "Cookie: %s", token);

  char ua_hdr[256];
  snprintf(ua_hdr, sizeof(ua_hdr), "User-Agent: %s", xkx_ua);

  const char *headers[] = {cookie_hdr, "X-Requested-With: XMLHttpRequest",
                           "Accept: application/json", ua_hdr, NULL};

  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, headers, NULL, 15);
  if (!resp) {
    return NULL;
  }
  if (resp->status == 404) {
    /* 邮箱不存在或无邮件 */
    *count = 0;
    tm_http_response_free(resp);
    return NULL;
  }
  if (resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }

  /* 解析 JSON 响应 */
  cJSON *json = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!json) {
    *count = 0;
    return NULL;
  }

  /* 响应可能是数组或 { "messages": [...] } 包装 */
  cJSON *arr = NULL;
  if (cJSON_IsArray(json)) {
    arr = json;
  } else {
    arr = cJSON_GetObjectItemCaseSensitive(json, "messages");
  }

  int n = (arr && cJSON_IsArray(arr)) ? cJSON_GetArraySize(arr) : 0;
  *count = n;
  if (n == 0) {
    cJSON_Delete(json);
    return NULL;
  }

  tm_email_t *emails = tm_emails_new(n);
  for (int i = 0; i < n; i++) {
    cJSON *msg = cJSON_GetArrayItem(arr, i);
    /* 构建标准化输入：优先使用 html，fallback 到 body */
    cJSON *html = cJSON_GetObjectItemCaseSensitive(msg, "html");
    if (!html || !cJSON_GetStringValue(html)) {
      cJSON *body_field = cJSON_GetObjectItemCaseSensitive(msg, "body");
      if (body_field && cJSON_GetStringValue(body_field)) {
        /* 将 body 字段值复制到 html 字段以便 normalize 处理 */
        cJSON_AddStringToObject(msg, "html", cJSON_GetStringValue(body_field));
      }
    }
    /* 添加 to 字段供 normalize 使用 */
    if (!cJSON_GetObjectItemCaseSensitive(msg, "to")) {
      cJSON_AddStringToObject(msg, "to", email);
    }
    emails[i] = tm_normalize_email(msg, email);
  }

  cJSON_Delete(json);
  return emails;
}
