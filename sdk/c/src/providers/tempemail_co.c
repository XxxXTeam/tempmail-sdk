/**
 * tempemail.co 渠道
 *
 * Cookie Session + REST JSON
 * 域名: @fmail10.de（由 API 返回，可能变化）
 *
 * 创建邮箱流程:
 *   GET https://tempemail.co/mail/random
 *   需要保存 Cookie（cookie jar）
 *   返回: {"result":true,"id":"user@fmail10.de","address":"user@fmail10.de"}
 *
 * 获取邮件流程:
 *   1. GET https://tempemail.co/get-mails?mail_id={address}&unseen=0&is_new=1
 *      需要带上 Cookie
 *      返回: {"result":true,"mails":"<html table>","count":0}
 *      mails 字段是 HTML 字符串，包含邮件表格
 *      当 count > 0 时，HTML 中每个邮件项有 class="read-mail" data-id="{id}"
 *      用 strstr 提取邮件 ID: data-id="(\d+)"
 *
 *   2. GET https://tempemail.co/mail/info?id={mailId}
 *      返回: {"result":true,"mail":{"fromName":"","fromAddress":"xxx",
 *             "displayDate":"xxx","subject":"xxx","textHtml":"<html>"}}
 *
 * token 存储策略:
 *   "tempemailco1:" + base64(json{"c":"cookie_header","addr":"address"})
 */
#include "tempmail_internal.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TECO_BASE "https://tempemail.co"
#define TECO_TOK_PREFIX "tempemailco1:"
#define TECO_MAX_COOKIE 8192
#define TECO_MAX_MAILS 128

/* ========== Cookie 工具 ========== */

typedef struct {
  char k[128];
  char v[2048];
} teco_ck_t;

static int teco_ck_find(teco_ck_t *tab, int n, const char *k) {
  for (int i = 0; i < n; i++) {
    if (strcmp(tab[i].k, k) == 0)
      return i;
  }
  return -1;
}

/**
 * 将 Cookie 串解析并入 tab（同名覆盖），返回新的 n
 */
