/**
 * rootsh.com (BccTo.CC) 渠道
 *
 * 创建邮箱流程:
 *   1. GET https://rootsh.com/ 获取 cookie session
 *   2. POST https://rootsh.com/applymail 申请邮箱
 *      响应:
 * {"delay":"10:00","tips":"","user":"xxx@bccto.cc","success":"true","time":1782762525}
 *
 * 获取邮件流程:
 *   POST https://rootsh.com/getmail 获取邮件列表
 *      响应:
 * {"to":"xxx@bccto.cc","mail":[["sender_name","sender_email","subject","date","fid","time"]],...}
 *
 * 查看邮件:
 *   POST https://rootsh.com/viewmail 获取邮件内容
 *      响应: {"mail":"html_content","attachment":"","success":"true"}
 *
 * token 存储 time 值，用于增量获取邮件
 * cookie session 编码在 token 中一并传递
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ROOTSH_BASE "https://rootsh.com"
#define ROOTSH_DEFAULT_DOMAIN "bccto.cc"
#define ROOTSH_TOK_PREFIX "rootsh1:"
#define ROOTSH_MAX_COOKIE 4096
#define ROOTSH_MAX_MAILS 128

/* URL 编码单个字符 */
static int rootsh_url_encode_char(char c, char *out) {
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

/* URL 编码字符串 */
static char *rootsh_url_encode(const char *s) {
  if (!s)
    return NULL;
  size_t len = strlen(s);
  /* 最坏情况每个字符编码为 3 字节 */
  char *out = (char *)malloc(len * 3 + 1);
  if (!out)
    return NULL;
  size_t o = 0;
  for (size_t i = 0; i < len; i++) {
    o += (size_t)rootsh_url_encode_char(s[i], out + o);
  }
  out[o] = '\0';
  return out;
}

/* 随机生成用户名 */
static void rootsh_random_user(char *out, size_t cap) {
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

/* base64 编码 */
static int rootsh_b64_encode(const unsigned char *in, size_t inlen, char *out,
                             size_t outcap) {
  static const char T[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t o = 0;
  for (size_t i = 0; i < inlen; i += 3) {
    unsigned n = (unsigned)in[i] << 16;
    if (i + 1 < inlen)
      n |= (unsigned)in[i + 1] << 8;
    if (i + 2 < inlen)
      n |= in[i + 2];
    if (o + 4 >= outcap)
      return -1;
    out[o++] = T[(n >> 18) & 63];
    out[o++] = T[(n >> 12) & 63];
    out[o++] = (i + 1 < inlen) ? T[(n >> 6) & 63] : '=';
    out[o++] = (i + 2 < inlen) ? T[n & 63] : '=';
  }
  out[o] = '\0';
  return 0;
}

/* base64 解码 */
static int rootsh_b64_val(int c) {
  if (c >= 'A' && c <= 'Z')
    return c - 'A';
  if (c >= 'a' && c <= 'z')
    return c - 'a' + 26;
  if (c >= '0' && c <= '9')
    return c - '0' + 52;
  if (c == '+')
    return 62;
  if (c == '/')
    return 63;
  return -1;
}

static unsigned char *rootsh_b64_decode_alloc(const char *s, size_t *outlen) {
  *outlen = 0;
  if (!s)
    return NULL;
  while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
    s++;
  size_t sl = strlen(s);
  while (sl > 0 && (s[sl - 1] == ' ' || s[sl - 1] == '\t'))
    sl--;
  if (sl == 0)
    return NULL;
  size_t maxo = (sl / 4) * 3 + 8;
  unsigned char *buf = (unsigned char *)malloc(maxo);
  if (!buf)
    return NULL;
  size_t o = 0;
  for (size_t i = 0; i + 3 < sl; i += 4) {
    int v0 = rootsh_b64_val((unsigned char)s[i]);
    int v1 = rootsh_b64_val((unsigned char)s[i + 1]);
    if (v0 < 0 || v1 < 0) {
      free(buf);
      return NULL;
    }
    unsigned triple = ((unsigned)v0 << 18) | ((unsigned)v1 << 12);
    if (s[i + 2] != '=') {
      int v2 = rootsh_b64_val((unsigned char)s[i + 2]);
      if (v2 < 0) {
        free(buf);
        return NULL;
      }
      triple |= (unsigned)v2 << 6;
      if (s[i + 3] != '=') {
        int v3 = rootsh_b64_val((unsigned char)s[i + 3]);
        if (v3 < 0) {
          free(buf);
          return NULL;
        }
        triple |= (unsigned)v3;
        if (o + 3 > maxo) {
          free(buf);
          return NULL;
        }
        buf[o++] = (unsigned char)((triple >> 16) & 255);
        buf[o++] = (unsigned char)((triple >> 8) & 255);
        buf[o++] = (unsigned char)(triple & 255);
      } else {
        if (o + 2 > maxo) {
          free(buf);
          return NULL;
        }
        buf[o++] = (unsigned char)((triple >> 16) & 255);
        buf[o++] = (unsigned char)((triple >> 8) & 255);
      }
    } else {
      if (o + 1 > maxo) {
        free(buf);
        return NULL;
      }
      buf[o++] = (unsigned char)((triple >> 16) & 255);
    }
  }
  buf[o] = '\0';
  *outlen = o;
  return buf;
}

/**
 * 构建 token: "rootsh1:" + base64(json)
 * json: {"c":"cookie_str","t":time_value}
 */
static char *rootsh_build_token(const char *cookie_hdr, long long time_val) {
  cJSON *o = cJSON_CreateObject();
  if (!o)
    return NULL;
  cJSON_AddStringToObject(o, "c", cookie_hdr ? cookie_hdr : "");
  cJSON_AddNumberToObject(o, "t", (double)time_val);
  char *json = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  if (!json)
    return NULL;
  size_t jl = strlen(json);
  size_t bcap = 4 * ((jl + 2) / 3) + 8 + strlen(ROOTSH_TOK_PREFIX);
  char *tok = (char *)malloc(bcap);
  if (!tok) {
    free(json);
    return NULL;
  }
  strcpy(tok, ROOTSH_TOK_PREFIX);
  if (rootsh_b64_encode((const unsigned char *)json, jl,
                        tok + strlen(ROOTSH_TOK_PREFIX),
                        bcap - strlen(ROOTSH_TOK_PREFIX)) != 0) {
    free(json);
    free(tok);
    return NULL;
  }
  free(json);
  return tok;
}

/**
 * 解析 token，提取 cookie 和 time 值
 * 成功返回 0，失败返回 -1
 */
static int rootsh_parse_token(const char *tok, char **cookie_hdr,
                              long long *time_val) {
  *cookie_hdr = NULL;
  *time_val = 0;
  if (!tok || strncmp(tok, ROOTSH_TOK_PREFIX, strlen(ROOTSH_TOK_PREFIX)) != 0)
    return -1;
  const char *b64 = tok + strlen(ROOTSH_TOK_PREFIX);
  size_t rawlen = 0;
  unsigned char *raw = rootsh_b64_decode_alloc(b64, &rawlen);
  if (!raw)
    return -1;
  raw[rawlen] = '\0';
  cJSON *j = cJSON_Parse((char *)raw);
  free(raw);
  if (!j)
    return -1;
  const cJSON *jc = cJSON_GetObjectItemCaseSensitive(j, "c");
  const cJSON *jt = cJSON_GetObjectItemCaseSensitive(j, "t");
  if (!cJSON_IsString(jc) || !jc->valuestring) {
    cJSON_Delete(j);
    return -1;
  }
  *cookie_hdr = tm_strdup(jc->valuestring);
  *time_val = cJSON_IsNumber(jt) ? (long long)jt->valuedouble : 0;
  cJSON_Delete(j);
  if (!*cookie_hdr)
    return -1;
  return 0;
}

/**
 * 创建 rootsh 临时邮箱
 *
 * 流程:
 *   1. GET https://rootsh.com/ 获取 session cookie
 *   2. POST https://rootsh.com/applymail 申请邮箱
 */
tm_email_info_t *tm_provider_rootsh_generate(void) {
  int timeout =
      tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

  /* 第一步: GET / 获取 cookie session */
  const char *hdr_home[] = {
      "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
      "AppleWebKit/537.36 "
      "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
      "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
      "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8", NULL};

  tm_http_response_t *r1 =
      tm_http_request(TM_HTTP_GET, ROOTSH_BASE "/", hdr_home, NULL, timeout);
  if (!r1 || (r1->status != 200 && r1->status != 302)) {
    TM_LOG_ERR("[rootsh] 获取首页失败, status=%ld", r1 ? r1->status : 0);
    tm_http_response_free(r1);
    return NULL;
  }
  char *cookie = tm_strdup(r1->cookies ? r1->cookies : "");
  tm_http_response_free(r1);
  if (!cookie)
    return NULL;

  /* 第二步: POST /applymail 申请邮箱 */
  char user[16];
  rootsh_random_user(user, sizeof(user));
  char post_body[256];
  snprintf(post_body, sizeof(post_body), "mail=%s%%40%s", user,
           ROOTSH_DEFAULT_DOMAIN);

  char h_cookie[ROOTSH_MAX_COOKIE];
  snprintf(h_cookie, sizeof(h_cookie), "Cookie: %s", cookie);

  const char *hdr_apply[] = {
      "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
      "AppleWebKit/537.36 "
      "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
      "Accept: application/json, text/javascript, */*; q=0.01",
      "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8",
      "Content-Type: application/x-www-form-urlencoded; charset=UTF-8",
      "X-Requested-With: XMLHttpRequest",
      "Referer: " ROOTSH_BASE "/",
      h_cookie,
      NULL};

  tm_http_response_t *r2 = tm_http_request(
      TM_HTTP_POST, ROOTSH_BASE "/applymail", hdr_apply, post_body, timeout);
  if (!r2 || r2->status != 200 || !r2->body) {
    TM_LOG_ERR("[rootsh] 申请邮箱失败, status=%ld", r2 ? r2->status : 0);
    tm_http_response_free(r2);
    free(cookie);
    return NULL;
  }

  /* 合并新返回的 cookie */
  if (r2->cookies && r2->cookies[0]) {
    size_t clen = strlen(cookie) + strlen(r2->cookies) + 4;
    char *merged = (char *)malloc(clen);
    if (merged) {
      if (cookie[0]) {
        snprintf(merged, clen, "%s; %s", cookie, r2->cookies);
      } else {
        strcpy(merged, r2->cookies);
      }
      free(cookie);
      cookie = merged;
    }
  }

  /* 解析响应 JSON */
  cJSON *json = cJSON_Parse(r2->body);
  tm_http_response_free(r2);
  if (!json) {
    TM_LOG_ERR("[rootsh] 解析 applymail 响应 JSON 失败");
    free(cookie);
    return NULL;
  }

  /* 校验 success 字段 */
  const cJSON *j_success = cJSON_GetObjectItemCaseSensitive(json, "success");
  if (!cJSON_IsString(j_success) ||
      strcmp(j_success->valuestring, "true") != 0) {
    TM_LOG_ERR("[rootsh] applymail 返回失败");
    cJSON_Delete(json);
    free(cookie);
    return NULL;
  }

  /* 提取 user（邮箱地址） */
  const cJSON *j_user = cJSON_GetObjectItemCaseSensitive(json, "user");
  if (!cJSON_IsString(j_user) || !j_user->valuestring ||
      !j_user->valuestring[0]) {
    TM_LOG_ERR("[rootsh] applymail 响应缺少 user 字段");
    cJSON_Delete(json);
    free(cookie);
    return NULL;
  }
  char *email_addr = tm_strdup(j_user->valuestring);

  /* 提取 time 值（用于增量获取邮件） */
  long long time_val = 0;
  const cJSON *j_time = cJSON_GetObjectItemCaseSensitive(json, "time");
  if (cJSON_IsNumber(j_time)) {
    time_val = (long long)j_time->valuedouble;
  }

  cJSON_Delete(json);

  /* 构建 token */
  char *tok = rootsh_build_token(cookie, time_val);
  free(cookie);
  if (!tok) {
    free(email_addr);
    return NULL;
  }

  tm_email_info_t *info = tm_email_info_new();
  if (!info) {
    free(email_addr);
    free(tok);
    return NULL;
  }
  info->channel = CHANNEL_ROOTSH;
  info->email = email_addr;
  info->token = tok;
  info->expires_at = 0;
  info->created_at = tm_strdup("");
  return info;
}

/**
 * 获取 rootsh 邮件列表
 *
 * 流程:
 *   1. POST /getmail 获取邮件列表（增量，基于 time 值）
 *   2. 对每封邮件 POST /viewmail 获取 HTML 内容
 */
tm_email_t *tm_provider_rootsh_get_emails(const char *token, const char *email,
                                          int *count) {
  *count = -1;
  if (!token || !email)
    return NULL;

  /* 解析 token */
  char *cookie = NULL;
  long long last_time = 0;
  if (rootsh_parse_token(token, &cookie, &last_time) != 0) {
    TM_LOG_ERR("[rootsh] 解析 token 失败");
    return NULL;
  }

  int timeout =
      tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

  /* 构建 getmail 请求 */
  char *encoded_email = rootsh_url_encode(email);
  if (!encoded_email) {
    free(cookie);
    return NULL;
  }

  long long now_ms = (long long)time(NULL) * 1000LL;
  char post_body[512];
  snprintf(post_body, sizeof(post_body), "mail=%s&time=%lld&_=%lld",
           encoded_email, last_time, now_ms);
  free(encoded_email);

  char h_cookie[ROOTSH_MAX_COOKIE];
  snprintf(h_cookie, sizeof(h_cookie), "Cookie: %s", cookie);

  const char *hdr_getmail[] = {
      "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
      "AppleWebKit/537.36 "
      "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
      "Accept: application/json, text/javascript, */*; q=0.01",
      "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8",
      "Content-Type: application/x-www-form-urlencoded; charset=UTF-8",
      "X-Requested-With: XMLHttpRequest",
      "Referer: " ROOTSH_BASE "/",
      h_cookie,
      NULL};

  tm_http_response_t *resp = tm_http_request(
      TM_HTTP_POST, ROOTSH_BASE "/getmail", hdr_getmail, post_body, timeout);
  if (!resp || resp->status != 200 || !resp->body) {
    TM_LOG_ERR("[rootsh] getmail 请求失败, status=%ld",
               resp ? resp->status : 0);
    tm_http_response_free(resp);
    free(cookie);
    return NULL;
  }

  cJSON *json = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!json) {
    TM_LOG_ERR("[rootsh] 解析 getmail 响应 JSON 失败");
    free(cookie);
    return NULL;
  }

  /* 校验 success */
  const cJSON *j_success = cJSON_GetObjectItemCaseSensitive(json, "success");
  if (!cJSON_IsString(j_success) ||
      strcmp(j_success->valuestring, "true") != 0) {
    TM_LOG_ERR("[rootsh] getmail 返回失败");
    cJSON_Delete(json);
    free(cookie);
    return NULL;
  }

  /* 解析 mail 数组 */
  const cJSON *j_mail = cJSON_GetObjectItemCaseSensitive(json, "mail");
  if (!cJSON_IsArray(j_mail)) {
    /* 无邮件 */
    cJSON_Delete(json);
    free(cookie);
    *count = 0;
    return NULL;
  }

  int mail_count = cJSON_GetArraySize(j_mail);
  if (mail_count <= 0) {
    cJSON_Delete(json);
    free(cookie);
    *count = 0;
    return NULL;
  }
  if (mail_count > ROOTSH_MAX_MAILS)
    mail_count = ROOTSH_MAX_MAILS;

  tm_email_t *emails = tm_emails_new(mail_count);
  if (!emails) {
    cJSON_Delete(json);
    free(cookie);
    return NULL;
  }

  int valid_count = 0;
  for (int i = 0; i < mail_count; i++) {
    const cJSON *item = cJSON_GetArrayItem(j_mail, i);
    if (!cJSON_IsArray(item) || cJSON_GetArraySize(item) < 6)
      continue;

    /* mail 数组元素: [display_name, from_email, subject, date_str, mail_fid,
     * received_time] */
    const cJSON *j_display_name = cJSON_GetArrayItem(item, 0);
    const cJSON *j_from_email = cJSON_GetArrayItem(item, 1);
    const cJSON *j_subject = cJSON_GetArrayItem(item, 2);
    const cJSON *j_date = cJSON_GetArrayItem(item, 3);
    const cJSON *j_fid = cJSON_GetArrayItem(item, 4);

    const char *from_email_str = TM_JSON_STR(j_from_email, "");
    const char *subject_str = TM_JSON_STR(j_subject, "");
    const char *date_str = TM_JSON_STR(j_date, "");
    const char *fid_str = TM_JSON_STR(j_fid, "");
    (void)j_display_name; /* display_name 暂不使用 */

    if (!fid_str || !fid_str[0])
      continue;

    /* POST /viewmail 获取邮件内容 */
    char *encoded_fid = rootsh_url_encode(fid_str);
    char *encoded_to = rootsh_url_encode(email);
    if (!encoded_fid || !encoded_to) {
      free(encoded_fid);
      free(encoded_to);
      continue;
    }

    char view_body[512];
    snprintf(view_body, sizeof(view_body), "mail=%s&to=%s", encoded_fid,
             encoded_to);
    free(encoded_fid);
    free(encoded_to);

    const char *hdr_view[] = {
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
        "AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
        "Accept: application/json, text/javascript, */*; q=0.01",
        "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8",
        "Content-Type: application/x-www-form-urlencoded; charset=UTF-8",
        "X-Requested-With: XMLHttpRequest",
        "Referer: " ROOTSH_BASE "/",
        h_cookie,
        NULL};

    tm_http_response_t *vresp = tm_http_request(
        TM_HTTP_POST, ROOTSH_BASE "/viewmail", hdr_view, view_body, timeout);
    char *html_content = tm_strdup("");
    if (vresp && vresp->status == 200 && vresp->body) {
      cJSON *vjson = cJSON_Parse(vresp->body);
      if (vjson) {
        const cJSON *j_vmail = cJSON_GetObjectItemCaseSensitive(vjson, "mail");
        if (cJSON_IsString(j_vmail) && j_vmail->valuestring) {
          free(html_content);
          html_content = tm_strdup(j_vmail->valuestring);
        }
        cJSON_Delete(vjson);
      }
    }
    tm_http_response_free(vresp);

    /* 构建标准化邮件 JSON */
    cJSON *raw = cJSON_CreateObject();
    if (!raw) {
      free(html_content);
      continue;
    }
    cJSON_AddStringToObject(raw, "id", fid_str);
    cJSON_AddStringToObject(raw, "from", from_email_str);
    cJSON_AddStringToObject(raw, "to", email);
    cJSON_AddStringToObject(raw, "subject", subject_str);
    cJSON_AddStringToObject(raw, "date", date_str);
    cJSON_AddStringToObject(raw, "html", html_content ? html_content : "");
    cJSON_AddBoolToObject(raw, "isRead", 0);
    cJSON_AddItemToObject(raw, "attachments", cJSON_CreateArray());
    free(html_content);

    emails[valid_count] = tm_normalize_email(raw, email);
    cJSON_Delete(raw);
    valid_count++;
  }

  cJSON_Delete(json);
  free(cookie);

  if (valid_count == 0) {
    free(emails);
    *count = 0;
    return NULL;
  }

  *count = valid_count;
  return emails;
}
