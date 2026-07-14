/**
 * mail.chatgpt.org.uk 渠道实现
 *
 * 流程:
 * 1. GET /api/domains/public → 可用域名列表
 * 2. 本地生成随机用户名，组合邮箱地址
 * 3. POST /api/inbox-token {"email":"..."} → JWT token + set-cookie gm_sid
 * 4. GET /api/emails?email=... (Cookie: gm_sid + x-inbox-token) → 邮件列表
 */
#include "tempmail_internal.h"
#include <ctype.h>
#include <time.h>

#define CG_BASE "https://mail.chatgpt.org.uk/api"

static const char *cg_headers[] = {
    "Accept: */*",
    "Referer: https://mail.chatgpt.org.uk/zh/",
    "Origin: https://mail.chatgpt.org.uk",
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/150.0.0.0 Safari/537.36 Edg/150.0.0.0",
    "DNT: 1",
    "sec-fetch-dest: empty",
    "sec-fetch-mode: cors",
    "sec-fetch-site: same-origin",
    NULL};

static void cg_random_username(char *buf, size_t len) {
  static const char charset[] = "abcdefghijklmnopqrstuvwxyz0123456789";
  srand((unsigned)time(NULL));
  for (size_t i = 0; i < len; i++) {
    buf[i] = charset[rand() % (sizeof(charset) - 1)];
  }
  buf[len] = '\0';
}

/** 从 set-cookie 响应头提取 gm_sid 值 */
static char *cg_extract_gm_sid(const char *cookies) {
  if (!cookies)
    return NULL;
  const char *p = strstr(cookies, "gm_sid=");
  if (!p)
    return NULL;
  p += 7;
  const char *end = p;
  while (*end && *end != ';' && *end != ' ')
    end++;
  size_t len = (size_t)(end - p);
  if (len == 0)
    return NULL;
  char *out = (char *)malloc(len + 1);
  if (!out)
    return NULL;
  memcpy(out, p, len);
  out[len] = '\0';
  return out;
}

/** 获取可用域名列表，随机选择一个返回 */
static char *cg_fetch_random_domain(void) {
  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, CG_BASE "/domains/public", cg_headers, NULL, 15);
  if (!resp || resp->status != 200) {
    tm_http_response_free(resp);
    return NULL;
  }

  cJSON *json = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!json)
    return NULL;

  if (!cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(json, "success"))) {
    cJSON_Delete(json);
    return NULL;
  }

  cJSON *data = cJSON_GetObjectItemCaseSensitive(json, "data");
  cJSON *domains = cJSON_GetObjectItemCaseSensitive(data, "domains");
  if (!cJSON_IsArray(domains) || cJSON_GetArraySize(domains) == 0) {
    cJSON_Delete(json);
    return NULL;
  }

  /* 收集 is_active==1 的域名 */
  int total = cJSON_GetArraySize(domains);
  char **active = (char **)calloc((size_t)total, sizeof(char *));
  int active_count = 0;

  for (int i = 0; i < total; i++) {
    cJSON *item = cJSON_GetArrayItem(domains, i);
    cJSON *is_active = cJSON_GetObjectItemCaseSensitive(item, "is_active");
    if (!cJSON_IsNumber(is_active) || is_active->valueint != 1)
      continue;
    const char *name = cJSON_GetStringValue(
        cJSON_GetObjectItemCaseSensitive(item, "domain_name"));
    if (name && name[0]) {
      active[active_count++] = tm_strdup(name);
    }
  }

  if (active_count == 0) {
    free(active);
    cJSON_Delete(json);
    return NULL;
  }

  int idx = rand() % active_count;
  char *result = active[idx];
  for (int i = 0; i < active_count; i++) {
    if (i != idx)
      free(active[i]);
  }
  free(active);
  cJSON_Delete(json);
  return result;
}

/** 创建收件箱，返回 inbox_token 和 gm_sid */
static int cg_create_inbox(const char *email, char **inbox_out,
                           char **gm_sid_out) {
  *inbox_out = NULL;
  *gm_sid_out = NULL;

  char body[512];
  snprintf(body, sizeof(body), "{\"email\":\"%s\"}", email);

  const char *headers[] = {
      "Accept: */*",
      "Referer: https://mail.chatgpt.org.uk/zh/",
      "Origin: https://mail.chatgpt.org.uk",
      "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
      "(KHTML, like Gecko) Chrome/150.0.0.0 Safari/537.36 Edg/150.0.0.0",
      "DNT: 1",
      "Content-Type: application/json",
      "sec-fetch-dest: empty",
      "sec-fetch-mode: cors",
      "sec-fetch-site: same-origin",
      NULL};

  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_POST, CG_BASE "/inbox-token", headers, body, 15);
  if (!resp || resp->status != 200) {
    tm_http_response_free(resp);
    return -1;
  }

  /* 从 set-cookie 提取 gm_sid */
  *gm_sid_out = cg_extract_gm_sid(resp->cookies);

  cJSON *json = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!json)
    return -1;

  cJSON *auth = cJSON_GetObjectItemCaseSensitive(json, "auth");
  const char *token =
      cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(auth, "token"));
  if (!token || !token[0]) {
    cJSON_Delete(json);
    return -1;
  }

  *inbox_out = tm_strdup(token);
  cJSON_Delete(json);
  return 0;
}

