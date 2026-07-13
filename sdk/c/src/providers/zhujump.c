/**
 * mail.zhujump.com 固定域渠道
 * 通过注册账号、登录会话后创建固定域邮箱，再通过邮箱 ID 获取邮件列表。
 */

#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ZHJ_BASE "https://mail.zhujump.com"
#define ZHJ_LOGIN_REFERER "https://mail.zhujump.com/zh-CN/login"
#define ZHJ_MAX_COOKIE 8192
#define ZHJ_EXPIRY_NONE (-1)

typedef struct {
  char k[128];
  char v[4096];
} zhj_ck_t;

static int zhj_ck_find(zhj_ck_t *tab, int n, const char *k) {
  for (int i = 0; i < n; i++) {
    if (strcmp(tab[i].k, k) == 0)
      return i;
  }
  return -1;
}

static int zhj_ck_parse_merge(zhj_ck_t *tab, int n, int maxn, const char *hdr) {
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
    int ix = zhj_ck_find(tab, n, key);
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

static char *zhj_cookie_join(zhj_ck_t *tab, int n) {
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

static char *zhj_cookie_merge(const char *a, const char *b) {
  zhj_ck_t tab[128];
  int n = 0;
  n = zhj_ck_parse_merge(tab, n, 128, a);
  n = zhj_ck_parse_merge(tab, n, 128, b);
  return zhj_cookie_join(tab, n);
}

static char *zhj_random_string(const char *prefix, int extra_len) {
  const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
  size_t prefix_len = prefix ? strlen(prefix) : 0;
  size_t total = prefix_len + (size_t)extra_len;
  char *out = (char *)malloc(total + 1);
  if (!out)
    return NULL;
  if (prefix_len > 0)
    memcpy(out, prefix, prefix_len);
  for (int i = 0; i < extra_len; i++) {
    out[prefix_len + (size_t)i] = chars[rand() % (sizeof(chars) - 1)];
  }
  out[total] = '\0';
  return out;
}

static char *zhj_random_password(void) {
  char *mid = zhj_random_string("", 12);
  if (!mid)
    return NULL;
  size_t need = strlen(mid) + 8;
  char *out = (char *)malloc(need);
  if (!out) {
    free(mid);
    return NULL;
  }
  snprintf(out, need, "Sdk!%sA1", mid);
  free(mid);
  return out;
}

static char *zhj_urlencode(const char *s) {
  if (!s)
    return tm_strdup("");
  size_t len = strlen(s);
  char *out = (char *)malloc(len * 3 + 1);
  if (!out)
    return NULL;
  char *p = out;
  static const char hex[] = "0123456789ABCDEF";
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)s[i];
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
        c == '~') {
      *p++ = (char)c;
    } else {
      *p++ = '%';
      *p++ = hex[c >> 4];
      *p++ = hex[c & 15];
    }
  }
  *p = '\0';
  return out;
}

static tm_http_response_t *zhj_request_json(const char *method, const char *url,
                                            const char **headers,
                                            const char *body) {
  return tm_http_request(strcmp(method, "POST") == 0 ? TM_HTTP_POST
                                                     : TM_HTTP_GET,
                         url, headers, body, 15);
}

static char *zhj_join_url(const char *base, const char *path) {
  size_t blen = strlen(base ? base : "");
  while (blen > 0 && base[blen - 1] == '/')
    blen--;
  size_t plen = strlen(path ? path : "");
  char *out = (char *)malloc(blen + plen + 1);
  if (!out)
    return NULL;
  memcpy(out, base, blen);
  memcpy(out + blen, path, plen);
  out[blen + plen] = '\0';
  return out;
}

static char *zhj_header_line(const char *name, const char *value) {
  size_t need = strlen(name ? name : "") + strlen(value ? value : "") + 3;
  char *line = (char *)malloc(need);
  if (!line)
    return NULL;
  snprintf(line, need, "%s: %s", name ? name : "", value ? value : "");
  return line;
}

