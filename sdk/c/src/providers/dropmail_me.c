/**
 * dropmail-me 渠道 — https://dropmail.me
 *
 * 流程：GET /en/ 提取 data-k → 反转 + base64 解码得 secret
 *       FNV-1a 生成 auth token
 *       GraphQL introduceSession 创建会话
 *       GraphQL session(id) 获取邮件
 * token 存储 JSON: {"session_id":"...","auth_token":"website_..."}
 */

#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DROPMAIL_ME_BASE "https://dropmail.me"

static const char *dropmail_me_headers[] = {
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
    "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
    NULL};

static const char *dropmail_me_json_headers[] = {
    "Content-Type: application/json",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
    NULL};

/* Base64 解码表 */
static const unsigned char b64_table[256] = {
    ['A'] = 0,  ['B'] = 1,  ['C'] = 2,  ['D'] = 3,  ['E'] = 4,  ['F'] = 5,
    ['G'] = 6,  ['H'] = 7,  ['I'] = 8,  ['J'] = 9,  ['K'] = 10, ['L'] = 11,
    ['M'] = 12, ['N'] = 13, ['O'] = 14, ['P'] = 15, ['Q'] = 16, ['R'] = 17,
    ['S'] = 18, ['T'] = 19, ['U'] = 20, ['V'] = 21, ['W'] = 22, ['X'] = 23,
    ['Y'] = 24, ['Z'] = 25, ['a'] = 26, ['b'] = 27, ['c'] = 28, ['d'] = 29,
    ['e'] = 30, ['f'] = 31, ['g'] = 32, ['h'] = 33, ['i'] = 34, ['j'] = 35,
    ['k'] = 36, ['l'] = 37, ['m'] = 38, ['n'] = 39, ['o'] = 40, ['p'] = 41,
    ['q'] = 42, ['r'] = 43, ['s'] = 44, ['t'] = 45, ['u'] = 46, ['v'] = 47,
    ['w'] = 48, ['x'] = 49, ['y'] = 50, ['z'] = 51, ['0'] = 52, ['1'] = 53,
    ['2'] = 54, ['3'] = 55, ['4'] = 56, ['5'] = 57, ['6'] = 58, ['7'] = 59,
    ['8'] = 60, ['9'] = 61, ['+'] = 62, ['/'] = 63};

static size_t dropmail_me_b64_decode(const char *src, size_t src_len,
                                     char *dst, size_t dst_cap) {
  size_t out = 0;
  unsigned int buf = 0;
  int bits = 0;
  for (size_t i = 0; i < src_len && out < dst_cap; i++) {
    unsigned char c = (unsigned char)src[i];
    if (c == '=' || c == '\n' || c == '\r')
      continue;
    buf = (buf << 6) | b64_table[c];
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      dst[out++] = (char)((buf >> bits) & 0xFF);
    }
  }
  if (out < dst_cap)
    dst[out] = '\0';
  return out;
}

/* FNV-1a 哈希（与前端 JS 一致） */
static void dropmail_me_fnv_hash(const char *s, char *out, size_t out_cap) {
  unsigned int hash = 2166136261u;
  for (size_t i = 0; s[i]; i++) {
    hash ^= (unsigned int)(unsigned char)s[i];
    hash += (hash << 1) + (hash << 4) + (hash << 7) + (hash << 8) +
            (hash << 24);
  }
  snprintf(out, out_cap, "%x", hash);
}

/* 提取 data-k：在 HTML 中查找 data-k="..." */
static int dropmail_me_extract_data_k(const char *html, char *out,
                                      size_t out_cap) {
  const char *needle = "data-k=\"";
  const char *p = strstr(html, needle);
  if (!p)
    return -1;
  p += strlen(needle);
  const char *end = strchr(p, '"');
  if (!end)
    return -1;
  size_t len = (size_t)(end - p);
  if (len >= out_cap)
    len = out_cap - 1;
  memcpy(out, p, len);
  out[len] = '\0';
  return 0;
}

