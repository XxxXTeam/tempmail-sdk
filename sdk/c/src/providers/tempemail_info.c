/**
 * tempemail.info 临时邮箱渠道 — https://tempemail.info
 *
 * 创建邮箱:
 *   1. GET / 获取 PHPSESSID 会话 Cookie
 *   2. 从 HTML 正则提取 var emailEncoded = "base64..."，base64 解码得邮箱地址
 * 获取邮件:
 *   1. POST /template/checker.php（body: last_id=0，带会话 Cookie）→
 * 邮件对象数组
 *   2. 对每封邮件 GET /view/{date}（date URL 编码）获取 HTML 正文
 *
 * token 存储会话 Cookie 串（明文，供后续请求绑定收件箱）
 */
#include "tempmail_internal.h"
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>

#define TEINFO_BASE "https://tempemail.info"
#define TEINFO_MAX_COOKIE 8192
#define TEINFO_MAX_MAILS 128

static const char *teinfo_ua =
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";

/* ========== Base64 解码（用于邮箱地址） ========== */

static int teinfo_b64_val(int c) {
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

static unsigned char *teinfo_b64_decode_alloc(const char *s, size_t *outlen) {
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
    int v0 = teinfo_b64_val((unsigned char)s[i]);
    int v1 = teinfo_b64_val((unsigned char)s[i + 1]);
    if (v0 < 0 || v1 < 0) {
      free(buf);
      return NULL;
    }
    unsigned triple = ((unsigned)v0 << 18) | ((unsigned)v1 << 12);
    if (s[i + 2] != '=') {
      int v2 = teinfo_b64_val((unsigned char)s[i + 2]);
      if (v2 < 0) {
        free(buf);
        return NULL;
      }
      triple |= (unsigned)v2 << 6;
      if (s[i + 3] != '=') {
        int v3 = teinfo_b64_val((unsigned char)s[i + 3]);
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

static char *teinfo_curl_escape(const char *s) {
  if (!s)
    return tm_strdup("");
  CURL *c = curl_easy_init();
  if (!c)
    return tm_strdup(s);
  char *e = curl_easy_escape(c, s, 0);
  curl_easy_cleanup(c);
  if (!e)
    return tm_strdup(s);
  char *d = tm_strdup(e);
  curl_free(e);
  return d;
}

/* ========== 邮箱地址提取 ========== */

/* 从 HTML 提取并解码 var emailEncoded = "base64..." */
static char *teinfo_extract_email(const char *html) {
  if (!html)
    return NULL;
  const char *p = strstr(html, "emailEncoded");
  if (!p)
    return NULL;
  const char *q = strchr(p, '"');
  if (!q)
    return NULL;
  q++;
  const char *end = strchr(q, '"');
  if (!end || end == q)
    return NULL;
  size_t len = (size_t)(end - q);
  char *b64 = (char *)malloc(len + 1);
  if (!b64)
    return NULL;
  memcpy(b64, q, len);
  b64[len] = '\0';

  size_t outlen = 0;
  unsigned char *decoded = teinfo_b64_decode_alloc(b64, &outlen);
  free(b64);
  if (!decoded)
    return NULL;
  char *email = tm_strdup((char *)decoded);
  free(decoded);
  if (!email || !strchr(email, '@')) {
    free(email);
    return NULL;
  }
  return email;
}

/* ========== 邮件正文获取 ========== */

/* GET /view/{date}（date URL 编码）获取 HTML 正文，失败返回 NULL */
static char *teinfo_fetch_body(const char *cookie_hdr, const char *date,
                               int timeout) {
  if (!date || !date[0])
    return NULL;
  char *enc = teinfo_curl_escape(date);
  size_t cap = strlen(TEINFO_BASE) + strlen(enc) + 16;
  char *url = (char *)malloc(cap);
  if (!url) {
    free(enc);
    return NULL;
  }
  snprintf(url, cap, "%s/view/%s", TEINFO_BASE, enc);
  free(enc);

  char h_ck[TEINFO_MAX_COOKIE];
  snprintf(h_ck, sizeof(h_ck), "Cookie: %s", cookie_hdr ? cookie_hdr : "");

  const char *hdr[] = {
      teinfo_ua,
      "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
      "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8",
      "Referer: " TEINFO_BASE "/",
      "Origin: " TEINFO_BASE,
      h_ck,
      NULL};

  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, hdr, NULL, timeout);
  free(url);
  if (!resp || resp->status < 200 || resp->status >= 300 || !resp->body) {
    tm_http_response_free(resp);
    return NULL;
  }
  char *body = tm_strdup(resp->body);
  tm_http_response_free(resp);
  return body;
}

/* ========== 创建邮箱 ========== */

tm_email_info_t *tm_provider_tempemail_info_generate(void) {
  int timeout =
      tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

  const char *hdr[] = {
      teinfo_ua,
      "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
      "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8",
      "Referer: " TEINFO_BASE "/",
      "Origin: " TEINFO_BASE,
      NULL};
  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, TEINFO_BASE "/", hdr, NULL, timeout);
  if (!resp || resp->status < 200 || resp->status >= 300 || !resp->body) {
    tm_http_response_free(resp);
    return NULL;
  }

  char *ck = tm_strdup(resp->cookies ? resp->cookies : "");
  char *email = teinfo_extract_email(resp->body);
  tm_http_response_free(resp);

  if (!ck || !ck[0] || !email) {
    free(ck);
    free(email);
    return NULL;
  }

  tm_email_info_t *info = tm_email_info_new();
  if (!info) {
    free(ck);
    free(email);
    return NULL;
  }
  info->channel = CHANNEL_TEMPEMAIL_INFO;
  info->email = email;
  info->token = ck;
  return info;
}

/* ========== 获取邮件 ========== */

tm_email_t *tm_provider_tempemail_info_get_emails(const char *token,
                                                  const char *email,
                                                  int *count) {
  *count = -1;
  if (!token || !token[0] || !email)
    return NULL;

  int timeout =
      tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

  char h_ck[TEINFO_MAX_COOKIE];
  snprintf(h_ck, sizeof(h_ck), "Cookie: %s", token);

  const char *hdr[] = {teinfo_ua,
                       "Accept: application/json, text/javascript, */*; q=0.01",
                       "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8",
                       "X-Requested-With: XMLHttpRequest",
                       "Content-Type: application/x-www-form-urlencoded",
                       "Referer: " TEINFO_BASE "/",
                       "Origin: " TEINFO_BASE,
                       h_ck,
                       NULL};

  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_POST, TEINFO_BASE "/template/checker.php", hdr,
                      "last_id=0", timeout);
  if (!resp || resp->status < 200 || resp->status >= 300 || !resp->body) {
    tm_http_response_free(resp);
    return NULL;
  }

  cJSON *list = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!list)
    return NULL;
  if (!cJSON_IsArray(list)) {
    cJSON_Delete(list);
    *count = 0;
    return NULL;
  }

  int n = cJSON_GetArraySize(list);
  if (n == 0) {
    cJSON_Delete(list);
    *count = 0;
    return NULL;
  }
  if (n > TEINFO_MAX_MAILS)
    n = TEINFO_MAX_MAILS;

  tm_email_t *emails = tm_emails_new(n);
  if (!emails) {
    cJSON_Delete(list);
    return NULL;
  }

  int valid = 0;
  for (int i = 0; i < n; i++) {
    const cJSON *row = cJSON_GetArrayItem(list, i);
    if (!cJSON_IsObject(row))
      continue;

    /* id 可能为数字或字符串 */
    const cJSON *j_id = cJSON_GetObjectItemCaseSensitive(row, "id");
    char id_str[64] = {0};
    if (cJSON_IsString(j_id) && j_id->valuestring) {
      snprintf(id_str, sizeof(id_str), "%s", j_id->valuestring);
    } else if (cJSON_IsNumber(j_id)) {
      snprintf(id_str, sizeof(id_str), "%lld", (long long)j_id->valuedouble);
    }

    const char *date =
        cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(row, "date"));

    cJSON *raw = cJSON_CreateObject();
    if (!raw)
      continue;
    cJSON_AddStringToObject(raw, "id", id_str);
    cJSON_AddStringToObject(raw, "to", email);

    /* 发件人：name 为显示名，from 为地址，格式化为 "name <from>" */
    const char *from =
        cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(row, "from"));
    const char *name =
        cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(row, "name"));
    if (name && from && name[0] && strcmp(name, from) != 0) {
      char from_buf[512];
      snprintf(from_buf, sizeof(from_buf), "%s <%s>", name, from);
      cJSON_AddStringToObject(raw, "from", from_buf);
    } else if (from && from[0]) {
      cJSON_AddStringToObject(raw, "from", from);
    } else if (name) {
      cJSON_AddStringToObject(raw, "from", name);
    }

    const char *subject =
        cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(row, "subject"));
    cJSON_AddStringToObject(raw, "subject", subject ? subject : "");
    if (date)
      cJSON_AddStringToObject(raw, "date", date);

    const cJSON *read = cJSON_GetObjectItemCaseSensitive(row, "read");
    if (cJSON_IsBool(read))
      cJSON_AddBoolToObject(raw, "is_read", cJSON_IsTrue(read));

    char *html = teinfo_fetch_body(token, date, timeout);
    if (html && html[0])
      cJSON_AddStringToObject(raw, "html", html);
    free(html);

    emails[valid] = tm_normalize_email(raw, email);
    cJSON_Delete(raw);
    valid++;
  }

  cJSON_Delete(list);
  if (valid == 0) {
    free(emails);
    *count = 0;
    return NULL;
  }
  *count = valid;
  return emails;
}
