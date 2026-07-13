/**
 * temp-mail-now 渠道
 *
 * Cookie Session + HTML 解析
 * 网站: temp-mail.now
 *
 * 创建邮箱流程:
 *   1. GET https://temp-mail.now/en/ -> 获取 session cookie
 *   2. POST https://temp-mail.now/change_email -> 获取新邮箱地址（携带 session
 * cookie）
 *   3. 从响应中提取邮箱地址
 *
 * 获取邮件流程:
 *   1. GET https://temp-mail.now/fetch_emails -> JSON 邮件列表（携带 session
 * cookie）
 *
 * token 存储策略:
 *   直接存储 session cookie 字符串
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TMN_BASE "https://temp-mail.now"
#define TMN_MAX_MAILS 128
#define TMN_MAX_COOKIE 8192

/* 公共 UA */
static const char *tmn_ua =
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36";

/* ========== Cookie 合并工具 ========== */

typedef struct {
  char k[128];
  char v[2048];
} tmn_ck_t;

static int tmn_ck_find(tmn_ck_t *tab, int n, const char *k) {
  for (int i = 0; i < n; i++) {
    if (strcmp(tab[i].k, k) == 0)
      return i;
  }
  return -1;
}

/**
 * 将 Cookie 串解析并入 tab（同名覆盖），返回新的 n
 */
static int tmn_ck_parse_merge(tmn_ck_t *tab, int n, int maxn, const char *hdr) {
  if (!hdr || !*hdr)
    return n;
  const char *p = hdr;
  while (*p) {
    while (*p == ' ' || *p == '\t' || *p == ';')
      p++;
    if (!*p)
      break;
    const char *semi = strchr(p, ';');
    const char *end = semi ? semi : p + strlen(p);
    const char *eq = memchr(p, '=', (size_t)(end - p));
    if (!eq || eq >= end) {
      p = semi ? semi + 1 : end;
      continue;
    }
    size_t klen = (size_t)(eq - p);
    while (klen > 0 && (p[klen - 1] == ' ' || p[klen - 1] == '\t'))
      klen--;
    const char *vs = eq + 1;
    size_t vlen = (size_t)(end - vs);
    while (vlen > 0 && (vs[vlen - 1] == ' ' || vs[vlen - 1] == '\t'))
      vlen--;
    if (klen == 0 || klen >= sizeof(tab[0].k)) {
      p = semi ? semi + 1 : end;
      continue;
    }
    char key[128];
    memcpy(key, p, klen);
    key[klen] = '\0';
    if (vlen >= sizeof(tab[0].v))
      vlen = sizeof(tab[0].v) - 1;
    int ix = tmn_ck_find(tab, n, key);
    if (ix < 0) {
      if (n >= maxn)
        return n;
      ix = n++;
      strcpy(tab[ix].k, key);
    }
    memcpy(tab[ix].v, vs, vlen);
    tab[ix].v[vlen] = '\0';
    p = semi ? semi + 1 : end;
  }
  return n;
}

static char *tmn_cookie_join(tmn_ck_t *tab, int n) {
  size_t need = 1;
  for (int i = 0; i < n; i++) {
    need += strlen(tab[i].k) + 1 + strlen(tab[i].v) + 2;
  }
  char *out = (char *)malloc(need);
  if (!out)
    return NULL;
  char *w = out;
  for (int i = 0; i < n; i++) {
    if (i > 0) {
      *w++ = ';';
      *w++ = ' ';
    }
    w += (size_t)sprintf(w, "%s=%s", tab[i].k, tab[i].v);
  }
  *w = '\0';
  return out;
}

/**
 * 合并两个 Cookie 头字符串，同名覆盖
 */
static char *tmn_cookie_merge(const char *a, const char *b) {
  tmn_ck_t tab[64];
  int n = 0;
  n = tmn_ck_parse_merge(tab, n, 64, a);
  n = tmn_ck_parse_merge(tab, n, 64, b);
  return tmn_cookie_join(tab, n);
}

/**
 * 从 HTML/JSON 响应中提取邮箱地址
 * 查找第一个包含 @ 的可能邮箱字符串
 */