static char *zhj_extract_json_string(const cJSON *obj, const char *key) {
  return tm_strdup(TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(obj, key), ""));
}

static char *zhj_build_cookie_header_line(const char *cookie) {
  size_t need = strlen(cookie ? cookie : "") + 9;
  char *line = (char *)malloc(need);
  if (!line)
    return NULL;
  snprintf(line, need, "Cookie: %s", cookie ? cookie : "");
  return line;
}

static char *zhj_encode_token(const char *cookie, const char *email_id,
                              const char *base_url) {
  cJSON *root = cJSON_CreateObject();
  if (!root)
    return NULL;
  cJSON_AddStringToObject(root, "cookie", cookie ? cookie : "");
  cJSON_AddStringToObject(root, "email_id", email_id ? email_id : "");
  cJSON_AddStringToObject(root, "base_url", base_url ? base_url : ZHJ_BASE);
  char *out = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  return out;
}

static int zhj_decode_token(const char *token, char **cookie, char **email_id,
                            char **base_url) {
  *cookie = NULL;
  *email_id = NULL;
  *base_url = NULL;
  cJSON *root = cJSON_Parse(token ? token : "");
  if (!root)
    return 0;
  const char *cookie_s =
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "cookie"), "");
  const char *email_id_s =
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "email_id"), "");
  const char *base_s =
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "base_url"), ZHJ_BASE);
  if (!cookie_s[0] || !email_id_s[0]) {
    cJSON_Delete(root);
    return 0;
  }
  *cookie = tm_strdup(cookie_s);
  *email_id = tm_strdup(email_id_s);
  *base_url = tm_strdup(base_s[0] ? base_s : ZHJ_BASE);
  cJSON_Delete(root);
  return *cookie && *email_id && *base_url;
}

