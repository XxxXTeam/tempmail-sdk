/**
 * apihz（接口盒子）临时邮箱渠道 — https://apihz.cn
 *
 * 需 id/key 凭据，内置官方公共账号 88888888/88888888（共享频次）作为默认，
 * 可用环境变量 APIHZ_ID / APIHZ_KEY 覆盖。
 *
 * 创建邮箱:
 *   GET /api/mail/mailcache.php?id=&key=&domain=&name=&pwd=&buytype=0
 *   domain 从 ["apimail.email","apimail.vip"] 随机选；name 随机 10 位
 * [a-z0-9]； pwd 随机 12 位 [A-Za-z0-9]，必须显式传（读信须携带创建时的 pwd）。
 *   响应: {"code":200,"mail":"...","pwd":"...","endtime":"..."}
 * 获取邮件:
 *   GET /api/mail/mailgetlist.php?id=&key=&mail=&pwd=&page=1
 *   响应:
 * {"code":200,"num":"N","data":[{frommail,fromname,subject,time1,time2,content}]}
 *
 * 邮箱有效期 10 分钟。token 编码策略: "apihz1:" +
 * base64(json{"mail":"...","pwd":"..."})。
 */
#include "tempmail_internal.h"
#include <stdlib.h>
#include <string.h>

#define APIHZ_BASE "https://cn.apihz.cn"
#define APIHZ_TOK_PREFIX "apihz1:"
#define APIHZ_PUBLIC_ID "88888888"
#define APIHZ_PUBLIC_KEY "88888888"
#define APIHZ_MAX_MAILS 128

static const char *apihz_ua =
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36";

/* apihz 自研收信域名（MX 指向 mail.apimail.* 自建服务器） */
static const char *apihz_domains[] = {"apimail.email", "apimail.vip"};

/* ========== 凭据读取 ========== */

/* 优先环境变量 APIHZ_ID/APIHZ_KEY，回退官方公共账号 */
static const char *apihz_id(void) {
  const char *v = getenv("APIHZ_ID");
  return (v && *v) ? v : APIHZ_PUBLIC_ID;
}
static const char *apihz_key(void) {
  const char *v = getenv("APIHZ_KEY");
  return (v && *v) ? v : APIHZ_PUBLIC_KEY;
}

/* ========== Base64 编解码 ========== */

