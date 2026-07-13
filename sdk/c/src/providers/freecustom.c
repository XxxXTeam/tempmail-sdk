/**
 * freecustom.email 渠道
 *
 * 免注册临时邮箱：任意 local part @ 可用域名即时可收信，token 即邮箱地址本身。
 * 读信时动态获取匿名 JWT（有效期约 2 小时），无需持久化。
 *
 * 创建邮箱:
 *   GET https://api2.freecustom.email/domains （无需鉴权）
 *   返回:
 * {"success":true,"data":[{"domain":"...","tier":"free","expiring_soon":false},
 * ...]} 从 data 中筛选 tier=="free" 且 expiring_soon!=true
 * 的域名，随机选一个；若无则退回全量随机。 本地随机生成 10 位 [a-z0-9] local
 * part，email = local@domain。
 *
 * 获取邮件:
 *   1. POST https://www.freecustom.email/api/auth （无 body）→
 * {"token":"<JWT>"}
 *   2. GET
 * https://www.freecustom.email/api/public-mailbox?fullMailboxId=<email> Header:
 * Authorization: Bearer <JWT>, x-fce-client: web-client 返回列表:
 * {"success":true,"data":[{"id":"...","from":"...","subject":"...","date":"..."}]}
 *      列表项无正文。
 *   3. 对每封 GET .../api/public-mailbox?fullMailboxId=<email>&messageId=<id>
 * 补全 html/text； 失败则退回列表元数据。
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FREECUSTOM_SITE "https://www.freecustom.email"
#define FREECUSTOM_DOMAINS_URL "https://api2.freecustom.email/domains"
#define FREECUSTOM_MAX_MAILS 128

/* 公共 UA */
static const char *freecustom_ua =
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36";

/* 公共 Referer */
static const char *freecustom_referer = "Referer: " FREECUSTOM_SITE "/en";

/* URL 编码单个字符（RFC3986 未保留字符原样保留，其余百分号编码） */
static int freecustom_url_encode_char(char c, char *out) {
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
static char *freecustom_url_encode(const char *s) {
  if (!s)
    return NULL;
  size_t len = strlen(s);
  char *out = (char *)malloc(len * 3 + 1);
  if (!out)
    return NULL;
  size_t o = 0;
  for (size_t i = 0; i < len; i++) {
    o += (size_t)freecustom_url_encode_char(s[i], out + o);
  }
  out[o] = '\0';
  return out;
}

/* 随机生成邮箱前缀（小写字母数字） */
static void freecustom_random_local(char *out, size_t cap) {
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
 * 挑选一个当前可收信的域名（堆分配，调用者 free）。
 * 优先 tier=="free" 且 expiring_soon!=true 的域名；若无则退回全量列表随机。
 */
static char *freecustom_pick_domain(int timeout) {
  const char *hdrs[] = {freecustom_ua, "Accept: application/json",
                        freecustom_referer, NULL};

  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, FREECUSTOM_DOMAINS_URL, hdrs, NULL, timeout);
  if (!resp || resp->status != 200 || !resp->body) {
    TM_LOG_ERR("[freecustom] 获取域名列表失败, status=%ld",
               resp ? resp->status : 0);
    tm_http_response_free(resp);
    return NULL;
  }

  cJSON *json = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!json) {
    TM_LOG_ERR("[freecustom] 域名列表非 JSON");
    return NULL;
  }

  const cJSON *data = cJSON_GetObjectItemCaseSensitive(json, "data");
  if (!cJSON_IsArray(data)) {
    TM_LOG_WARN("[freecustom] 域名列表为空");
    cJSON_Delete(json);
    return NULL;
  }

  int n = cJSON_GetArraySize(data);
  if (n == 0) {
    TM_LOG_WARN("[freecustom] 域名列表为空");
    cJSON_Delete(json);
    return NULL;
  }

  int *pool = (int *)malloc((size_t)n * sizeof(int));
  if (!pool) {
    cJSON_Delete(json);
    return NULL;
  }

  /* 第一轮：收集 tier=="free" 且 expiring_soon!=true 且 domain 有效的条目 */
  int pool_n = 0;
  for (int i = 0; i < n; i++) {
    const cJSON *d = cJSON_GetArrayItem(data, i);
    if (!cJSON_IsObject(d))
      continue;
    const cJSON *dom = cJSON_GetObjectItemCaseSensitive(d, "domain");
    if (!cJSON_IsString(dom) || !dom->valuestring || !dom->valuestring[0])
      continue;
    const cJSON *tier = cJSON_GetObjectItemCaseSensitive(d, "tier");
    const cJSON *exp = cJSON_GetObjectItemCaseSensitive(d, "expiring_soon");
    if (cJSON_IsString(tier) && tier->valuestring &&
        strcmp(tier->valuestring, "free") == 0 && !cJSON_IsTrue(exp)) {
      pool[pool_n++] = i;
    }
  }

  /* 第二轮：若无可用域名，退回全量（仅要求 domain 有效） */
  if (pool_n == 0) {
    for (int i = 0; i < n; i++) {
      const cJSON *d = cJSON_GetArrayItem(data, i);
      if (!cJSON_IsObject(d))
        continue;
      const cJSON *dom = cJSON_GetObjectItemCaseSensitive(d, "domain");
      if (!cJSON_IsString(dom) || !dom->valuestring || !dom->valuestring[0])
        continue;
      pool[pool_n++] = i;
    }
  }

  if (pool_n == 0) {
    TM_LOG_WARN("[freecustom] 无可用域名");
    free(pool);
    cJSON_Delete(json);
    return NULL;
  }

  int picked = pool[rand() % pool_n];
  free(pool);

  const cJSON *chosen = cJSON_GetArrayItem(data, picked);
  const cJSON *dom = cJSON_GetObjectItemCaseSensitive(chosen, "domain");
  char *domain = tm_strdup(dom->valuestring);
  cJSON_Delete(json);
  return domain;
}