static tm_email_info_t *zhj_generate_ex(const char *base_url,
                                        const char *domain,
                                        tm_channel_t channel, int expiry_time) {
  char *username = zhj_random_string("sdk", 10);
  char *password = zhj_random_password();
  char *local = zhj_random_string("sdk", 10);
  if (!username || !password || !local || !base_url || !base_url[0] ||
      !domain || !domain[0]) {
    free(username);
    free(password);
    free(local);
    return NULL;
  }

  char *login_referer = zhj_join_url(base_url, "/zh-CN/login");
  char *origin_line = zhj_header_line("Origin", base_url);
  char *referer_line =
      zhj_header_line("Referer", login_referer ? login_referer : "");
  if (!login_referer || !origin_line || !referer_line) {
    free(username);
    free(password);
    free(local);
    free(login_referer);
    free(origin_line);
    free(referer_line);
    return NULL;
  }

  char *cookie = tm_strdup("");
  if (!cookie) {
    free(username);
    free(password);
    free(local);
    free(login_referer);
    free(origin_line);
    free(referer_line);
    return NULL;
  }

  cJSON *reg = cJSON_CreateObject();
  cJSON_AddStringToObject(reg, "username", username);
  cJSON_AddStringToObject(reg, "password", password);
  cJSON_AddStringToObject(reg, "turnstileToken", "");
  char *reg_body = cJSON_PrintUnformatted(reg);
  cJSON_Delete(reg);
  if (!reg_body) {
    free(username);
    free(password);
    free(local);
    free(cookie);
    free(login_referer);
    free(origin_line);
    free(referer_line);
    return NULL;
  }

  const char *reg_headers[] = {
      "Accept: application/json",
      origin_line,
      referer_line,
      "Content-Type: application/json",
      "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
      "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
      NULL};
  char *url_register = zhj_join_url(base_url, "/api/auth/register");
  tm_http_response_t *r1 =
      url_register
          ? zhj_request_json("POST", url_register, reg_headers, reg_body)
          : NULL;
  free(url_register);
  free(reg_body);
  if (!r1 || r1->status < 200 || r1->status >= 300) {
    tm_http_response_free(r1);
    free(username);
    free(password);
    free(local);
    free(cookie);
    free(login_referer);
    free(origin_line);
    free(referer_line);
    return NULL;
  }
  char *cookie2 = zhj_cookie_merge(cookie, r1->cookies ? r1->cookies : "");
  free(cookie);
  cookie = cookie2;
  tm_http_response_free(r1);

  char *cookie_line = zhj_build_cookie_header_line(cookie);
  const char *csrf_headers[] = {
      "Accept: application/json",
      origin_line,
      referer_line,
      "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
      "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
      cookie_line,
      NULL};
  char *url_csrf = zhj_join_url(base_url, "/api/auth/csrf");
  tm_http_response_t *r2 =
      url_csrf ? zhj_request_json("GET", url_csrf, csrf_headers, NULL) : NULL;
  free(url_csrf);
  free(cookie_line);
  if (!r2 || r2->status < 200 || r2->status >= 300) {
    tm_http_response_free(r2);
    free(username);
    free(password);
    free(local);
    free(cookie);
    free(login_referer);
    free(origin_line);
    free(referer_line);
    return NULL;
  }
  cookie2 = zhj_cookie_merge(cookie, r2->cookies ? r2->cookies : "");
  free(cookie);
  cookie = cookie2;
  cJSON *csrf_json = cJSON_Parse(r2->body);
  tm_http_response_free(r2);
  if (!csrf_json) {
    free(username);
    free(password);
    free(local);
    free(cookie);
    free(login_referer);
    free(origin_line);
    free(referer_line);
    return NULL;
  }
  char *csrf = zhj_extract_json_string(csrf_json, "csrfToken");
  cJSON_Delete(csrf_json);
  if (!csrf || !csrf[0]) {
    free(username);
    free(password);
    free(local);
    free(cookie);
    free(csrf);
    free(login_referer);
    free(origin_line);
    free(referer_line);
    return NULL;
  }

  char *enc_user = zhj_urlencode(username);
  char *enc_pass = zhj_urlencode(password);
  char *enc_csrf = zhj_urlencode(csrf);
  char *enc_cb = zhj_urlencode(login_referer);
  if (!enc_user || !enc_pass || !enc_csrf || !enc_cb) {
    free(username);
    free(password);
    free(local);
    free(cookie);
    free(csrf);
    free(login_referer);
    free(origin_line);
    free(referer_line);
    free(enc_user);
    free(enc_pass);
    free(enc_csrf);
    free(enc_cb);
    return NULL;
  }
  size_t login_body_cap = strlen(enc_user) + strlen(enc_pass) +
                          strlen(enc_csrf) + strlen(enc_cb) + 128;
  char *login_body = (char *)malloc(login_body_cap);
  if (!login_body) {
    free(username);
    free(password);
    free(local);
    free(cookie);
    free(csrf);
    free(login_referer);
    free(origin_line);
    free(referer_line);
    free(enc_user);
    free(enc_pass);
    free(enc_csrf);
    free(enc_cb);
    return NULL;
  }
  snprintf(login_body, login_body_cap,
           "username=%s&password=%s&turnstileToken=&redirect=false&csrfToken=%"
           "s&callbackUrl=%s",
           enc_user, enc_pass, enc_csrf, enc_cb);
  free(enc_user);
  free(enc_pass);
  free(enc_csrf);
  free(enc_cb);

  cookie_line = zhj_build_cookie_header_line(cookie);
  const char *login_headers[] = {
      "Accept: application/json",
      origin_line,
      referer_line,
      "Content-Type: application/x-www-form-urlencoded",
      "x-auth-return-redirect: 1",
      "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
      "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
      cookie_line,
      NULL};
  char *url_login = zhj_join_url(base_url, "/api/auth/callback/credentials?");
  tm_http_response_t *r3 =
      url_login ? zhj_request_json("POST", url_login, login_headers, login_body)
                : NULL;
  free(url_login);
  free(cookie_line);
  free(login_body);
  if (!r3 || r3->status < 200 || r3->status >= 300) {
    tm_http_response_free(r3);
    free(username);
    free(password);
    free(local);
    free(cookie);
    free(csrf);
    free(login_referer);
    free(origin_line);
    free(referer_line);
    return NULL;
  }
  cookie2 = zhj_cookie_merge(cookie, r3->cookies ? r3->cookies : "");
  free(cookie);
  cookie = cookie2;
  tm_http_response_free(r3);

  cookie_line = zhj_build_cookie_header_line(cookie);
  const char *session_headers[] = {
      "Accept: application/json",
      origin_line,
      referer_line,
      "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
      "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
      cookie_line,
      NULL};
  char *url_session = zhj_join_url(base_url, "/api/auth/session");
  tm_http_response_t *r4 =
      url_session ? zhj_request_json("GET", url_session, session_headers, NULL)
                  : NULL;
  free(url_session);
  free(cookie_line);
  if (!r4 || r4->status < 200 || r4->status >= 300) {
    tm_http_response_free(r4);
    free(username);
    free(password);
    free(local);
    free(cookie);
    free(csrf);
    free(login_referer);
    free(origin_line);
    free(referer_line);
    return NULL;
  }
  cJSON *session_json = cJSON_Parse(r4->body);
  tm_http_response_free(r4);
  if (!session_json) {
    free(username);
    free(password);
    free(local);
    free(cookie);
    free(csrf);
    free(login_referer);
    free(origin_line);
    free(referer_line);
    return NULL;
  }
  cJSON *user_obj = cJSON_GetObjectItemCaseSensitive(session_json, "user");
  const char *username_back =
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(user_obj, "username"), "");
  if (strcmp(username_back, username) != 0) {
    cJSON_Delete(session_json);
    free(username);
    free(password);
    free(local);
    free(cookie);
    free(csrf);
    free(login_referer);
    free(origin_line);
    free(referer_line);
    return NULL;
  }
  cJSON_Delete(session_json);

  cJSON *gen = cJSON_CreateObject();
  cJSON_AddStringToObject(gen, "name", local);
  cJSON_AddStringToObject(gen, "domain", domain);
  if (expiry_time >= 0) {
    cJSON_AddNumberToObject(gen, "expiryTime", expiry_time);
  }
  char *gen_body = cJSON_PrintUnformatted(gen);
  cJSON_Delete(gen);
  if (!gen_body) {
    free(username);
    free(password);
    free(local);
    free(cookie);
    free(csrf);
    free(login_referer);
    free(origin_line);
    free(referer_line);
    return NULL;
  }

  cookie_line = zhj_build_cookie_header_line(cookie);
  const char *gen_headers[] = {
      "Accept: application/json",
      origin_line,
      referer_line,
      "Content-Type: application/json",
      "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
      "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
      cookie_line,
      NULL};
  char *url_generate = zhj_join_url(base_url, "/api/emails/generate");
  tm_http_response_t *r5 =
      url_generate
          ? zhj_request_json("POST", url_generate, gen_headers, gen_body)
          : NULL;
  free(url_generate);
  free(cookie_line);
  free(gen_body);
  if (!r5 || r5->status < 200 || r5->status >= 300) {
    tm_http_response_free(r5);
    free(username);
    free(password);
    free(local);
    free(cookie);
    free(csrf);
    free(login_referer);
    free(origin_line);
    free(referer_line);
    return NULL;
  }
  cJSON *gen_json = cJSON_Parse(r5->body);
  tm_http_response_free(r5);
  if (!gen_json) {
    free(username);
    free(password);
    free(local);
    free(cookie);
    free(csrf);
    free(login_referer);
    free(origin_line);
    free(referer_line);
    return NULL;
  }
  char *email = zhj_extract_json_string(gen_json, "email");
  char *email_id = zhj_extract_json_string(gen_json, "id");
  cJSON_Delete(gen_json);
  if (!email || !email[0] || !email_id || !email_id[0]) {
    free(username);
    free(password);
    free(local);
    free(cookie);
    free(csrf);
    free(email);
    free(email_id);
    free(login_referer);
    free(origin_line);
    free(referer_line);
    return NULL;
  }

  char *token = zhj_encode_token(cookie, email_id, base_url);
  if (!token) {
    free(username);
    free(password);
    free(local);
    free(cookie);
    free(csrf);
    free(email);
    free(email_id);
    free(login_referer);
    free(origin_line);
    free(referer_line);
    return NULL;
  }

  tm_email_info_t *info = tm_email_info_new();
  if (!info) {
    free(username);
    free(password);
    free(local);
    free(cookie);
    free(csrf);
    free(email);
    free(email_id);
    free(token);
    free(login_referer);
    free(origin_line);
    free(referer_line);
    return NULL;
  }
  info->channel = channel;
  info->email = email;
  info->token = token;

  free(username);
  free(password);
  free(local);
  free(cookie);
  free(csrf);
  free(email_id);
  free(login_referer);
  free(origin_line);
  free(referer_line);
  return info;
}