/* 反转字符串 */
static void dropmail_me_reverse(char *s, size_t len) {
  for (size_t i = 0, j = len - 1; i < j; i++, j--) {
    char tmp = s[i];
    s[i] = s[j];
    s[j] = tmp;
  }
}

/* 生成 YYYYMMDD + 16位随机字母数字 */
static void dropmail_me_random_part(char *buf, size_t cap) {
  static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
  time_t now = time(NULL);
  struct tm t;
  gmtime_r(&now, &t);
  int off = snprintf(buf, cap, "%04d%02d%02d", t.tm_year + 1900, t.tm_mon + 1,
                     t.tm_mday);
  for (int i = 0; i < 16 && (size_t)(off + i + 1) < cap; i++) {
    buf[off + i] = chars[rand() % (sizeof(chars) - 1)];
  }
  buf[off + 16] = '\0';
}

/**
 * 创建 dropmail.me 临时邮箱
 */
tm_email_info_t *tm_provider_dropmail_me_generate(void) {
  /* 1. 获取首页，提取 data-k */
  char url[128];
  snprintf(url, sizeof(url), "%s/en/", DROPMAIL_ME_BASE);

  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, dropmail_me_headers, NULL, 15);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    TM_LOG_ERR("dropmail-me: 获取首页失败");
    tm_http_response_free(resp);
    return NULL;
  }

  char data_k[512];
  if (dropmail_me_extract_data_k(resp->body, data_k, sizeof(data_k)) < 0) {
    TM_LOG_ERR("dropmail-me: 未找到 data-k");
    tm_http_response_free(resp);
    return NULL;
  }
  tm_http_response_free(resp);

  /* 2. 反转 + base64 解码得 secret */
  size_t dk_len = strlen(data_k);
  dropmail_me_reverse(data_k, dk_len);

  char secret[512];
  dropmail_me_b64_decode(data_k, dk_len, secret, sizeof(secret));

  /* 3. 生成 auth token: website_ + random_part + _ + fnv_hash(random_part + secret) */
  char random_part[32];
  dropmail_me_random_part(random_part, sizeof(random_part));

  char hash_input[1024];
  snprintf(hash_input, sizeof(hash_input), "%s%s", random_part, secret);

  char hash_str[16];
  dropmail_me_fnv_hash(hash_input, hash_str, sizeof(hash_str));

  char auth_token[128];
  snprintf(auth_token, sizeof(auth_token), "website_%s_%s", random_part,
           hash_str);

  /* 4. GraphQL introduceSession */
  char api_url[256];
  snprintf(api_url, sizeof(api_url), "%s/api/graphql/%s", DROPMAIL_ME_BASE,
           auth_token);

  const char *query =
      "{\"query\":\"mutation { introduceSession { id expiresAt addresses { "
      "address } } }\"}";

  resp = tm_http_request(TM_HTTP_POST, api_url, dropmail_me_json_headers, query,
                         15);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    TM_LOG_ERR("dropmail-me: GraphQL introduceSession 失败");
    tm_http_response_free(resp);
    return NULL;
  }

  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!root) {
    TM_LOG_ERR("dropmail-me: 解析 GraphQL 响应失败");
    return NULL;
  }

  cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
  cJSON *session = cJSON_GetObjectItemCaseSensitive(data, "introduceSession");
  if (!session) {
    TM_LOG_ERR("dropmail-me: 无 introduceSession 数据");
    cJSON_Delete(root);
    return NULL;
  }

  const char *session_id =
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(session, "id"), "");
  const char *expires_at =
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(session, "expiresAt"), "");

  cJSON *addrs = cJSON_GetObjectItemCaseSensitive(session, "addresses");
  if (!cJSON_IsArray(addrs) || cJSON_GetArraySize(addrs) == 0) {
    TM_LOG_ERR("dropmail-me: 无可用邮箱地址");
    cJSON_Delete(root);
    return NULL;
  }

  cJSON *first_addr = cJSON_GetArrayItem(addrs, 0);
  const char *email_addr =
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(first_addr, "address"), "");

  if (!session_id[0] || !email_addr[0]) {
    TM_LOG_ERR("dropmail-me: session_id 或 address 为空");
    cJSON_Delete(root);
    return NULL;
  }

  /* 构造 token JSON */
  cJSON *token_obj = cJSON_CreateObject();
  cJSON_AddStringToObject(token_obj, "session_id", session_id);
  cJSON_AddStringToObject(token_obj, "auth_token", auth_token);
  char *token_str = cJSON_PrintUnformatted(token_obj);
  cJSON_Delete(token_obj);

  tm_email_info_t *info = tm_email_info_new();
  if (!info) {
    cJSON_Delete(root);
    free(token_str);
    return NULL;
  }

  info->email = tm_strdup(email_addr);
  info->token = tm_strdup(token_str);
  info->expires_at = tm_strdup(expires_at);
  free(token_str);
  cJSON_Delete(root);
  return info;
}