static int teco_ck_parse_merge(teco_ck_t *tab, int n, int maxn,
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
    int ix = teco_ck_find(tab, n, key);
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

static char *teco_cookie_join(teco_ck_t *tab, int n) {
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
static char *teco_cookie_merge(const char *a, const char *b) {
  teco_ck_t tab[128];
  int n = 0;
  n = teco_ck_parse_merge(tab, n, 128, a);
  n = teco_ck_parse_merge(tab, n, 128, b);
  return teco_cookie_join(tab, n);
}

/* ========== Base64 编解码 ========== */

static int teco_b64_encode(const unsigned char *in, size_t inlen, char *out,
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

static int teco_b64_val(int c) {
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

static unsigned char *teco_b64_decode_alloc(const char *s, size_t *outlen) {
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
    int v0 = teco_b64_val((unsigned char)s[i]);
    int v1 = teco_b64_val((unsigned char)s[i + 1]);
    if (v0 < 0 || v1 < 0) {
      free(buf);
      return NULL;
    }
    unsigned triple = ((unsigned)v0 << 18) | ((unsigned)v1 << 12);
    if (s[i + 2] != '=') {
      int v2 = teco_b64_val((unsigned char)s[i + 2]);
      if (v2 < 0) {
        free(buf);
        return NULL;
      }
      triple |= (unsigned)v2 << 6;
      if (s[i + 3] != '=') {
        int v3 = teco_b64_val((unsigned char)s[i + 3]);
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

/**
 * token 格式: tempemailco1:<base64(json)>
 * json 内含 cookie 头和 address
 */
static char *teco_build_token(const char *cookie_hdr, const char *addr) {
  cJSON *o = cJSON_CreateObject();
  if (!o)
    return NULL;
  cJSON_AddStringToObject(o, "c", cookie_hdr ? cookie_hdr : "");
  cJSON_AddStringToObject(o, "addr", addr ? addr : "");
  char *json = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  if (!json)
    return NULL;
  size_t jl = strlen(json);
  size_t bcap = 4 * ((jl + 2) / 3) + 8 + strlen(TECO_TOK_PREFIX);
  char *tok = (char *)malloc(bcap);
  if (!tok) {
    free(json);
    return NULL;
  }
  strcpy(tok, TECO_TOK_PREFIX);
  if (teco_b64_encode((const unsigned char *)json, jl,
                      tok + strlen(TECO_TOK_PREFIX),
                      bcap - strlen(TECO_TOK_PREFIX)) != 0) {
    free(json);
    free(tok);
    return NULL;
  }
  free(json);
  return tok;
}

/**
 * 从 token 中解析出 cookie 头和 address
 */
static int teco_parse_token(const char *tok, char **cookie_hdr, char **addr) {
  *cookie_hdr = NULL;
  *addr = NULL;
  if (!tok || strncmp(tok, TECO_TOK_PREFIX, strlen(TECO_TOK_PREFIX)) != 0)
    return -1;
  const char *b64 = tok + strlen(TECO_TOK_PREFIX);
  size_t rawlen = 0;
  unsigned char *raw = teco_b64_decode_alloc(b64, &rawlen);
  if (!raw)
    return -1;
  raw[rawlen] = '\0';
  cJSON *j = cJSON_Parse((char *)raw);
  free(raw);
  if (!j)
    return -1;
  const cJSON *jc = cJSON_GetObjectItemCaseSensitive(j, "c");
  const cJSON *ja = cJSON_GetObjectItemCaseSensitive(j, "addr");
  if (!cJSON_IsString(jc) || !jc->valuestring || !jc->valuestring[0]) {
    cJSON_Delete(j);
    return -1;
  }
  *cookie_hdr = tm_strdup(jc->valuestring);
  *addr =
      tm_strdup(cJSON_IsString(ja) && ja->valuestring ? ja->valuestring : "");
  cJSON_Delete(j);
  if (!*cookie_hdr || !(*cookie_hdr)[0]) {
    free(*cookie_hdr);
    free(*addr);
    *cookie_hdr = NULL;
    *addr = NULL;
    return -1;
  }
  return 0;
}

/* ========== 公共请求头 ========== */

static const char *teco_ua =
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36";

/* ========== 从 HTML 中提取邮件 ID ========== */

/**
 * 从 get-mails 返回的 HTML 中提取所有 data-id="(\d+)" 值
 * 返回整数数组，调用者需 free
 *
 * @param html   HTML 字符串
 * @param out_n  输出 ID 数量
 */
static int *teco_extract_mail_ids(const char *html, int *out_n) {
  *out_n = 0;
  if (!html || !*html)
    return NULL;

  int cap = 32;
  int *ids = (int *)malloc(sizeof(int) * (size_t)cap);
  if (!ids)
    return NULL;

  const char *p = html;
  const char *needle = "data-id=\"";
  size_t needle_len = strlen(needle);

  while ((p = strstr(p, needle)) != NULL) {
    p += needle_len;
    /* 提取数字直到引号 */
    const char *start = p;
    while (*p >= '0' && *p <= '9')
      p++;
    if (p == start || *p != '"')
      continue;

    /* 解析数字 */
    size_t dlen = (size_t)(p - start);
    if (dlen > 15)
      continue; /* 防止溢出 */
    char numbuf[16];
    memcpy(numbuf, start, dlen);
    numbuf[dlen] = '\0';
    int id = atoi(numbuf);
    if (id <= 0)
      continue;

    /* 扩容 */
    if (*out_n >= cap) {
      cap *= 2;
      int *tmp = (int *)realloc(ids, sizeof(int) * (size_t)cap);
      if (!tmp)
        break;
      ids = tmp;
    }
    ids[(*out_n)++] = id;
  }

  if (*out_n == 0) {
    free(ids);
    return NULL;
  }
  return ids;
}

/* ========== 创建邮箱 ========== */

/**
 * 创建 tempemail.co 临时邮箱
 *
 * 流程:
 *   1. GET /mail/random -> 获取 session cookie + JSON 响应
 *   2. 返回 {"result":true,"id":"user@fmail10.de","address":"user@fmail10.de"}
 *   3. token 存储 cookie 和 address
 */
tm_email_info_t *tm_provider_tempemail_co_generate(void) {
  int timeout =
      tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

  const char *hdrs[] = {
      teco_ua, "Accept: application/json, text/html, */*; q=0.01",
      "Accept-Language: en-US,en;q=0.9", "Referer: " TECO_BASE "/", NULL};

  tm_http_response_t *resp = tm_http_request(
      TM_HTTP_GET, TECO_BASE "/mail/random", hdrs, NULL, timeout);
  if (!resp || resp->status != 200 || !resp->body) {
    TM_LOG_ERR("[tempemail-co] 创建邮箱请求失败, status=%ld",
               resp ? resp->status : 0);
    tm_http_response_free(resp);
    return NULL;
  }

  /* 保存 Cookie */
  char *ck = tm_strdup(resp->cookies ? resp->cookies : "");

  /* 解析 JSON 响应 */
  cJSON *json = cJSON_Parse(resp->body);
  tm_http_response_free(resp);

  if (!json || !cJSON_IsObject(json)) {
    TM_LOG_ERR("[tempemail-co] 解析创建邮箱响应 JSON 失败");
    cJSON_Delete(json);
    free(ck);
    return NULL;
  }

  /* 检查 result 字段 */
  const cJSON *j_result = cJSON_GetObjectItemCaseSensitive(json, "result");
  if (!cJSON_IsBool(j_result) || !cJSON_IsTrue(j_result)) {
    TM_LOG_ERR("[tempemail-co] 创建邮箱响应 result 不为 true");
    cJSON_Delete(json);
    free(ck);
    return NULL;
  }

  /* 提取 address 字段 */
  const cJSON *j_addr = cJSON_GetObjectItemCaseSensitive(json, "address");
  if (!cJSON_IsString(j_addr) || !j_addr->valuestring ||
      !j_addr->valuestring[0]) {
    TM_LOG_ERR("[tempemail-co] 响应中未找到 address 字段");
    cJSON_Delete(json);
    free(ck);
    return NULL;
  }

  /* 验证邮箱格式 */
  if (!strchr(j_addr->valuestring, '@')) {
    TM_LOG_ERR("[tempemail-co] 返回的邮箱格式无效: %s", j_addr->valuestring);
    cJSON_Delete(json);
    free(ck);
    return NULL;
  }

  /* 构建 token（存储 cookie 和 address） */
  char *tok = teco_build_token(ck, j_addr->valuestring);
  if (!tok) {
    TM_LOG_ERR("[tempemail-co] 构建 token 失败");
    cJSON_Delete(json);
    free(ck);
    return NULL;
  }

  /* 构建结果 */
  tm_email_info_t *info = tm_email_info_new();
  if (!info) {
    free(tok);
    cJSON_Delete(json);
    free(ck);
    return NULL;
  }

  info->channel = CHANNEL_TEMPEMAIL_CO;
  info->email = tm_strdup(j_addr->valuestring);
  info->token = tok;
  info->expires_at = 0;
  info->created_at = tm_strdup("");

  cJSON_Delete(json);
  free(ck);

  TM_LOG_DBG("[tempemail-co] 创建邮箱成功: %s", info->email);
  return info;
}

/* ========== 获取邮件 ========== */

/**
 * 获取 tempemail.co 邮件列表
 *
 * 流程:
 *   1. GET /get-mails?mail_id={address}&unseen=0&is_new=1
 *      返回 {"result":true,"mails":"<html>","count":N}
 *      从 mails HTML 中用 strstr 提取 data-id="(\d+)"
 *   2. 对每个邮件 ID，GET /mail/info?id={id}
 *      返回 {"result":true,"mail":{...}}
 *
 * @param token  tempemailco1:... 编码的 cookie + address
 * @param email  完整邮箱地址
 * @param count  输出邮件数量，-1 表示请求失败
 */
tm_email_t *tm_provider_tempemail_co_get_emails(const char *token,
                                                const char *email, int *count) {
  *count = -1;
  if (!token || !email)
    return NULL;

  char *ck = NULL;
  char *addr = NULL;
  if (teco_parse_token(token, &ck, &addr) != 0) {
    TM_LOG_ERR("[tempemail-co] token 解析失败");
    return NULL;
  }

  int timeout =
      tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

  /* 第一步: GET /get-mails 获取邮件列表（HTML 包装在 JSON 中） */
  char list_url[512];
  snprintf(list_url, sizeof(list_url),
           TECO_BASE "/get-mails?mail_id=%s&unseen=0&is_new=1", addr);

  char h_ck[TECO_MAX_COOKIE];
  snprintf(h_ck, sizeof(h_ck), "Cookie: %s", ck);

  const char *hdr_list[] = {
      teco_ua,
      "Accept: application/json, text/javascript, */*; q=0.01",
      "Accept-Language: en-US,en;q=0.9",
      "X-Requested-With: XMLHttpRequest",
      "Referer: " TECO_BASE "/",
      h_ck,
      NULL};

  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, list_url, hdr_list, NULL, timeout);
  if (!resp || resp->status != 200 || !resp->body) {
    TM_LOG_ERR("[tempemail-co] 获取邮件列表请求失败, status=%ld",
               resp ? resp->status : 0);
    tm_http_response_free(resp);
    free(ck);
    free(addr);
    return NULL;
  }

  /* 合并可能的新 Cookie */
  if (resp->cookies && resp->cookies[0]) {
    char *merged = teco_cookie_merge(ck, resp->cookies);
    free(ck);
    ck = merged;
  }

  /* 解析 JSON 响应: {"result":true,"mails":"<html>","count":N} */
  cJSON *json = cJSON_Parse(resp->body);
  tm_http_response_free(resp);

  if (!json || !cJSON_IsObject(json)) {
    TM_LOG_ERR("[tempemail-co] 解析邮件列表 JSON 失败");
    cJSON_Delete(json);
    free(ck);
    free(addr);
    return NULL;
  }

  /* 检查 count 字段 */
  const cJSON *j_count = cJSON_GetObjectItemCaseSensitive(json, "count");
  int mail_count = 0;
  if (cJSON_IsNumber(j_count)) {
    mail_count = (int)j_count->valuedouble;
  }

  if (mail_count <= 0) {
    /* 没有邮件 */
    cJSON_Delete(json);
    free(ck);
    free(addr);
    *count = 0;
    return NULL;
  }

  /* 从 mails HTML 中提取邮件 ID */
  const cJSON *j_mails = cJSON_GetObjectItemCaseSensitive(json, "mails");
  if (!cJSON_IsString(j_mails) || !j_mails->valuestring) {
    TM_LOG_ERR("[tempemail-co] 响应中 mails 字段不是字符串");
    cJSON_Delete(json);
    free(ck);
    free(addr);
    return NULL;
  }

  int id_count = 0;
  int *mail_ids = teco_extract_mail_ids(j_mails->valuestring, &id_count);
  cJSON_Delete(json);

  if (!mail_ids || id_count <= 0) {
    TM_LOG_ERR("[tempemail-co] 未能从 HTML 中提取到邮件 ID");
    free(mail_ids);
    free(ck);
    free(addr);
    *count = 0;
    return NULL;
  }

  if (id_count > TECO_MAX_MAILS)
    id_count = TECO_MAX_MAILS;

  /* 第二步: 逐个获取邮件详情 */
  tm_email_t *emails = tm_emails_new(id_count);
  if (!emails) {
    free(mail_ids);
    free(ck);
    free(addr);
    return NULL;
  }

  int valid = 0;
  for (int i = 0; i < id_count; i++) {
    char detail_url[256];
    snprintf(detail_url, sizeof(detail_url), TECO_BASE "/mail/info?id=%d",
             mail_ids[i]);

    char h_ck_d[TECO_MAX_COOKIE];
    snprintf(h_ck_d, sizeof(h_ck_d), "Cookie: %s", ck);

    const char *hdr_detail[] = {
        teco_ua,
        "Accept: application/json, text/javascript, */*; q=0.01",
        "Accept-Language: en-US,en;q=0.9",
        "X-Requested-With: XMLHttpRequest",
        "Referer: " TECO_BASE "/",
        h_ck_d,
        NULL};

    tm_http_response_t *rd =
        tm_http_request(TM_HTTP_GET, detail_url, hdr_detail, NULL, timeout);
    if (!rd || rd->status != 200 || !rd->body) {
      TM_LOG_WARN("[tempemail-co] 获取邮件详情失败, id=%d, status=%ld",
                  mail_ids[i], rd ? rd->status : 0);
      tm_http_response_free(rd);
      continue;
    }

    /* 解析详情 JSON: {"result":true,"mail":{...}} */
    cJSON *detail_json = cJSON_Parse(rd->body);
    tm_http_response_free(rd);

    if (!detail_json || !cJSON_IsObject(detail_json)) {
      cJSON_Delete(detail_json);
      continue;
    }

    const cJSON *j_mail = cJSON_GetObjectItemCaseSensitive(detail_json, "mail");
    if (!cJSON_IsObject(j_mail)) {
      cJSON_Delete(detail_json);
      continue;
    }

    /* 构建标准化用的 cJSON 对象 */
    cJSON *raw = cJSON_CreateObject();
    if (!raw) {
      cJSON_Delete(detail_json);
      continue;
    }

    /* 设置 ID */
    char id_str[32];
    snprintf(id_str, sizeof(id_str), "%d", mail_ids[i]);
    cJSON_AddStringToObject(raw, "id", id_str);

    /* 映射 fromAddress -> from */
    const cJSON *j_from_addr =
        cJSON_GetObjectItemCaseSensitive(j_mail, "fromAddress");
    if (cJSON_IsString(j_from_addr) && j_from_addr->valuestring) {
      cJSON_AddStringToObject(raw, "from", j_from_addr->valuestring);
    }

    /* 映射 fromName -> from_name */
    const cJSON *j_from_name =
        cJSON_GetObjectItemCaseSensitive(j_mail, "fromName");
    if (cJSON_IsString(j_from_name) && j_from_name->valuestring &&
        j_from_name->valuestring[0]) {
      cJSON_AddStringToObject(raw, "from_name", j_from_name->valuestring);
    }

    /* 收件人 */
    cJSON_AddStringToObject(raw, "to", email);

    /* 主题 */
    const cJSON *j_subject =
        cJSON_GetObjectItemCaseSensitive(j_mail, "subject");
    if (cJSON_IsString(j_subject) && j_subject->valuestring) {
      cJSON_AddStringToObject(raw, "subject", j_subject->valuestring);
    }

    /* 日期 */
    const cJSON *j_date =
        cJSON_GetObjectItemCaseSensitive(j_mail, "displayDate");
    if (cJSON_IsString(j_date) && j_date->valuestring) {
      cJSON_AddStringToObject(raw, "date", j_date->valuestring);
    }

    /* HTML 内容 */
    const cJSON *j_html = cJSON_GetObjectItemCaseSensitive(j_mail, "textHtml");
    if (cJSON_IsString(j_html) && j_html->valuestring) {
      cJSON_AddStringToObject(raw, "html", j_html->valuestring);
    }

    emails[valid] = tm_normalize_email(raw, email);
    cJSON_Delete(raw);
    cJSON_Delete(detail_json);
    valid++;
  }

  free(mail_ids);
  free(ck);
  free(addr);

  if (valid == 0) {
    free(emails);
    *count = 0;
    return NULL;
  }

  *count = valid;
  return emails;
}