static int apihz_b64_encode(const unsigned char *in, size_t inlen, char *out,
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

static int apihz_b64_val(int c) {
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

static unsigned char *apihz_b64_decode_alloc(const char *s, size_t *outlen) {
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
    int v0 = apihz_b64_val((unsigned char)s[i]);
    int v1 = apihz_b64_val((unsigned char)s[i + 1]);
    if (v0 < 0 || v1 < 0) {
      free(buf);
      return NULL;
    }
    unsigned triple = ((unsigned)v0 << 18) | ((unsigned)v1 << 12);
    if (s[i + 2] != '=') {
      int v2 = apihz_b64_val((unsigned char)s[i + 2]);
      if (v2 < 0) {
        free(buf);
        return NULL;
      }
      triple |= (unsigned)v2 << 6;
      if (s[i + 3] != '=') {
        int v3 = apihz_b64_val((unsigned char)s[i + 3]);
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

/* ========== URL 编码 ========== */

/* URL 编码字符串（RFC3986 未保留字符原样保留，其余百分号编码；堆分配，调用者
 * free） */
static char *apihz_url_encode(const char *s) {
  static const char hex[] = "0123456789ABCDEF";
  if (!s)
    return NULL;
  size_t len = strlen(s);
  char *out = (char *)malloc(len * 3 + 1);
  if (!out)
    return NULL;
  size_t o = 0;
  for (size_t i = 0; i < len; i++) {
    unsigned char uc = (unsigned char)s[i];
    if ((uc >= 'A' && uc <= 'Z') || (uc >= 'a' && uc <= 'z') ||
        (uc >= '0' && uc <= '9') || uc == '-' || uc == '_' || uc == '.' ||
        uc == '~') {
      out[o++] = (char)uc;
    } else {
      out[o++] = '%';
      out[o++] = hex[(uc >> 4) & 0x0F];
      out[o++] = hex[uc & 0x0F];
    }
  }
  out[o] = '\0';
  return out;
}

/* ========== Token 构建与解析 ========== */

/* token = "apihz1:" + base64(json{"mail","pwd"}) */
static char *apihz_build_token(const char *mail, const char *pwd) {
  cJSON *o = cJSON_CreateObject();
  if (!o)
    return NULL;
  cJSON_AddStringToObject(o, "mail", mail ? mail : "");
  cJSON_AddStringToObject(o, "pwd", pwd ? pwd : "");
  char *json = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  if (!json)
    return NULL;
  size_t jl = strlen(json);
  size_t prefix_len = strlen(APIHZ_TOK_PREFIX);
  size_t bcap = 4 * ((jl + 2) / 3) + 8 + prefix_len;
  char *tok = (char *)malloc(bcap);
  if (!tok) {
    free(json);
    return NULL;
  }
  strcpy(tok, APIHZ_TOK_PREFIX);
  if (apihz_b64_encode((const unsigned char *)json, jl, tok + prefix_len,
                       bcap - prefix_len) != 0) {
    free(json);
    free(tok);
    return NULL;
  }
  free(json);
  return tok;
}

/* 解析 token 得到 mail/pwd（调用者 free） */
static int apihz_parse_token(const char *tok, char **mail, char **pwd) {
  *mail = NULL;
  *pwd = NULL;
  size_t prefix_len = strlen(APIHZ_TOK_PREFIX);
  if (!tok || strncmp(tok, APIHZ_TOK_PREFIX, prefix_len) != 0)
    return -1;
  size_t rawlen = 0;
  unsigned char *raw = apihz_b64_decode_alloc(tok + prefix_len, &rawlen);
  if (!raw)
    return -1;
  raw[rawlen] = '\0';
  cJSON *j = cJSON_Parse((char *)raw);
  free(raw);
  if (!j)
    return -1;
  const cJSON *jmail = cJSON_GetObjectItemCaseSensitive(j, "mail");
  const cJSON *jpwd = cJSON_GetObjectItemCaseSensitive(j, "pwd");
  if (!cJSON_IsString(jmail) || !jmail->valuestring || !jmail->valuestring[0] ||
      !cJSON_IsString(jpwd) || !jpwd->valuestring || !jpwd->valuestring[0]) {
    cJSON_Delete(j);
    return -1;
  }
  *mail = tm_strdup(jmail->valuestring);
  *pwd = tm_strdup(jpwd->valuestring);
  cJSON_Delete(j);
  if (!*mail || !*pwd) {
    free(*mail);
    free(*pwd);
    *mail = NULL;
    *pwd = NULL;
    return -1;
  }
  return 0;
}

/* ========== 随机串 ========== */

/* 随机 n 位 [a-z0-9] 邮箱前缀 */
static void apihz_random_local(char *out, size_t n) {
  static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
  for (size_t i = 0; i < n; i++) {
    out[i] = chars[rand() % (int)(sizeof(chars) - 1)];
  }
  out[n] = '\0';
}

/* 随机 12 位 [A-Za-z0-9] 密码（读信须携带创建时的密码） */
static void apihz_random_pwd(char *out, size_t n) {
  static const char chars[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  for (size_t i = 0; i < n; i++) {
    out[i] = chars[rand() % (int)(sizeof(chars) - 1)];
  }
  out[n] = '\0';
}

/**
 * 创建 apihz（接口盒子）临时邮箱
 */
tm_email_info_t *tm_provider_apihz_generate(void) {
  int timeout =
      tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

  const char *domain = apihz_domains[rand() % (int)(sizeof(apihz_domains) /
                                                    sizeof(apihz_domains[0]))];
  char name[16];
  char pwd[16];
  apihz_random_local(name, 10);
  apihz_random_pwd(pwd, 12);

  char *e_id = apihz_url_encode(apihz_id());
  char *e_key = apihz_url_encode(apihz_key());
  char *e_domain = apihz_url_encode(domain);
  char *e_name = apihz_url_encode(name);
  char *e_pwd = apihz_url_encode(pwd);
  if (!e_id || !e_key || !e_domain || !e_name || !e_pwd) {
    free(e_id);
    free(e_key);
    free(e_domain);
    free(e_name);
    free(e_pwd);
    return NULL;
  }

  char url[512];
  snprintf(
      url, sizeof(url),
      APIHZ_BASE
      "/api/mail/mailcache.php?id=%s&key=%s&domain=%s&name=%s&pwd=%s&buytype=0",
      e_id, e_key, e_domain, e_name, e_pwd);
  free(e_id);
  free(e_key);
  free(e_domain);
  free(e_name);
  free(e_pwd);

  const char *hdrs[] = {apihz_ua, "Accept: application/json", NULL};

  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, hdrs, NULL, timeout);
  if (!resp || resp->status != 200 || !resp->body) {
    TM_LOG_ERR("[apihz] 创建邮箱失败, status=%ld", resp ? resp->status : 0);
    tm_http_response_free(resp);
    return NULL;
  }

  cJSON *json = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!json) {
    TM_LOG_ERR("[apihz] 创建响应非 JSON");
    return NULL;
  }

  const cJSON *j_code = cJSON_GetObjectItemCaseSensitive(json, "code");
  const cJSON *j_mail = cJSON_GetObjectItemCaseSensitive(json, "mail");
  if (!cJSON_IsNumber(j_code) || j_code->valueint != 200 ||
      !cJSON_IsString(j_mail) || !j_mail->valuestring ||
      !j_mail->valuestring[0]) {
    TM_LOG_WARN("[apihz] 创建邮箱失败，无效响应");
    cJSON_Delete(json);
    return NULL;
  }

  /* 优先使用响应回传的 pwd（与请求一致），确保读信密码正确 */
  const cJSON *j_pwd = cJSON_GetObjectItemCaseSensitive(json, "pwd");
  const char *final_pwd =
      (cJSON_IsString(j_pwd) && j_pwd->valuestring && j_pwd->valuestring[0])
          ? j_pwd->valuestring
          : pwd;

  char *mail_addr = tm_strdup(j_mail->valuestring);
  char *token = apihz_build_token(mail_addr, final_pwd);
  cJSON_Delete(json);
  if (!mail_addr || !token) {
    free(mail_addr);
    free(token);
    return NULL;
  }

  tm_email_info_t *info = tm_email_info_new();
  if (!info) {
    free(mail_addr);
    free(token);
    return NULL;
  }
  info->channel = CHANNEL_APIHZ;
  info->email = mail_addr;
  info->token = token;
  info->expires_at = 0;
  info->created_at = tm_strdup("");

  TM_LOG_DBG("[apihz] 创建邮箱成功: %s", info->email);
  return info;
}

/**
 * 获取 apihz 邮件列表
 */
tm_email_t *tm_provider_apihz_get_emails(const char *token, const char *email,
                                         int *count) {
  *count = -1;

  char *mail = NULL;
  char *pwd = NULL;
  if (apihz_parse_token(token, &mail, &pwd) != 0) {
    TM_LOG_WARN("[apihz] 无效 token");
    return NULL;
  }
  const char *addr = (mail && mail[0]) ? mail : email;
  if (!addr || !addr[0]) {
    free(mail);
    free(pwd);
    TM_LOG_WARN("[apihz] 缺少邮箱地址");
    return NULL;
  }

  int timeout =
      tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

  char *e_id = apihz_url_encode(apihz_id());
  char *e_key = apihz_url_encode(apihz_key());
  char *e_mail = apihz_url_encode(addr);
  char *e_pwd = apihz_url_encode(pwd);
  if (!e_id || !e_key || !e_mail || !e_pwd) {
    free(mail);
    free(pwd);
    free(e_id);
    free(e_key);
    free(e_mail);
    free(e_pwd);
    return NULL;
  }

  char url[512];
  snprintf(url, sizeof(url),
           APIHZ_BASE
           "/api/mail/mailgetlist.php?id=%s&key=%s&mail=%s&pwd=%s&page=1",
           e_id, e_key, e_mail, e_pwd);
  free(e_id);
  free(e_key);
  free(e_mail);
  free(e_pwd);

  const char *hdrs[] = {apihz_ua, "Accept: application/json", NULL};

  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, hdrs, NULL, timeout);
  free(mail);
  free(pwd);
  if (!resp || resp->status != 200 || !resp->body) {
    TM_LOG_ERR("[apihz] 获取邮件失败, status=%ld", resp ? resp->status : 0);
    tm_http_response_free(resp);
    return NULL;
  }

  cJSON *json = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!json) {
    TM_LOG_ERR("[apihz] 邮件响应解析失败");
    return NULL;
  }

  const cJSON *j_code = cJSON_GetObjectItemCaseSensitive(json, "code");
  const cJSON *j_data = cJSON_GetObjectItemCaseSensitive(json, "data");
  if (!cJSON_IsNumber(j_code) || j_code->valueint != 200 ||
      !cJSON_IsArray(j_data)) {
    cJSON_Delete(json);
    *count = 0;
    return NULL;
  }

  int n = cJSON_GetArraySize(j_data);
  if (n == 0) {
    cJSON_Delete(json);
    *count = 0;
    return NULL;
  }
  if (n > APIHZ_MAX_MAILS)
    n = APIHZ_MAX_MAILS;

  tm_email_t *emails = tm_emails_new(n);
  if (!emails) {
    cJSON_Delete(json);
    return NULL;
  }

  int valid = 0;
  for (int i = 0; i < n; i++) {
    const cJSON *m = cJSON_GetArrayItem(j_data, i);
    if (!cJSON_IsObject(m))
      continue;

    const cJSON *j_frommail = cJSON_GetObjectItemCaseSensitive(m, "frommail");
    const cJSON *j_fromname = cJSON_GetObjectItemCaseSensitive(m, "fromname");
    const cJSON *j_subject = cJSON_GetObjectItemCaseSensitive(m, "subject");
    const cJSON *j_content = cJSON_GetObjectItemCaseSensitive(m, "content");
    const cJSON *j_time1 = cJSON_GetObjectItemCaseSensitive(m, "time1");
    const cJSON *j_time2 = cJSON_GetObjectItemCaseSensitive(m, "time2");

    /* id = String(time1)，date = time2 || time1 */
    char id_buf[32];
    const char *id_str = "";
    if (cJSON_IsString(j_time1) && j_time1->valuestring) {
      id_str = j_time1->valuestring;
    } else if (cJSON_IsNumber(j_time1)) {
      snprintf(id_buf, sizeof(id_buf), "%lld", (long long)j_time1->valuedouble);
      id_str = id_buf;
    }
    const char *from_str = TM_JSON_STR(j_frommail, TM_JSON_STR(j_fromname, ""));
    const char *content_str = TM_JSON_STR(j_content, "");
    const char *date_str = TM_JSON_STR(j_time2, id_str);

    cJSON *flat = cJSON_CreateObject();
    if (!flat)
      continue;
    cJSON_AddStringToObject(flat, "id", id_str);
    cJSON_AddStringToObject(flat, "from", from_str);
    cJSON_AddStringToObject(flat, "to", addr);
    cJSON_AddStringToObject(flat, "subject", TM_JSON_STR(j_subject, ""));
    cJSON_AddStringToObject(flat, "text", content_str);
    cJSON_AddStringToObject(flat, "html", content_str);
    cJSON_AddStringToObject(flat, "received_at", date_str);
    emails[valid] = tm_normalize_email(flat, addr);
    cJSON_Delete(flat);
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
