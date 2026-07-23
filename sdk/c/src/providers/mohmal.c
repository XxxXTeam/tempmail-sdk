/**
 * mohmal.com：HTML 收件箱 + connect.sid Session Cookie
 * 创建邮箱：GET /en/create/random -> 跟随重定向到 /en/inbox
 * 获取邮件列表：GET /en/inbox（解析 HTML 表格）
 * 查看邮件详情：GET /en/message/{id}（解析 HTML 内容）
 */
#include "tempmail_internal.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MOHMAL_ORIGIN "https://www.mohmal.com"
#define MOHMAL_TOK_PREFIX "mohmal1:"
#define MOHMAL_MAX_COOKIE 8192
#define MOHMAL_MAX_IDS 64

#ifdef _WIN32
#define mohmal_strncasecmp _strnicmp
#else
#define mohmal_strncasecmp strncasecmp
#endif

/* ========== Cookie 工具 ========== */

typedef struct {
  char k[128];
  char v[2048];
} mohmal_ck_t;

static int mohmal_ck_find(mohmal_ck_t *tab, int n, const char *k) {
  for (int i = 0; i < n; i++) {
    if (strcmp(tab[i].k, k) == 0)
      return i;
  }
  return -1;
}

/* 将 Cookie 串解析并入 tab（同名覆盖），返回新的 n */
static int mohmal_ck_parse_merge(mohmal_ck_t *tab, int n, int maxn,
                                 const char *hdr) {
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
    const char *ks = p;
    const char *vs = eq + 1;
    size_t klen = (size_t)(eq - ks);
    while (klen > 0 && (ks[klen - 1] == ' ' || ks[klen - 1] == '\t'))
      klen--;
    size_t vlen = (size_t)(end - vs);
    while (vlen > 0 && (vs[vlen - 1] == ' ' || vs[vlen - 1] == '\t'))
      vlen--;
    if (klen == 0 || klen >= sizeof(tab[0].k)) {
      p = semi ? semi + 1 : end;
      continue;
    }
    char key[128];
    memcpy(key, ks, klen);
    key[klen] = '\0';
    if (vlen >= sizeof(tab[0].v))
      vlen = sizeof(tab[0].v) - 1;
    int ix = mohmal_ck_find(tab, n, key);
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

static char *mohmal_cookie_join(mohmal_ck_t *tab, int n) {
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

/* 合并两个 Cookie 头字符串，同名覆盖 */
static char *mohmal_cookie_merge(const char *a, const char *b) {
  mohmal_ck_t tab[128];
  int n = 0;
  n = mohmal_ck_parse_merge(tab, n, 128, a);
  n = mohmal_ck_parse_merge(tab, n, 128, b);
  return mohmal_cookie_join(tab, n);
}

/* 检查 Cookie 中是否包含 connect.sid */
static int mohmal_cookie_has_session(const char *hdr) {
  if (!hdr)
    return 0;
  const char *p = hdr;
  const char name[] = "connect.sid";
  size_t nl = sizeof(name) - 1;
  while (*p) {
    while (*p == ' ' || *p == '\t')
      p++;
    if (mohmal_strncasecmp(p, name, (int)nl) == 0 && p[nl] == '=')
      return 1;
    const char *semi = strchr(p, ';');
    if (!semi)
      break;
    p = semi + 1;
  }
  return 0;
}

/* ========== Base64 编解码 ========== */

static int mohmal_b64_encode(const unsigned char *in, size_t inlen, char *out,
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

static int mohmal_b64_val(int c) {
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

static unsigned char *mohmal_b64_decode_alloc(const char *s, size_t *outlen) {
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
    int v0 = mohmal_b64_val((unsigned char)s[i]);
    int v1 = mohmal_b64_val((unsigned char)s[i + 1]);
    if (v0 < 0 || v1 < 0) {
      free(buf);
      return NULL;
    }
    unsigned triple = ((unsigned)v0 << 18) | ((unsigned)v1 << 12);
    if (s[i + 2] != '=') {
      int v2 = mohmal_b64_val((unsigned char)s[i + 2]);
      if (v2 < 0) {
        free(buf);
        return NULL;
      }
      triple |= (unsigned)v2 << 6;
      if (s[i + 3] != '=') {
        int v3 = mohmal_b64_val((unsigned char)s[i + 3]);
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

/* token 格式：mohmal1:<base64(json)>，json 内含 cookie 头 */
static char *mohmal_build_token(const char *cookie_hdr) {
  cJSON *o = cJSON_CreateObject();
  if (!o)
    return NULL;
  cJSON_AddStringToObject(o, "c", cookie_hdr ? cookie_hdr : "");
  char *json = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  if (!json)
    return NULL;
  size_t jl = strlen(json);
  size_t bcap = 4 * ((jl + 2) / 3) + 8 + strlen(MOHMAL_TOK_PREFIX);
  char *tok = (char *)malloc(bcap);
  if (!tok) {
    free(json);
    return NULL;
  }
  strcpy(tok, MOHMAL_TOK_PREFIX);
  if (mohmal_b64_encode((const unsigned char *)json, jl,
                        tok + strlen(MOHMAL_TOK_PREFIX),
                        bcap - strlen(MOHMAL_TOK_PREFIX)) != 0) {
    free(json);
    free(tok);
    return NULL;
  }
  free(json);
  return tok;
}

/* 从 token 中解析出 cookie 头 */
static int mohmal_parse_token(const char *tok, char **cookie_hdr) {
  *cookie_hdr = NULL;
  if (!tok || strncmp(tok, MOHMAL_TOK_PREFIX, strlen(MOHMAL_TOK_PREFIX)) != 0)
    return -1;
  const char *b64 = tok + strlen(MOHMAL_TOK_PREFIX);
  size_t rawlen = 0;
  unsigned char *raw = mohmal_b64_decode_alloc(b64, &rawlen);
  if (!raw)
    return -1;
  raw[rawlen] = '\0';
  cJSON *j = cJSON_Parse((char *)raw);
  free(raw);
  if (!j)
    return -1;
  const cJSON *jc = cJSON_GetObjectItemCaseSensitive(j, "c");
  if (!cJSON_IsString(jc) || !jc->valuestring || !jc->valuestring[0]) {
    cJSON_Delete(j);
    return -1;
  }
  *cookie_hdr = tm_strdup(jc->valuestring);
  cJSON_Delete(j);
  if (!*cookie_hdr || !(*cookie_hdr)[0]) {
    free(*cookie_hdr);
    *cookie_hdr = NULL;
    return -1;
  }
  return 0;
}

/* ========== HTML 解析工具 ========== */

/* 从 HTML 中提取 data-email="..." 属性值 */
static int mohmal_parse_data_email(const char *html, char *out, size_t cap) {
  const char *attr = strstr(html, "data-email=\"");
  if (!attr)
    return -1;
  attr += strlen("data-email=\"");
  const char *end = strchr(attr, '"');
  if (!end)
    return -1;
  size_t n = (size_t)(end - attr);
  if (n == 0 || n >= cap)
    return -1;
  memcpy(out, attr, n);
  out[n] = '\0';
  /* 验证包含 @ 符号 */
  if (!strchr(out, '@'))
    return -1;
  return 0;
}

/* 去除 HTML 标签，保留纯文本 */
static void mohmal_strip_tags(const char *in, char *out, size_t cap) {
  size_t o = 0;
  int in_tag = 0;
  for (const char *p = in; *p && o + 1 < cap; p++) {
    if (*p == '<') {
      in_tag = 1;
      continue;
    }
    if (*p == '>') {
      in_tag = 0;
      continue;
    }
    if (!in_tag) {
      if (*p == '\n' || *p == '\r')
        out[o++] = ' ';
      else
        out[o++] = *p;
    }
  }
  /* 去除尾部空格 */
  while (o > 0 && out[o - 1] == ' ')
    o--;
  out[o] = '\0';
}

/* 去除字符串首尾空白 */
static char *mohmal_trim(char *s) {
  while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
    s++;
  char *end = s + strlen(s);
  while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' ||
                     end[-1] == '\r'))
    end--;
  *end = '\0';
  return s;
}

/* 从收件箱 HTML 中收集邮件 ID（/en/message/{id} 链接） */
static void mohmal_collect_ids(const char *html, char ids[][64], int *nids,
                               int maxids) {
  *nids = 0;
  const char *needle = "/en/message/";
  size_t needle_len = strlen(needle);
  const char *p = html;
  while (*p && *nids < maxids) {
    const char *h = strstr(p, needle);
    if (!h)
      break;
    const char *start = h + needle_len;
    /* 提取 ID（到引号或斜杠为止） */
    const char *end = start;
    while (*end && *end != '"' && *end != '\'' && *end != '/' && *end != ' ' &&
           *end != '>')
      end++;
    size_t id_len = (size_t)(end - start);
    if (id_len == 0 || id_len >= 64) {
      p = end;
      continue;
    }
    char uid[64];
    memcpy(uid, start, id_len);
    uid[id_len] = '\0';
    /* 去重 */
    int dup = 0;
    for (int i = 0; i < *nids; i++) {
      if (strcmp(ids[i], uid) == 0) {
        dup = 1;
        break;
      }
    }
    if (!dup)
      strcpy(ids[(*nids)++], uid);
    p = end;
  }
}

/* 从收件箱表格行中解析基本信息（发件人、主题、时间） */
typedef struct {
  char from[512];
  char subject[512];
  char date[256];
} mohmal_row_info_t;

/* 解析 #inbox-table 中特定 ID 对应行的基本信息 */
static int mohmal_parse_row_info(const char *html, const char *id,
                                 mohmal_row_info_t *info) {
  memset(info, 0, sizeof(*info));
  /* 找到包含此 ID 链接的行 */
  char id_needle[128];
  snprintf(id_needle, sizeof(id_needle), "/en/message/%s", id);
  const char *link = strstr(html, id_needle);
  if (!link)
    return -1;
  /* 回溯到 <tr */
  const char *tr_start = link;
  while (tr_start > html) {
    if (tr_start[0] == '<' && (tr_start[1] == 't' || tr_start[1] == 'T') &&
        (tr_start[2] == 'r' || tr_start[2] == 'R'))
      break;
    tr_start--;
  }
  const char *tr_end = strstr(link, "</tr>");
  if (!tr_end)
    tr_end = html + strlen(html);

  /* 解析 <td> 单元格 */
  int td_idx = 0;
  const char *p = tr_start;
  while (p < tr_end && td_idx < 4) {
    const char *td = strstr(p, "<td");
    if (!td || td >= tr_end)
      break;
    const char *td_gt = strchr(td, '>');
    if (!td_gt || td_gt >= tr_end)
      break;
    td_gt++;
    const char *td_close = strstr(td_gt, "</td>");
    if (!td_close || td_close > tr_end)
      td_close = tr_end;
    size_t cell_len = (size_t)(td_close - td_gt);
    char cell_buf[1024];
    if (cell_len >= sizeof(cell_buf))
      cell_len = sizeof(cell_buf) - 1;
    memcpy(cell_buf, td_gt, cell_len);
    cell_buf[cell_len] = '\0';
    char stripped[512];
    mohmal_strip_tags(cell_buf, stripped, sizeof(stripped));
    char *trimmed = mohmal_trim(stripped);
    switch (td_idx) {
    case 0: /* 发件人 */
      strncpy(info->from, trimmed, sizeof(info->from) - 1);
      break;
    case 1: /* 主题 */
      strncpy(info->subject, trimmed, sizeof(info->subject) - 1);
      break;
    case 2: /* 时间 */
      strncpy(info->date, trimmed, sizeof(info->date) - 1);
      break;
    default:
      break;
    }
    td_idx++;
    p = td_close + 5;
  }
  return 0;
}

/* 从邮件详情页面提取 HTML 内容 */
static char *mohmal_extract_message_body(const char *page) {
  /* 尝试查找 .mail-body 或 #message-body 等常见容器 */
  const char *markers[] = {
      "class=\"mail-body\"",  "class='mail-body'",   "class=\"message-body\"",
      "class='message-body'", "id=\"message-body\"", "id='message-body'",
      "class=\"email-body\"", "class='email-body'",  NULL};
  const char *start = NULL;
  for (int i = 0; markers[i]; i++) {
    start = strstr(page, markers[i]);
    if (start)
      break;
  }
  if (!start) {
    /* 备用：整个页面 body 作为内容 */
    const char *body = strstr(page, "<body");
    if (body) {
      body = strchr(body, '>');
      if (body) {
        body++;
        const char *end_body = strstr(body, "</body>");
        if (end_body) {
          size_t n = (size_t)(end_body - body);
          char *s = (char *)malloc(n + 1);
          if (s) {
            memcpy(s, body, n);
            s[n] = '\0';
            return s;
          }
        }
      }
    }
    return tm_strdup("");
  }
  /* 找到容器的开始 > */
  const char *gt = strchr(start, '>');
  if (!gt)
    return tm_strdup("");
  gt++;
  /* 栈式深度匹配以支持嵌套 div，避免非贪婪截断 */
  const char *pos = gt;
  const char *page_end = page + strlen(page);
  int depth = 1;
  const char *end = NULL;
  while (pos < page_end && depth > 0) {
    const char *next_open = strstr(pos, "<div");
    const char *next_close = strstr(pos, "</div>");
    if (!next_close)
      break;
    if (next_open && next_open < next_close) {
      depth++;
      pos = next_open + 4;
    } else {
      depth--;
      if (depth == 0) {
        end = next_close;
        break;
      }
      pos = next_close + 6;
    }
  }
  if (!end)
    end = strstr(gt, "</section>");
  if (!end)
    end = page + strlen(page);
  size_t n = (size_t)(end - gt);
  char *s = (char *)malloc(n + 1);
  if (!s)
    return tm_strdup("");
  memcpy(s, gt, n);
  s[n] = '\0';
  return s;
}

/* ========== 公共请求头 ========== */

static const char *mohmal_ua =
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";

/* ========== 创建邮箱 ========== */

tm_email_info_t *tm_provider_mohmal_generate(void) {
  int timeout =
      tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

  /* 第一步：GET /en/create/random，获取 session cookie 并重定向到 /en/inbox */
  const char *create_url = MOHMAL_ORIGIN "/en/create/random";
  const char *hdr_create[] = {mohmal_ua,
                              "Accept: "
                              "text/html,application/xhtml+xml,application/"
                              "xml;q=0.9,image/avif,image/webp,*/*;q=0.8",
                              "Accept-Language: en-US,en;q=0.9",
                              "Cache-Control: no-cache",
                              "DNT: 1",
                              "Pragma: no-cache",
                              "Upgrade-Insecure-Requests: 1",
                              NULL};

  tm_http_response_t *r1 =
      tm_http_request(TM_HTTP_GET, create_url, hdr_create, NULL, timeout);
  if (!r1 || (r1->status != 200 && r1->status != 302)) {
    TM_LOG_ERR("mohmal: 创建邮箱请求失败，状态码: %ld", r1 ? r1->status : 0);
    tm_http_response_free(r1);
    return NULL;
  }

  char *ck = tm_strdup(r1->cookies ? r1->cookies : "");
  char *body = r1->body ? tm_strdup(r1->body) : NULL;
  tm_http_response_free(r1);

  /* 如果重定向后没有拿到页面内容或 cookie，尝试用 cookie 再请求 /en/inbox */
  if (!body || !strstr(body, "data-email")) {
    free(body);
    body = NULL;

    if (!mohmal_cookie_has_session(ck)) {
      TM_LOG_ERR("mohmal: 未获取到 connect.sid session cookie");
      free(ck);
      return NULL;
    }

    char h_ck[MOHMAL_MAX_COOKIE];
    snprintf(h_ck, sizeof(h_ck), "Cookie: %s", ck);
    const char *hdr_inbox[] = {mohmal_ua,
                               "Accept: "
                               "text/html,application/xhtml+xml,application/"
                               "xml;q=0.9,image/avif,image/webp,*/*;q=0.8",
                               "Accept-Language: en-US,en;q=0.9",
                               "Cache-Control: no-cache",
                               "DNT: 1",
                               "Pragma: no-cache",
                               "Upgrade-Insecure-Requests: 1",
                               "Referer: " MOHMAL_ORIGIN "/en",
                               h_ck,
                               NULL};
    tm_http_response_t *r2 = tm_http_request(
        TM_HTTP_GET, MOHMAL_ORIGIN "/en/inbox", hdr_inbox, NULL, timeout);
    if (!r2 || r2->status != 200 || !r2->body) {
      TM_LOG_ERR("mohmal: 获取收件箱页面失败");
      tm_http_response_free(r2);
      free(ck);
      return NULL;
    }
    char *ck2 = mohmal_cookie_merge(ck, r2->cookies ? r2->cookies : "");
    free(ck);
    ck = ck2;
    body = tm_strdup(r2->body);
    tm_http_response_free(r2);
  }

  if (!body) {
    free(ck);
    return NULL;
  }

  /* 从页面中提取邮箱地址 */
  char email_buf[256];
  if (mohmal_parse_data_email(body, email_buf, sizeof(email_buf)) != 0) {
    TM_LOG_ERR("mohmal: 无法从页面中提取邮箱地址");
    free(body);
    free(ck);
    return NULL;
  }
  free(body);

  /* 构建 token */
  char *tok = mohmal_build_token(ck);
  free(ck);
  if (!tok)
    return NULL;

  tm_email_info_t *info = tm_email_info_new();
  if (!info) {
    free(tok);
    return NULL;
  }
  info->channel = CHANNEL_MOHMAL;
  info->email = tm_strdup(email_buf);
  info->token = tok;
  if (!info->email) {
    tm_free_email_info(info);
    return NULL;
  }

  TM_LOG_DBG("mohmal: 创建邮箱成功: %s", email_buf);
  return info;
}

/* ========== 获取邮件 ========== */

tm_email_t *tm_provider_mohmal_get_emails(const char *token, const char *email,
                                          int *count) {
  *count = -1;
  if (!token || !email)
    return NULL;

  char *ck = NULL;
  if (mohmal_parse_token(token, &ck) != 0) {
    TM_LOG_ERR("mohmal: token 解析失败");
    return NULL;
  }

  int timeout =
      tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

  /* GET /en/inbox 获取邮件列表 */
  char h_ck[MOHMAL_MAX_COOKIE];
  snprintf(h_ck, sizeof(h_ck), "Cookie: %s", ck);
  const char *hdr_inbox[] = {mohmal_ua,
                             "Accept: "
                             "text/html,application/xhtml+xml,application/"
                             "xml;q=0.9,image/avif,image/webp,*/*;q=0.8",
                             "Accept-Language: en-US,en;q=0.9",
                             "Cache-Control: no-cache",
                             "DNT: 1",
                             "Pragma: no-cache",
                             "Upgrade-Insecure-Requests: 1",
                             "Referer: " MOHMAL_ORIGIN "/en",
                             h_ck,
                             NULL};

  tm_http_response_t *r = tm_http_request(
      TM_HTTP_GET, MOHMAL_ORIGIN "/en/inbox", hdr_inbox, NULL, timeout);
  if (!r || r->status != 200 || !r->body) {
    TM_LOG_ERR("mohmal: 获取收件箱页面失败");
    tm_http_response_free(r);
    free(ck);
    return NULL;
  }

  /* 收集邮件 ID */
  char ids[MOHMAL_MAX_IDS][64];
  int nids = 0;
  mohmal_collect_ids(r->body, ids, &nids, MOHMAL_MAX_IDS);

  /* 同时解析各行的基本信息 */
  mohmal_row_info_t *row_infos = NULL;
  if (nids > 0) {
    row_infos =
        (mohmal_row_info_t *)calloc((size_t)nids, sizeof(mohmal_row_info_t));
    if (row_infos) {
      for (int i = 0; i < nids; i++) {
        mohmal_parse_row_info(r->body, ids[i], &row_infos[i]);
      }
    }
  }
  tm_http_response_free(r);

  if (nids == 0) {
    free(ck);
    free(row_infos);
    *count = 0;
    return NULL;
  }

  tm_email_t *arr = tm_emails_new(nids);
  if (!arr) {
    free(ck);
    free(row_infos);
    return NULL;
  }

  /* 逐个获取邮件详情 */
  for (int i = 0; i < nids; i++) {
    char url[384];
    snprintf(url, sizeof(url), MOHMAL_ORIGIN "/en/message/%s", ids[i]);
    char h_ref[400];
    snprintf(h_ref, sizeof(h_ref), "Referer: " MOHMAL_ORIGIN "/en/inbox");
    char h_ck_d[MOHMAL_MAX_COOKIE];
    snprintf(h_ck_d, sizeof(h_ck_d), "Cookie: %s", ck);

    const char *hdr_detail[] = {mohmal_ua,
                                "Accept: "
                                "text/html,application/xhtml+xml,application/"
                                "xml;q=0.9,image/avif,image/webp,*/*;q=0.8",
                                "Accept-Language: en-US,en;q=0.9",
                                "Cache-Control: no-cache",
                                "DNT: 1",
                                "Pragma: no-cache",
                                "Upgrade-Insecure-Requests: 1",
                                h_ref,
                                h_ck_d,
                                NULL};

    tm_http_response_t *rd =
        tm_http_request(TM_HTTP_GET, url, hdr_detail, NULL, timeout);

    /* 构建 cJSON 对象用于标准化 */
    cJSON *raw = cJSON_CreateObject();
    if (raw) {
      cJSON_AddStringToObject(raw, "id", ids[i]);
      cJSON_AddStringToObject(raw, "to", email);

      /* 使用行信息填充基本字段 */
      if (row_infos) {
        cJSON_AddStringToObject(raw, "from", row_infos[i].from);
        cJSON_AddStringToObject(raw, "subject", row_infos[i].subject);
        cJSON_AddStringToObject(raw, "date", row_infos[i].date);
      }

      /* 从详情页面提取邮件正文 */
      if (rd && rd->status == 200 && rd->body) {
        char *html_body = mohmal_extract_message_body(rd->body);
        if (html_body) {
          cJSON_AddStringToObject(raw, "html", html_body);
          /* 生成纯文本版本 */
          size_t text_cap = strlen(html_body) + 1;
          if (text_cap > 65536)
            text_cap = 65536;
          char *text_buf = (char *)malloc(text_cap);
          if (text_buf) {
            mohmal_strip_tags(html_body, text_buf, text_cap);
            cJSON_AddStringToObject(raw, "body_text", text_buf);
            free(text_buf);
          }
          free(html_body);
        }
      }

      arr[i] = tm_normalize_email(raw, email);
      cJSON_Delete(raw);
    } else {
      memset(&arr[i], 0, sizeof(arr[i]));
    }

    tm_http_response_free(rd);
  }

  free(ck);
  free(row_infos);
  *count = nids;
  return arr;
}
