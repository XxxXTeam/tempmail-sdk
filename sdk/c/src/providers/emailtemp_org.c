/**
 * emailtemp.org 临时邮箱渠道 — https://emailtemp.org（Laravel + CSRF）
 *
 * 创建邮箱:
 *   1. GET /en 获取 session cookie + 从 HTML meta 提取 CSRF token
 *   2. POST /messages（body: _token={csrf}&captcha=，头 X-CSRF-TOKEN）→
 * {mailbox, messages} 获取邮件:
 *   1. POST /messages（同上，带 session cookie）→ {mailbox, messages}
 *   2. 对每封邮件 GET /view/{id} 获取 HTML 正文
 * 该站点 reCAPTCHA 已禁用，captcha 参数传空即可。
 *
 * token 存储策略: "etpo1:" + base64(json{"csrf":"...","c":"cookie_header"})
 * （C 端无 cookie jar，需在 generate/get_emails 间手动复用 session cookie）
 */
#include "tempmail_internal.h"
#include <stdlib.h>
#include <string.h>

#define ETPO_BASE "https://emailtemp.org"
#define ETPO_TOK_PREFIX "etpo1:"
#define ETPO_MAX_COOKIE 8192
#define ETPO_MAX_MAILS 128

static const char *etpo_ua =
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";

/* ========== Base64 编解码 ========== */

static int etpo_b64_encode(const unsigned char *in, size_t inlen, char *out,
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
    if (i + 1 < inlen)
      out[o++] = T[(n >> 6) & 63];
    else
      out[o++] = '=';
    if (i + 2 < inlen)
      out[o++] = T[n & 63];
    else
      out[o++] = '=';
  }
  out[o] = '\0';
  return 0;
}

