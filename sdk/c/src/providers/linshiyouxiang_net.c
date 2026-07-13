/**
 * linshiyouxiang.net 临时邮箱渠道 — https://www.linshiyouxiang.net
 *
 * 创建邮箱:
 *   1. GET / 获取 temp_mail 会话 Cookie 及首页 HTML
 *   2. 从 HTML 正则提取 tempMailGlobal='...'（邮箱）和
 * mailCodeGlobal='...'（校验 code） 获取邮件: POST /get-messages  body:
 * {"email":"...","code":"<code>"}（带会话 Cookie） 响应:
 * {"emails":null|[...],"success":true}
 *
 * token 存储策略: "lxyx1:" + base64(json{"code":"...","c":"cookie_header"})
 * （C 端无 cookie jar，需在 generate/get_emails 间手动复用 temp_mail cookie）
 */
#include "tempmail_internal.h"
#include <stdlib.h>
#include <string.h>

#define LXYX_BASE "https://www.linshiyouxiang.net"
#define LXYX_TOK_PREFIX "lxyx1:"
#define LXYX_MAX_COOKIE 8192
#define LXYX_MAX_MAILS 128

static const char *lxyx_ua =
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";

/* ========== Base64 编解码 ========== */

static int lxyx_b64_encode(const unsigned char *in, size_t inlen, char *out,
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

static int lxyx_b64_val(int c) {
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

static unsigned char *lxyx_b64_decode_alloc(const char *s, size_t *outlen) {
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
    int v0 = lxyx_b64_val((unsigned char)s[i]);
    int v1 = lxyx_b64_val((unsigned char)s[i + 1]);
    if (v0 < 0 || v1 < 0) {
      free(buf);
      return NULL;
    }
    unsigned triple = ((unsigned)v0 << 18) | ((unsigned)v1 << 12);
    if (s[i + 2] != '=') {
      int v2 = lxyx_b64_val((unsigned char)s[i + 2]);
      if (v2 < 0) {
        free(buf);
        return NULL;
      }
      triple |= (unsigned)v2 << 6;
      if (s[i + 3] != '=') {
        int v3 = lxyx_b64_val((unsigned char)s[i + 3]);
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

static char *lxyx_build_token(const char *code, const char *cookie_hdr) {
  cJSON *o = cJSON_CreateObject();
  if (!o)
    return NULL;
  cJSON_AddStringToObject(o, "code", code ? code : "");
  cJSON_AddStringToObject(o, "c", cookie_hdr ? cookie_hdr : "");
  char *json = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  if (!json)
    return NULL;
  size_t jl = strlen(json);
  size_t bcap = 4 * ((jl + 2) / 3) + 8 + strlen(LXYX_TOK_PREFIX);
  char *tok = (char *)malloc(bcap);
  if (!tok) {
    free(json);
    return NULL;
  }
  strcpy(tok, LXYX_TOK_PREFIX);
  if (lxyx_b64_encode((const unsigned char *)json, jl,
                      tok + strlen(LXYX_TOK_PREFIX),
                      bcap - strlen(LXYX_TOK_PREFIX)) != 0) {
    free(json);
    free(tok);
    return NULL;
  }
  free(json);
  return tok;
}

static int lxyx_parse_token(const char *tok, char **code, char **cookie_hdr) {
  *code = NULL;
  *cookie_hdr = NULL;
  if (!tok || strncmp(tok, LXYX_TOK_PREFIX, strlen(LXYX_TOK_PREFIX)) != 0)
    return -1;
  const char *b64 = tok + strlen(LXYX_TOK_PREFIX);
  size_t rawlen = 0;
  unsigned char *raw = lxyx_b64_decode_alloc(b64, &rawlen);
  if (!raw)
    return -1;
  raw[rawlen] = '\0';
  cJSON *j = cJSON_Parse((char *)raw);
  free(raw);
  if (!j)
    return -1;
  const cJSON *jcode = cJSON_GetObjectItemCaseSensitive(j, "code");
  const cJSON *jc = cJSON_GetObjectItemCaseSensitive(j, "c");
  *code = tm_strdup(
      cJSON_IsString(jcode) && jcode->valuestring ? jcode->valuestring : "");
  *cookie_hdr =
      tm_strdup(cJSON_IsString(jc) && jc->valuestring ? jc->valuestring : "");
  cJSON_Delete(j);
  return 0;
}

/* ========== HTML 提取 ========== */

/* 提取形如 marker = 'value' 的单引号包裹值（marker 为
 * tempMailGlobal/mailCodeGlobal） */
static char *lxyx_extract_single_quoted(const char *html, const char *marker) {
  if (!html || !marker)
    return NULL;
  const char *p = strstr(html, marker);
  if (!p)
    return NULL;
  p += strlen(marker);
  /* 跳过空白、= 号 */
  while (*p == ' ' || *p == '\t' || *p == '=')
    p++;
  if (*p != '\'')
    return NULL;
  p++;
  const char *end = strchr(p, '\'');
  if (!end || end == p)
    return NULL;
  size_t len = (size_t)(end - p);
  char *out = (char *)malloc(len + 1);
  if (!out)
    return NULL;
  memcpy(out, p, len);
  out[len] = '\0';
  return out;
}

/* ========== 创建邮箱 ========== */

tm_email_info_t *tm_provider_linshiyouxiang_net_generate(void) {
  int timeout =
      tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

  const char *hdr[] = {
      lxyx_ua,
      "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
      "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8", NULL};
  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, LXYX_BASE "/", hdr, NULL, timeout);
  if (!resp || resp->status < 200 || resp->status >= 300 || !resp->body) {
    tm_http_response_free(resp);
    return NULL;
  }

  char *ck = tm_strdup(resp->cookies ? resp->cookies : "");
  char *email = lxyx_extract_single_quoted(resp->body, "tempMailGlobal");
  char *code = lxyx_extract_single_quoted(resp->body, "mailCodeGlobal");
  tm_http_response_free(resp);

  if (!email || !email[0]) {
    free(ck);
    free(email);
    free(code);
    return NULL;
  }
  if (!code)
    code = tm_strdup("");
  if (!ck)
    ck = tm_strdup("");

  char *tok = lxyx_build_token(code, ck);
  free(code);
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
  info->channel = CHANNEL_LINSHIYOUXIANG_NET;
  info->email = email;
  info->token = tok;
  return info;
}

/* ========== 获取邮件 ========== */

tm_email_t *tm_provider_linshiyouxiang_net_get_emails(const char *token,
                                                      const char *email,
                                                      int *count) {
  *count = -1;
  if (!token || !email || !email[0])
    return NULL;

  char *code = NULL;
  char *ck = NULL;
  if (lxyx_parse_token(token, &code, &ck) != 0)
    return NULL;

  int timeout =
      tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

  /* 构造请求体 {"email":"...","code":"..."} */
  cJSON *reqObj = cJSON_CreateObject();
  if (!reqObj) {
    free(code);
    free(ck);
    return NULL;
  }
  cJSON_AddStringToObject(reqObj, "email", email);
  cJSON_AddStringToObject(reqObj, "code", code ? code : "");
  char *reqBody = cJSON_PrintUnformatted(reqObj);
  cJSON_Delete(reqObj);
  free(code);
  if (!reqBody) {
    free(ck);
    return NULL;
  }

  char h_ck[LXYX_MAX_COOKIE];
  int has_cookie = (ck && ck[0]);
  if (has_cookie)
    snprintf(h_ck, sizeof(h_ck), "Cookie: %s", ck);

  const char *hdr[] = {lxyx_ua,
                       "Accept: application/json, text/javascript, */*; q=0.01",
                       "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8",
                       "Referer: " LXYX_BASE "/",
                       "Origin: " LXYX_BASE,
                       "Content-Type: application/json",
                       "X-Requested-With: XMLHttpRequest",
                       has_cookie ? h_ck : NULL,
                       NULL,
                       NULL};

  tm_http_response_t *resp = tm_http_request(
      TM_HTTP_POST, LXYX_BASE "/get-messages", hdr, reqBody, timeout);
  free(reqBody);
  free(ck);
  if (!resp || resp->status < 200 || resp->status >= 300 || !resp->body) {
    tm_http_response_free(resp);
    return NULL;
  }

  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!root)
    return NULL;

  cJSON *arr = cJSON_GetObjectItemCaseSensitive(root, "emails");
  int n = (arr && cJSON_IsArray(arr)) ? cJSON_GetArraySize(arr) : 0;
  if (n == 0) {
    cJSON_Delete(root);
    *count = 0;
    return NULL;
  }
  if (n > LXYX_MAX_MAILS)
    n = LXYX_MAX_MAILS;

  tm_email_t *emails = tm_emails_new(n);
  if (!emails) {
    cJSON_Delete(root);
    return NULL;
  }

  int valid = 0;
  for (int i = 0; i < n; i++) {
    cJSON *item = cJSON_GetArrayItem(arr, i);
    if (!cJSON_IsObject(item))
      continue;
    emails[valid] = tm_normalize_email(item, email);
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