/**
 * 创建 freecustom.email 临时邮箱
 */
tm_email_info_t *tm_provider_freecustom_generate(void) {
  int timeout =
      tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

  char *domain = freecustom_pick_domain(timeout);
  if (!domain)
    return NULL;

  char local[16];
  freecustom_random_local(local, sizeof(local));

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
  info->channel = CHANNEL_FREECUSTOM;
  info->email = email_addr;
  info->token = tm_strdup(email_addr); /* token 即邮箱地址 */
  if (!info->token) {
    tm_free_email_info(info);
    return NULL;
  }
  info->expires_at = 0;
  info->created_at = tm_strdup("");

  TM_LOG_DBG("[freecustom] 创建邮箱成功: %s", info->email);
  return info;
}

/**
 * 获取匿名访问令牌（JWT，堆分配，调用者 free）
 * POST /api/auth → {"token":"<JWT>"}
 */
static char *freecustom_fetch_auth_token(int timeout) {
  const char *hdrs[] = {freecustom_ua, "Accept: application/json",
                        "Content-Type: application/json", freecustom_referer,
                        NULL};

  tm_http_response_t *resp = tm_http_request(
      TM_HTTP_POST, FREECUSTOM_SITE "/api/auth", hdrs, "", timeout);
  if (!resp || resp->status < 200 || resp->status >= 300 || !resp->body) {
    TM_LOG_ERR("[freecustom] 获取令牌失败, status=%ld",
               resp ? resp->status : 0);
    tm_http_response_free(resp);
    return NULL;
  }

  cJSON *json = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!json) {
    TM_LOG_ERR("[freecustom] 令牌响应非 JSON");
    return NULL;
  }

  const cJSON *tok = cJSON_GetObjectItemCaseSensitive(json, "token");
  if (!cJSON_IsString(tok) || !tok->valuestring || !tok->valuestring[0]) {
    TM_LOG_WARN("[freecustom] 令牌响应无效");
    cJSON_Delete(json);
    return NULL;
  }

  char *jwt = tm_strdup(tok->valuestring);
  cJSON_Delete(json);
  return jwt;
}

/**
 * 获取 freecustom.email 邮件列表
 */