static char *tmn_extract_email(const char *body) {
  if (!body)
    return NULL;

  /* 尝试 JSON 解析 */
  cJSON *json = cJSON_Parse(body);
  if (json) {
    const char *email_keys[] = {"email", "address", "mail"};
    for (int i = 0; i < 3; i++) {
      const cJSON *item = cJSON_GetObjectItemCaseSensitive(json, email_keys[i]);
      if (cJSON_IsString(item) && item->valuestring &&
          strchr(item->valuestring, '@')) {
        char *result = tm_strdup(item->valuestring);
        cJSON_Delete(json);
        return result;
      }
    }
    cJSON_Delete(json);
  }

  /* 尝试从 HTML 中查找邮箱地址模式 */
  const char *p = body;
  while ((p = strchr(p, '@')) != NULL) {
    /* 向前查找用户名部分 */
    const char *start = p;
    while (start > body) {
      char c = *(start - 1);
      if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-' ||
          c == '+') {
        start--;
      } else {
        break;
      }
    }
    /* 向后查找域名部分 */
    const char *end = p + 1;
    int has_dot = 0;
    while (*end) {
      char c = *end;
      if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '.' || c == '-') {
        if (c == '.')
          has_dot = 1;
        end++;
      } else {
        break;
      }
    }
    /* 验证: 有用户名、有域名点号、域名不以点结尾 */
    if (start < p && has_dot && end > p + 1 && *(end - 1) != '.') {
      size_t elen = (size_t)(end - start);
      char *email = (char *)malloc(elen + 1);
      if (email) {
        memcpy(email, start, elen);
        email[elen] = '\0';
        return email;
      }
    }
    p++;
  }

  return NULL;
}

/* ========== 创建邮箱 ========== */

/**
 * 创建 temp-mail-now 临时邮箱
 *
 * 流程:
 *   1. GET /en/ -> 获取 session cookie
 *   2. POST /change_email -> 获取新邮箱地址
 */
tm_email_info_t *tm_provider_temp_mail_now_generate(void) {
  int timeout =
      tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

  /* 第一步: GET /en/ 获取 session cookie */
  const char *hdrs_home[] = {
      tmn_ua,
      "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
      "Accept-Language: en-US,en;q=0.9", NULL};

  tm_http_response_t *r1 =
      tm_http_request(TM_HTTP_GET, TMN_BASE "/en/", hdrs_home, NULL, timeout);
  if (!r1 || (r1->status != 200 && r1->status != 302) || !r1->cookies) {
    TM_LOG_ERR("[temp-mail-now] 获取首页失败, status=%ld", r1 ? r1->status : 0);
    tm_http_response_free(r1);
    return NULL;
  }

  /* 保存 Cookie */
  char *ck = tm_strdup(r1->cookies);

  /* 尝试从首页响应中直接提取邮箱 */
  char *email_from_home = NULL;
  if (r1->body) {
    email_from_home = tmn_extract_email(r1->body);
  }
  tm_http_response_free(r1);

  if (!ck || !ck[0]) {
    TM_LOG_ERR("[temp-mail-now] 未获取到 session cookie");
    free(ck);
    free(email_from_home);
    return NULL;
  }

  /* 第二步: POST /change_email 获取新邮箱地址 */
  char h_ck[TMN_MAX_COOKIE];
  snprintf(h_ck, sizeof(h_ck), "Cookie: %s", ck);

  const char *hdrs_change[] = {
      tmn_ua,
      "Accept: application/json, text/html, */*;q=0.8",
      "Accept-Language: en-US,en;q=0.9",
      "Referer: " TMN_BASE "/en/",
      "Content-Type: application/x-www-form-urlencoded",
      h_ck,
      NULL};

  tm_http_response_t *r2 = tm_http_request(
      TM_HTTP_POST, TMN_BASE "/change_email", hdrs_change, "", timeout);
  if (!r2 || (r2->status != 200 && r2->status != 302)) {
    /* 如果 POST 失败，尝试使用首页提取的邮箱 */
    if (email_from_home) {
      TM_LOG_WARN("[temp-mail-now] POST /change_email 失败, 使用首页邮箱");
      tm_http_response_free(r2);
    } else {
      TM_LOG_ERR("[temp-mail-now] POST /change_email 失败, status=%ld",
                 r2 ? r2->status : 0);
      tm_http_response_free(r2);
      free(ck);
      return NULL;
    }
  }

  /* 合并 Cookie */
  if (r2 && r2->cookies && r2->cookies[0]) {
    char *merged = tmn_cookie_merge(ck, r2->cookies);
    free(ck);
    ck = merged;
  }

  /* 从 POST 响应中提取邮箱地址 */
  char *email_addr = NULL;
  if (r2 && r2->body && r2->body[0]) {
    email_addr = tmn_extract_email(r2->body);
  }
  tm_http_response_free(r2);

  /* 如果 POST 未返回邮箱，使用首页的 */
  if (!email_addr && email_from_home) {
    email_addr = email_from_home;
    email_from_home = NULL;
  }
  free(email_from_home);

  if (!email_addr || !strchr(email_addr, '@')) {
    TM_LOG_ERR("[temp-mail-now] 未能获取有效的邮箱地址");
    free(email_addr);
    free(ck);
    return NULL;
  }

  /* 构建结果 */
  tm_email_info_t *info = tm_email_info_new();
  if (!info) {
    free(email_addr);
    free(ck);
    return NULL;
  }

  info->channel = CHANNEL_TEMP_MAIL_NOW;
  info->email = email_addr;
  info->token = ck;
  info->expires_at = 0;
  info->created_at = tm_strdup("");

  TM_LOG_DBG("[temp-mail-now] 创建邮箱成功: %s", info->email);
  return info;
}