/**
 * 获取 dropmail.me 邮件列表
 */
tm_email_t *tm_provider_dropmail_me_get_emails(const char *token,
                                               const char *email, int *count) {
  *count = 0;
  if (!token || !token[0])
    return NULL;

  /* 解析 token JSON */
  cJSON *token_obj = cJSON_Parse(token);
  if (!token_obj)
    return NULL;

  const char *session_id =
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(token_obj, "session_id"),
                  "");
  const char *auth_token =
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(token_obj, "auth_token"),
                  "");

  if (!session_id[0] || !auth_token[0]) {
    cJSON_Delete(token_obj);
    return NULL;
  }

  /* 构造 GraphQL 查询 */
  char query_buf[512];
  snprintf(query_buf, sizeof(query_buf),
           "{\"query\":\"{ session(id:\\\"%s\\\") { mails { id headerFrom "
           "headerSubject text html receivedAt } } }\"}",
           session_id);

  char api_url[256];
  snprintf(api_url, sizeof(api_url), "%s/api/graphql/%s", DROPMAIL_ME_BASE,
           auth_token);

  tm_http_response_t *resp = tm_http_request(
      TM_HTTP_POST, api_url, dropmail_me_json_headers, query_buf, 15);
  cJSON_Delete(token_obj);

  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }

  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!root)
    return NULL;

  cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
  cJSON *session = cJSON_GetObjectItemCaseSensitive(data, "session");
  cJSON *mails = cJSON_GetObjectItemCaseSensitive(session, "mails");
  if (!cJSON_IsArray(mails) || cJSON_GetArraySize(mails) == 0) {
    cJSON_Delete(root);
    return NULL;
  }

  int n = cJSON_GetArraySize(mails);
  *count = n;
  tm_email_t *emails = tm_emails_new(n);
  if (!emails) {
    *count = -1;
    cJSON_Delete(root);
    return NULL;
  }

  for (int i = 0; i < n; i++) {
    cJSON *msg = cJSON_GetArrayItem(mails, i);
    cJSON *raw = cJSON_CreateObject();
    cJSON_AddStringToObject(
        raw, "id",
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(msg, "id"), ""));
    cJSON_AddStringToObject(
        raw, "from",
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(msg, "headerFrom"), ""));
    cJSON_AddStringToObject(raw, "to", email ? email : "");
    cJSON_AddStringToObject(
        raw, "subject",
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(msg, "headerSubject"),
                    ""));
    cJSON_AddStringToObject(
        raw, "text",
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(msg, "text"), ""));
    cJSON_AddStringToObject(
        raw, "html",
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(msg, "html"), ""));
    cJSON_AddStringToObject(
        raw, "date",
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(msg, "receivedAt"), ""));

    emails[i] = tm_normalize_email(raw, email);
    cJSON_Delete(raw);
  }

  cJSON_Delete(root);
  return emails;
}