tm_email_t *tm_provider_freecustom_get_emails(const char *token,
                                              const char *email, int *count) {
  *count = -1;
  const char *addr = (token && token[0]) ? token : email;
  if (!addr || !addr[0]) {
    TM_LOG_WARN("[freecustom] 缺少邮箱地址");
    return NULL;
  }

  int timeout =
      tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

  /* 步骤1: 获取匿名 JWT */
  char *jwt = freecustom_fetch_auth_token(timeout);
  if (!jwt)
    return NULL;

  size_t auth_cap = strlen(jwt) + 32;
  char *auth_hdr = (char *)malloc(auth_cap);
  if (!auth_hdr) {
    free(jwt);
    return NULL;
  }
  snprintf(auth_hdr, auth_cap, "Authorization: Bearer %s", jwt);
  free(jwt);

  const char *auth_hdrs[] = {
      freecustom_ua, "Accept: application/json", freecustom_referer,
      auth_hdr,      "x-fce-client: web-client", NULL};

  /* 步骤2: 获取邮件元数据列表 */
  char *enc_addr = freecustom_url_encode(addr);
  if (!enc_addr) {
    free(auth_hdr);
    return NULL;
  }

  size_t list_url_cap = strlen(FREECUSTOM_SITE) + strlen(enc_addr) + 48;
  char *list_url = (char *)malloc(list_url_cap);
  if (!list_url) {
    free(enc_addr);
    free(auth_hdr);
    return NULL;
  }
  snprintf(list_url, list_url_cap,
           FREECUSTOM_SITE "/api/public-mailbox?fullMailboxId=%s", enc_addr);

  tm_http_response_t *list_resp =
      tm_http_request(TM_HTTP_GET, list_url, auth_hdrs, NULL, timeout);
  free(list_url);
  if (!list_resp || list_resp->status < 200 || list_resp->status >= 300 ||
      !list_resp->body) {
    TM_LOG_ERR("[freecustom] 获取邮件失败, status=%ld",
               list_resp ? list_resp->status : 0);
    tm_http_response_free(list_resp);
    free(enc_addr);
    free(auth_hdr);
    return NULL;
  }

  cJSON *list_root = cJSON_Parse(list_resp->body);
  tm_http_response_free(list_resp);
  if (!list_root) {
    TM_LOG_ERR("[freecustom] 邮件列表解析失败");
    free(enc_addr);
    free(auth_hdr);
    return NULL;
  }

  const cJSON *j_success =
      cJSON_GetObjectItemCaseSensitive(list_root, "success");
  const cJSON *data = cJSON_GetObjectItemCaseSensitive(list_root, "data");
  if (!cJSON_IsTrue(j_success) || !cJSON_IsArray(data)) {
    cJSON_Delete(list_root);
    free(enc_addr);
    free(auth_hdr);
    *count = 0;
    return NULL;
  }

  int n = cJSON_GetArraySize(data);
  if (n == 0) {
    cJSON_Delete(list_root);
    free(enc_addr);
    free(auth_hdr);
    *count = 0;
    return NULL;
  }
  if (n > FREECUSTOM_MAX_MAILS)
    n = FREECUSTOM_MAX_MAILS;

  tm_email_t *emails = tm_emails_new(n);
  if (!emails) {
    cJSON_Delete(list_root);
    free(enc_addr);
    free(auth_hdr);
    return NULL;
  }

  /* 步骤3: 逐封补全正文 */
  int valid = 0;
  for (int i = 0; i < n; i++) {
    const cJSON *item = cJSON_GetArrayItem(data, i);
    if (!cJSON_IsObject(item))
      continue;

    const cJSON *id_item = cJSON_GetObjectItemCaseSensitive(item, "id");
    const char *msg_id = cJSON_IsString(id_item) ? id_item->valuestring : NULL;

    /* 默认使用列表元数据；若单封请求成功则替换为完整正文 */
    const cJSON *normalize_src = item;
    cJSON *msg_root = NULL;

    if (msg_id && msg_id[0]) {
      char *enc_id = freecustom_url_encode(msg_id);
      if (enc_id) {
        size_t msg_url_cap =
            strlen(FREECUSTOM_SITE) + strlen(enc_addr) + strlen(enc_id) + 64;
        char *msg_url = (char *)malloc(msg_url_cap);
        if (msg_url) {
          snprintf(msg_url, msg_url_cap,
                   FREECUSTOM_SITE
                   "/api/public-mailbox?fullMailboxId=%s&messageId=%s",
                   enc_addr, enc_id);
          tm_http_response_t *msg_resp =
              tm_http_request(TM_HTTP_GET, msg_url, auth_hdrs, NULL, timeout);
          free(msg_url);
          if (msg_resp && msg_resp->status >= 200 && msg_resp->status < 300 &&
              msg_resp->body) {
            msg_root = cJSON_Parse(msg_resp->body);
            if (msg_root) {
              const cJSON *msg_ok =
                  cJSON_GetObjectItemCaseSensitive(msg_root, "success");
              const cJSON *msg_data =
                  cJSON_GetObjectItemCaseSensitive(msg_root, "data");
              if (cJSON_IsTrue(msg_ok) && cJSON_IsObject(msg_data)) {
                normalize_src = msg_data;
              }
            }
          }
          tm_http_response_free(msg_resp);
        }
        free(enc_id);
      }
    }

    emails[valid] = tm_normalize_email(normalize_src, addr);
    valid++;

    if (msg_root)
      cJSON_Delete(msg_root);
  }

  cJSON_Delete(list_root);
  free(enc_addr);
  free(auth_hdr);

  if (valid == 0) {
    free(emails);
    *count = 0;
    return NULL;
  }

  *count = valid;
  return emails;
}