tm_email_info_t *tm_provider_zhujump_generate(const char *domain,
                                              tm_channel_t channel) {
  return zhj_generate_ex(ZHJ_BASE, domain, channel, ZHJ_EXPIRY_NONE);
}

tm_email_info_t *tm_provider_moemail_generate(const char *base_url,
                                              const char *domain,
                                              tm_channel_t channel,
                                              int expiry_time) {
  return zhj_generate_ex(base_url, domain, channel, expiry_time);
}

static cJSON *zhj_fetch_message_detail(const char *base_url, const char *cookie,
                                       const char *email_id,
                                       const char *message_id) {
  if (!base_url || !cookie || !email_id || !message_id || !message_id[0])
    return NULL;

  char path[1024];
  snprintf(path, sizeof(path), "/api/emails/%s/%s", email_id, message_id);
  char *url = zhj_join_url(base_url, path);
  char *login_referer = zhj_join_url(base_url, "/zh-CN/login");
  char *origin_line = zhj_header_line("Origin", base_url);
  char *referer_line =
      zhj_header_line("Referer", login_referer ? login_referer : "");
  char *cookie_line = zhj_build_cookie_header_line(cookie);
  if (!url || !login_referer || !origin_line || !referer_line || !cookie_line) {
    free(url);
    free(login_referer);
    free(origin_line);
    free(referer_line);
    free(cookie_line);
    return NULL;
  }
  const char *headers[] = {
      "Accept: application/json",
      origin_line,
      referer_line,
      "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
      "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
      cookie_line,
      NULL};
  tm_http_response_t *resp = zhj_request_json("GET", url, headers, NULL);
  free(url);
  free(login_referer);
  free(origin_line);
  free(referer_line);
  free(cookie_line);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }
  cJSON *json = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!json)
    return NULL;
  cJSON *message = cJSON_DetachItemFromObjectCaseSensitive(json, "message");
  cJSON_Delete(json);
  return cJSON_IsObject(message) ? message : NULL;
}