static int etpo_b64_val(int c) {
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

static unsigned char *etpo_b64_decode_alloc(const char *s, size_t *outlen) {
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
    int v0 = etpo_b64_val((unsigned char)s[i]);
    int v1 = etpo_b64_val((unsigned char)s[i + 1]);
    if (v0 < 0 || v1 < 0) {
      free(buf);
      return NULL;
    }
    unsigned triple = ((unsigned)v0 << 18) | ((unsigned)v1 << 12);
    if (s[i + 2] != '=') {
      int v2 = etpo_b64_val((unsigned char)s[i + 2]);
      if (v2 < 0) {
        free(buf);
        return NULL;
      }
      triple |= (unsigned)v2 << 6;
      if (s[i + 3] != '=') {
        int v3 = etpo_b64_val((unsigned char)s[i + 3]);
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

/* ========== Token 构建与解析 ========== */

static char *etpo_build_token(const char *csrf, const char *cookie_hdr) {
  cJSON *o = cJSON_CreateObject();
  if (!o)
    return NULL;
  cJSON_AddStringToObject(o, "csrf", csrf ? csrf : "");
  cJSON_AddStringToObject(o, "c", cookie_hdr ? cookie_hdr : "");
  char *json = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  if (!json)
    return NULL;
  size_t jl = strlen(json);
  size_t bcap = 4 * ((jl + 2) / 3) + 8 + strlen(ETPO_TOK_PREFIX);
  char *tok = (char *)malloc(bcap);
  if (!tok) {
    free(json);
    return NULL;
  }
  strcpy(tok, ETPO_TOK_PREFIX);
  if (etpo_b64_encode((const unsigned char *)json, jl,
                      tok + strlen(ETPO_TOK_PREFIX),
                      bcap - strlen(ETPO_TOK_PREFIX)) != 0) {
    free(json);
    free(tok);
    return NULL;
  }
  free(json);
  return tok;
}

static int etpo_parse_token(const char *tok, char **csrf, char **cookie_hdr) {
  *csrf = NULL;
  *cookie_hdr = NULL;
  if (!tok || strncmp(tok, ETPO_TOK_PREFIX, strlen(ETPO_TOK_PREFIX)) != 0)
    return -1;
  const char *b64 = tok + strlen(ETPO_TOK_PREFIX);
  size_t rawlen = 0;
  unsigned char *raw = etpo_b64_decode_alloc(b64, &rawlen);
  if (!raw)
    return -1;
  raw[rawlen] = '\0';
  cJSON *j = cJSON_Parse((char *)raw);
  free(raw);
  if (!j)
    return -1;
  const cJSON *jcsrf = cJSON_GetObjectItemCaseSensitive(j, "csrf");
  const cJSON *jc = cJSON_GetObjectItemCaseSensitive(j, "c");
  if (!cJSON_IsString(jcsrf) || !jcsrf->valuestring || !jcsrf->valuestring[0]) {
    cJSON_Delete(j);
    return -1;
  }
  *csrf = tm_strdup(jcsrf->valuestring);
  *cookie_hdr =
      tm_strdup(cJSON_IsString(jc) && jc->valuestring ? jc->valuestring : "");
  cJSON_Delete(j);
  return 0;
}

/* ========== CSRF 提取 ========== */

/* 从 meta 标签提取 CSRF token: <meta name="csrf-token" content="xxx"> */
static char *etpo_extract_csrf(const char *html) {
  if (!html)
    return NULL;
  const char *p = strstr(html, "name=\"csrf-token\"");
  if (!p)
    return NULL;
  const char *c = strstr(p, "content=\"");
  if (!c)
    return NULL;
  c += strlen("content=\"");
  const char *end = strchr(c, '"');
  if (!end || end == c)
    return NULL;
  size_t len = (size_t)(end - c);
  char *csrf = (char *)malloc(len + 1);
  if (!csrf)
    return NULL;
  memcpy(csrf, c, len);
  csrf[len] = '\0';
  return csrf;
}

/* ========== 邮箱地址提取 ========== */

/* 从 POST /messages 响应 JSON 中提取 mailbox 字段（含 @ 校验） */
static char *etpo_extract_mailbox(const char *body) {
  cJSON *j = cJSON_Parse(body);
  if (!j)
    return NULL;
  const char *mb =
      cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(j, "mailbox"));
  char *out = NULL;
  if (mb && strchr(mb, '@'))
    out = tm_strdup(mb);
  cJSON_Delete(j);
  return out;
}

/* ========== POST /messages 请求 ========== */

/*
 * 发送 POST /messages，返回响应体（调用者 free）
 * cookie_in 为已有 cookie（可为 NULL）
 */
static char *etpo_post_messages(const char *csrf, const char *cookie_in,
                                int timeout) {
  char form[512];
  snprintf(form, sizeof(form), "_token=%s&captcha=", csrf ? csrf : "");

  char h_csrf[256];
  snprintf(h_csrf, sizeof(h_csrf), "X-CSRF-TOKEN: %s", csrf ? csrf : "");
  char h_ck[ETPO_MAX_COOKIE];
  int has_cookie = (cookie_in && cookie_in[0]);
  if (has_cookie)
    snprintf(h_ck, sizeof(h_ck), "Cookie: %s", cookie_in);

  const char *hdr[] = {etpo_ua,
                       "Accept: application/json, text/javascript, */*; q=0.01",
                       "Accept-Language: en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
                       "X-Requested-With: XMLHttpRequest",
                       "Referer: " ETPO_BASE "/en",
                       "Content-Type: application/x-www-form-urlencoded",
                       h_csrf,
                       has_cookie ? h_ck : NULL,
                       NULL,
                       NULL};
  /* 若无 cookie，上面 has_cookie?h_ck:NULL 已经提前终止数组 */

  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_POST, ETPO_BASE "/messages", hdr, form, timeout);
  if (!resp || resp->status < 200 || resp->status >= 300 || !resp->body) {
    tm_http_response_free(resp);
    return NULL;
  }
  char *body = tm_strdup(resp->body);
  tm_http_response_free(resp);
  return body;
}

/* ========== 邮件正文获取 ========== */

/* GET /view/{id} 获取单封邮件 HTML 正文，失败返回 NULL */
static char *etpo_fetch_view(const char *id, const char *cookie_hdr,
                             int timeout) {
  if (!id || !id[0])
    return NULL;
  char url[512];
  snprintf(url, sizeof(url), "%s/view/%s", ETPO_BASE, id);

  char h_ck[ETPO_MAX_COOKIE];
  int has_cookie = (cookie_hdr && cookie_hdr[0]);
  if (has_cookie)
    snprintf(h_ck, sizeof(h_ck), "Cookie: %s", cookie_hdr);

  const char *hdr[] = {
      etpo_ua,
      "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
      "Accept-Language: en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
      "Referer: " ETPO_BASE "/en",
      has_cookie ? h_ck : NULL,
      NULL,
      NULL};

  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, hdr, NULL, timeout);
  if (!resp || resp->status < 200 || resp->status >= 300 || !resp->body) {
    tm_http_response_free(resp);
    return NULL;
  }
  char *body = tm_strdup(resp->body);
  tm_http_response_free(resp);
  return body;
}

/* ========== 创建邮箱 ========== */

tm_email_info_t *tm_provider_emailtemp_org_generate(void) {
  int timeout =
      tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

  /* 1. GET /en 获取 session cookie + CSRF */
  const char *hdr_home[] = {
      etpo_ua,
      "Accept: "
      "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/"
      "webp,image/apng,*/*;q=0.8",
      "Accept-Language: en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
      "Upgrade-Insecure-Requests: 1", NULL};
  tm_http_response_t *r1 =
      tm_http_request(TM_HTTP_GET, ETPO_BASE "/en", hdr_home, NULL, timeout);
  if (!r1 || r1->status != 200 || !r1->body) {
    tm_http_response_free(r1);
    return NULL;
  }
  char *csrf = etpo_extract_csrf(r1->body);
  char *ck = tm_strdup(r1->cookies ? r1->cookies : "");
  tm_http_response_free(r1);
  if (!csrf || !csrf[0]) {
    free(csrf);
    free(ck);
    return NULL;
  }

  /* 2. POST /messages 获取邮箱地址 */
  char *body = etpo_post_messages(csrf, ck, timeout);
  if (!body) {
    free(csrf);
    free(ck);
    return NULL;
  }
  char *email = etpo_extract_mailbox(body);
  free(body);
  if (!email) {
    free(csrf);
    free(ck);
    return NULL;
  }

  char *tok = etpo_build_token(csrf, ck);
  free(csrf);
  free(ck);
  if (!tok) {
    free(email);
    return NULL;
  }

  tm_email_info_t *info = tm_email_info_new();
  if (!info) {
    free(email);
    free(tok);
    return NULL;
  }
  info->channel = CHANNEL_EMAILTEMP_ORG;
  info->email = email;
  info->token = tok;
  return info;
}

/* ========== 获取邮件 ========== */

tm_email_t *tm_provider_emailtemp_org_get_emails(const char *token,
                                                 const char *email,
                                                 int *count) {
  *count = -1;
  if (!token || !email)
    return NULL;

  char *csrf = NULL;
  char *ck = NULL;
  if (etpo_parse_token(token, &csrf, &ck) != 0)
    return NULL;

  int timeout =
      tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

  char *body = etpo_post_messages(csrf, ck, timeout);
  free(csrf);
  if (!body) {
    free(ck);
    return NULL;
  }

  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) {
    free(ck);
    return NULL;
  }

  cJSON *messages = cJSON_GetObjectItemCaseSensitive(root, "messages");
  int n =
      (messages && cJSON_IsArray(messages)) ? cJSON_GetArraySize(messages) : 0;
  if (n == 0) {
    cJSON_Delete(root);
    free(ck);
    *count = 0;
    return NULL;
  }
  if (n > ETPO_MAX_MAILS)
    n = ETPO_MAX_MAILS;

  tm_email_t *emails = tm_emails_new(n);
  if (!emails) {
    cJSON_Delete(root);
    free(ck);
    return NULL;
  }

  int valid = 0;
  for (int i = 0; i < n; i++) {
    const cJSON *msg = cJSON_GetArrayItem(messages, i);
    if (!cJSON_IsObject(msg))
      continue;

    /* id 可能为数字或字符串 */
    const cJSON *j_id = cJSON_GetObjectItemCaseSensitive(msg, "id");
    char id_str[64] = {0};
    if (cJSON_IsString(j_id) && j_id->valuestring) {
      snprintf(id_str, sizeof(id_str), "%s", j_id->valuestring);
    } else if (cJSON_IsNumber(j_id)) {
      snprintf(id_str, sizeof(id_str), "%lld", (long long)j_id->valuedouble);
    }
    if (!id_str[0])
      continue;

    cJSON *raw = cJSON_CreateObject();
    if (!raw)
      continue;
    cJSON_AddStringToObject(raw, "id", id_str);
    cJSON_AddStringToObject(raw, "to", email);

    /* 发件人：from_email 为地址，from 为显示名，格式化为 "name <email>" */
    const char *from_email = cJSON_GetStringValue(
        cJSON_GetObjectItemCaseSensitive(msg, "from_email"));
    const char *from_name =
        cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(msg, "from"));
    if (from_name && from_email && from_name[0] &&
        strcmp(from_name, from_email) != 0) {
      char from_buf[512];
      snprintf(from_buf, sizeof(from_buf), "%s <%s>", from_name, from_email);
      cJSON_AddStringToObject(raw, "from", from_buf);
    } else if (from_email && from_email[0]) {
      cJSON_AddStringToObject(raw, "from", from_email);
    } else if (from_name) {
      cJSON_AddStringToObject(raw, "from", from_name);
    }

    const char *subject =
        cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(msg, "subject"));
    cJSON_AddStringToObject(raw, "subject", subject ? subject : "");

    const cJSON *seen = cJSON_GetObjectItemCaseSensitive(msg, "is_seen");
    if (cJSON_IsBool(seen))
      cJSON_AddBoolToObject(raw, "is_read", cJSON_IsTrue(seen));

    char *html = etpo_fetch_view(id_str, ck, timeout);
    if (html && html[0])
      cJSON_AddStringToObject(raw, "html", html);
    free(html);

    emails[valid] = tm_normalize_email(raw, email);
    cJSON_Delete(raw);
    valid++;
  }

  cJSON_Delete(root);
  free(ck);

  if (valid == 0) {
    free(emails);
    *count = 0;
    return NULL;
  }
  *count = valid;
  return emails;
}