/* ========== 获取邮件 ========== */

/**
 * 获取 temp-mail-now 邮件列表
 *
 * 流程:
 *   1. GET /fetch_emails -> JSON 邮件数组（携带 session cookie）
 *
 * @param token  session cookie 字符串
 * @param email  完整邮箱地址
 * @param count  输出邮件数量，-1 表示请求失败
 */
tm_email_t *tm_provider_temp_mail_now_get_emails(const char *token,
                                                 const char *email,
                                                 int *count) {
  *count = -1;
  if (!token || !email)
    return NULL;

  int timeout =
      tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

  /* 构建请求头 */
  char h_ck[TMN_MAX_COOKIE];
  snprintf(h_ck, sizeof(h_ck), "Cookie: %s", token);

  const char *hdrs[] = {tmn_ua,
                        "Accept: application/json, */*;q=0.8",
                        "Accept-Language: en-US,en;q=0.9",
                        "Referer: " TMN_BASE "/en/",
                        "X-Requested-With: XMLHttpRequest",
                        h_ck,
                        NULL};

  tm_http_response_t *resp = tm_http_request(
      TM_HTTP_GET, TMN_BASE "/fetch_emails", hdrs, NULL, timeout);
  if (!resp || resp->status != 200 || !resp->body) {
    TM_LOG_ERR("[temp-mail-now] 获取邮件请求失败, status=%ld",
               resp ? resp->status : 0);
    tm_http_response_free(resp);
    return NULL;
  }

  /* 解析 JSON */
  cJSON *json = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!json) {
    TM_LOG_ERR("[temp-mail-now] 解析邮件列表 JSON 失败");
    return NULL;
  }

  /* 响应可能是数组或包含数组的对象 */
  cJSON *arr = json;
  if (!cJSON_IsArray(json)) {
    arr = cJSON_GetObjectItemCaseSensitive(json, "emails");
    if (!arr)
      arr = cJSON_GetObjectItemCaseSensitive(json, "mails");
    if (!arr)
      arr = cJSON_GetObjectItemCaseSensitive(json, "data");
    if (!arr || !cJSON_IsArray(arr)) {
      TM_LOG_ERR("[temp-mail-now] 响应中未找到邮件数组");
      cJSON_Delete(json);
      return NULL;
    }
  }

  int msg_count = cJSON_GetArraySize(arr);
  if (msg_count <= 0) {
    cJSON_Delete(json);
    *count = 0;
    return NULL;
  }

  if (msg_count > TMN_MAX_MAILS)
    msg_count = TMN_MAX_MAILS;

  tm_email_t *emails = tm_emails_new(msg_count);
  if (!emails) {
    cJSON_Delete(json);
    return NULL;
  }

  int valid = 0;
  for (int i = 0; i < msg_count; i++) {
    const cJSON *msg = cJSON_GetArrayItem(arr, i);
    if (!cJSON_IsObject(msg))
      continue;

    /* 构建标准化用的 cJSON 对象 */
    cJSON *raw = cJSON_CreateObject();
    if (!raw)
      continue;

    /* 提取字段并映射到标准化格式 */
    const char *id_keys[] = {"id", "_id", "mail_id"};
    char *id_str = tm_json_get_str(msg, id_keys, 3);
    cJSON_AddStringToObject(raw, "id", id_str);
    free(id_str);

    const char *from_keys[] = {"from", "from_address", "sender", "mail_from",
                               "mail_sender"};
    char *from_str = tm_json_get_str(msg, from_keys, 5);
    cJSON_AddStringToObject(raw, "from", from_str);
    free(from_str);

    cJSON_AddStringToObject(raw, "to", email);

    const char *subj_keys[] = {"subject", "mail_subject", "title"};
    char *subj_str = tm_json_get_str(msg, subj_keys, 3);
    cJSON_AddStringToObject(raw, "subject", subj_str);
    free(subj_str);

    const char *text_keys[] = {"text", "body_text", "text_body"};
    char *text_str = tm_json_get_str(msg, text_keys, 3);
    cJSON_AddStringToObject(raw, "text", text_str);
    free(text_str);

    const char *html_keys[] = {"html", "body_html", "html_body", "body"};
    char *html_str = tm_json_get_str(msg, html_keys, 4);
    cJSON_AddStringToObject(raw, "html", html_str);
    free(html_str);

    const char *date_keys[] = {"date", "created_at", "receivedAt", "timestamp"};
    char *date_str = tm_json_get_str(msg, date_keys, 4);
    cJSON_AddStringToObject(raw, "date", date_str);
    free(date_str);

    emails[valid] = tm_normalize_email(raw, email);
    cJSON_Delete(raw);
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