tm_email_t *tm_provider_zhujump_get_emails(const char *token, const char *email,
                                           int *count) {
  *count = -1;
  if (!token || !email || !email[0])
    return NULL;

  char *cookie = NULL;
  char *email_id = NULL;
  char *base_url = NULL;
  if (!zhj_decode_token(token, &cookie, &email_id, &base_url)) {
    free(cookie);
    free(email_id);
    free(base_url);
    return NULL;
  }

  char path[1024];
  snprintf(path, sizeof(path), "/api/emails/%s", email_id);
  char *url = zhj_join_url(base_url, path);
  char *login_referer = zhj_join_url(base_url, "/zh-CN/login");
  char *origin_line = zhj_header_line("Origin", base_url);
  char *referer_line =
      zhj_header_line("Referer", login_referer ? login_referer : "");
  char *cookie_line = zhj_build_cookie_header_line(cookie);
  if (!url || !login_referer || !origin_line || !referer_line || !cookie_line) {
    free(cookie);
    free(email_id);
    free(base_url);
    free(url);
    free(login_referer);
    free(origin_line);
    free(referer_line);
    free(cookie_line);
    return NULL;
  }
  const char *headers[] = {
      "Accept: application/json",
      origin_line,
      referer_line,
      "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
      "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
      cookie_line,
      NULL};
  tm_http_response_t *resp = zhj_request_json("GET", url, headers, NULL);
  free(url);
  free(login_referer);
  free(origin_line);
  free(referer_line);
  free(cookie_line);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    free(cookie);
    free(email_id);
    free(base_url);
    return NULL;
  }
  cJSON *json = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!json) {
    free(cookie);
    free(email_id);
    free(base_url);
    return NULL;
  }

  cJSON *messages = cJSON_GetObjectItemCaseSensitive(json, "messages");
  int n = cJSON_IsArray(messages) ? cJSON_GetArraySize(messages) : 0;
  *count = n;
  if (n == 0) {
    cJSON_Delete(json);
    free(cookie);
    free(email_id);
    free(base_url);
    return NULL;
  }

  tm_email_t *emails = tm_emails_new(n);
  if (!emails) {
    cJSON_Delete(json);
    free(cookie);
    free(email_id);
    free(base_url);
    *count = -1;
    return NULL;
  }

  for (int i = 0; i < n; i++) {
    cJSON *item = cJSON_GetArrayItem(messages, i);
    cJSON *detail = NULL;
    const char *content =
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(item, "content"), "");
    const char *html =
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(item, "html"), "");
    const char *message_id =
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(item, "id"), "");
    cJSON *source = item;
    if ((!content[0] && !html[0]) && message_id[0]) {
      detail = zhj_fetch_message_detail(base_url, cookie, email_id, message_id);
      if (detail)
        source = detail;
    }
    cJSON *norm = cJSON_CreateObject();
    cJSON_AddStringToObject(
        norm, "id",
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(source, "id"), ""));
    cJSON_AddStringToObject(
        norm, "from_address",
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(source, "from_address"),
                    ""));
    cJSON_AddStringToObject(
        norm, "to_address",
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(source, "to_address"),
                    email));
    cJSON_AddStringToObject(
        norm, "subject",
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(source, "subject"), ""));
    cJSON_AddStringToObject(
        norm, "content",
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(source, "content"), ""));
    cJSON_AddStringToObject(
        norm, "html",
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(source, "html"), ""));
    cJSON *received = cJSON_GetObjectItemCaseSensitive(source, "received_at");
    cJSON *sent = cJSON_GetObjectItemCaseSensitive(source, "sent_at");
    if (cJSON_IsNumber(received)) {
      cJSON_AddNumberToObject(norm, "received_at", received->valuedouble);
    } else if (cJSON_IsString(received) && received->valuestring) {
      cJSON_AddStringToObject(norm, "received_at", received->valuestring);
    } else if (cJSON_IsNumber(sent)) {
      cJSON_AddNumberToObject(norm, "received_at", sent->valuedouble);
    } else if (cJSON_IsString(sent) && sent->valuestring) {
      cJSON_AddStringToObject(norm, "received_at", sent->valuestring);
    }
    emails[i] = tm_normalize_email(norm, email);
    cJSON_Delete(norm);
    cJSON_Delete(detail);
  }

  cJSON_Delete(json);
  free(cookie);
  free(email_id);
  free(base_url);
  return emails;
}