static char *cg_pack_token(const char *gm_sid, const char *inbox) {
  cJSON *o = cJSON_CreateObject();
  if (!o)
    return NULL;
  cJSON_AddStringToObject(o, "gmSid", gm_sid ? gm_sid : "");
  cJSON_AddStringToObject(o, "inbox", inbox);
  char *s = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return s;
}

static int cg_parse_packed(const char *token, char **gm_sid_out,
                           char **inbox_out) {
  *gm_sid_out = NULL;
  *inbox_out = NULL;
  if (!token || !token[0])
    return -1;

  if (token[0] == '{') {
    cJSON *j = cJSON_Parse(token);
    if (j) {
      const char *gs =
          cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(j, "gmSid"));
      const char *ib =
          cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(j, "inbox"));
      if (gs && ib && ib[0]) {
        *gm_sid_out = tm_strdup(gs);
        *inbox_out = tm_strdup(ib);
        cJSON_Delete(j);
        return 0;
      }
      cJSON_Delete(j);
    }
  }

  *inbox_out = tm_strdup(token);
  return 0;
}

static tm_email_t *cg_fetch_emails(const char *inbox_token, const char *email,
                                   const char *gm_sid, int *count,
                                   long *status_out) {
  if (count)
    *count = -1;
  if (status_out)
    *status_out = 0;
  if (!inbox_token || !inbox_token[0])
    return NULL;

  char url[512];
  snprintf(url, sizeof(url), CG_BASE "/emails?email=%s", email);

  char token_hdr[2048];
  snprintf(token_hdr, sizeof(token_hdr), "x-inbox-token: %s", inbox_token);

  char cookie_hdr[512];
  snprintf(cookie_hdr, sizeof(cookie_hdr), "Cookie: gm_sid=%s",
           gm_sid ? gm_sid : "");

  const char *headers[] = {
      "Accept: */*",
      "Referer: https://mail.chatgpt.org.uk/zh/",
      "Origin: https://mail.chatgpt.org.uk",
      "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
      "(KHTML, like Gecko) Chrome/150.0.0.0 Safari/537.36 Edg/150.0.0.0",
      "DNT: 1",
      cookie_hdr,
      token_hdr,
      NULL};

  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, headers, NULL, 15);
  if (!resp)
    return NULL;
  if (status_out)
    *status_out = resp->status;
  if (resp->status != 200) {
    tm_http_response_free(resp);
    return NULL;
  }

  cJSON *json = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!json)
    return NULL;

  if (!cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(json, "success"))) {
    cJSON_Delete(json);
    return NULL;
  }

  cJSON *data = cJSON_GetObjectItemCaseSensitive(json, "data");
  cJSON *arr = cJSON_GetObjectItemCaseSensitive(data, "emails");
  int n = cJSON_IsArray(arr) ? cJSON_GetArraySize(arr) : 0;
  if (count)
    *count = n;
  if (n == 0) {
    cJSON_Delete(json);
    return NULL;
  }

  tm_email_t *emails = tm_emails_new(n);
  for (int i = 0; i < n; i++)
    emails[i] = tm_normalize_email(cJSON_GetArrayItem(arr, i), email);
  cJSON_Delete(json);
  return emails;
}

tm_email_info_t *tm_provider_chatgpt_org_uk_generate(void) {
  char *domain = cg_fetch_random_domain();
  if (!domain)
    return NULL;

  char username[11];
  cg_random_username(username, 10);

  char email[256];
  snprintf(email, sizeof(email), "%s@%s", username, domain);
  free(domain);

  char *inbox = NULL;
  char *gm_sid = NULL;
  if (cg_create_inbox(email, &inbox, &gm_sid) != 0 || !inbox) {
    free(inbox);
    free(gm_sid);
    return NULL;
  }

  char *packed = cg_pack_token(gm_sid, inbox);
  free(inbox);
  free(gm_sid);
  if (!packed)
    return NULL;

  tm_email_info_t *info = tm_email_info_new();
  info->channel = CHANNEL_CHATGPT_ORG_UK;
  info->email = tm_strdup(email);
  info->token = packed;
  return info;
}

tm_email_t *tm_provider_chatgpt_org_uk_get_emails(const char *token,
                                                  const char *email,
                                                  int *count) {
  *count = -1;
  if (!token || !token[0])
    return NULL;

  char *gm_sid = NULL;
  char *inbox = NULL;
  if (cg_parse_packed(token, &gm_sid, &inbox) != 0 || !inbox) {
    free(gm_sid);
    free(inbox);
    return NULL;
  }

  /* 如果 gm_sid 缺失，重新创建 inbox 获取 */
  if (!gm_sid || !gm_sid[0]) {
    free(gm_sid);
    free(inbox);
    gm_sid = NULL;
    inbox = NULL;
    if (cg_create_inbox(email, &inbox, &gm_sid) != 0) {
      free(gm_sid);
      free(inbox);
      return NULL;
    }
  }

  long status = 0;
  tm_email_t *emails = cg_fetch_emails(inbox, email, gm_sid, count, &status);

  /* 401/403 时重新创建 inbox 重试 */
  if (status == 401 || status == 403) {
    free(emails);
    char *new_inbox = NULL;
    char *new_sid = NULL;
    if (cg_create_inbox(email, &new_inbox, &new_sid) == 0) {
      emails = cg_fetch_emails(new_inbox, email, new_sid, count, &status);
      free(new_inbox);
      free(new_sid);
    } else {
      emails = NULL;
    }
  }

  free(gm_sid);
  free(inbox);
  return emails;
}
