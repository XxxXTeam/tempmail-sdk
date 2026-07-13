/**
 * TempGBox 渠道实现
 * API: https://tempgbox.net/api/proxy
 */
#include "tempmail_internal.h"
#include <time.h>

#define TGBOX_API "https://tempgbox.net/api/proxy"

static int tgbox_seeded = 0;

static void tgbox_seed_once(void) {
  if (!tgbox_seeded) {
    srand((unsigned int)time(NULL) ^ (unsigned int)clock());
    tgbox_seeded = 1;
  }
}

static void tgbox_random_device_id(char out[65]) {
  /* 上游按 X-Device-ID 限制连续生成；每个新邮箱使用新的设备
   * ID，后续收件复用该邮箱 token。 */
  static const char hex[] = "0123456789abcdef";
  tgbox_seed_once();
  for (int i = 0; i < 32; i++) {
    unsigned int v = (unsigned int)(rand() & 0xff);
    out[i * 2] = hex[(v >> 4) & 0xf];
    out[i * 2 + 1] = hex[v & 0xf];
  }
  out[64] = '\0';
}

static void tgbox_random_ip(char out[32]) {
  tgbox_seed_once();
  snprintf(out, 32, "%d.%d.%d.%d", (rand() % 254) + 1, (rand() % 254) + 1,
           (rand() % 254) + 1, (rand() % 254) + 1);
}

static int tgbox_b64_val(unsigned char c) {
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
  if (c == '=')
    return -2;
  return -1;
}

static unsigned char *tgbox_b64_decode(const char *s, size_t *outlen) {
  size_t len = s ? strlen(s) : 0;
  if (len == 0 || len % 4 != 0)
    return NULL;
  unsigned char *out = (unsigned char *)malloc((len / 4) * 3 + 1);
  if (!out)
    return NULL;
  size_t o = 0;
  for (size_t i = 0; i < len; i += 4) {
    int v0 = tgbox_b64_val((unsigned char)s[i]);
    int v1 = tgbox_b64_val((unsigned char)s[i + 1]);
    int v2 = tgbox_b64_val((unsigned char)s[i + 2]);
    int v3 = tgbox_b64_val((unsigned char)s[i + 3]);
    if (v0 < 0 || v1 < 0 || v2 == -1 || v3 == -1) {
      free(out);
      return NULL;
    }
    out[o++] = (unsigned char)((v0 << 2) | (v1 >> 4));
    if (v2 >= 0) {
      out[o++] = (unsigned char)(((v1 & 15) << 4) | (v2 >> 2));
    }
    if (v3 >= 0 && v2 >= 0) {
      out[o++] = (unsigned char)(((v2 & 3) << 6) | v3);
    }
  }
  out[o] = '\0';
  if (outlen)
    *outlen = o;
  return out;
}

static char *tgbox_extract_data_x(const char *html) {
  if (!html)
    return NULL;
  const char *p = strstr(html, "data-x=\"");
  char quote = '"';
  if (!p) {
    p = strstr(html, "data-x='");
    quote = '\'';
  }
  if (!p)
    return NULL;
  p += 8;
  const char *end = strchr(p, quote);
  if (!end || end <= p)
    return NULL;
  size_t len = (size_t)(end - p);
  char *out = (char *)malloc(len + 1);
  if (!out)
    return NULL;
  memcpy(out, p, len);
  out[len] = '\0';
  return out;
}

static cJSON *tgbox_decode_payload(const char *html) {
  char *b64 = tgbox_extract_data_x(html);
  if (!b64)
    return NULL;
  size_t raw_len = 0;
  unsigned char *raw = tgbox_b64_decode(b64, &raw_len);
  free(b64);
  if (!raw)
    return NULL;
  cJSON *json = cJSON_Parse((const char *)raw);
  free(raw);
  return json;
}

static tm_http_response_t *tgbox_post(const char *route, const char *device_id,
                                      const char *body) {
  char url[256];
  char ip[32];
  char device_hdr[96];
  char xff[64];
  char xreal[64];
  char xorig[64];
  tgbox_random_ip(ip);
  snprintf(url, sizeof(url), TGBOX_API "?route=%s", route);
  snprintf(device_hdr, sizeof(device_hdr), "X-Device-ID: %s", device_id);
  snprintf(xff, sizeof(xff), "X-Forwarded-For: %s", ip);
  snprintf(xreal, sizeof(xreal), "X-Real-IP: %s", ip);
  snprintf(xorig, sizeof(xorig), "X-Originating-IP: %s", ip);
  const char *headers[] = {
      "Accept: text/html,application/json",
      "Content-Type: application/json",
      "Origin: https://tempgbox.net",
      "Referer: https://tempgbox.net/",
      "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
      "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
      device_hdr,
      xff,
      xreal,
      xorig,
      NULL};
  return tm_http_request(TM_HTTP_POST, url, headers, body, 15);
}

tm_email_info_t *tm_provider_tempgbox_generate(void) {
  char device_id[65];
  tgbox_random_device_id(device_id);
  tm_http_response_t *resp =
      tgbox_post("generate", device_id, "{\"variant\":\"googlemail\"}");
  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }
  cJSON *json = tgbox_decode_payload(resp->body);
  tm_http_response_free(resp);
  if (!json)
    return NULL;

  cJSON *alias = cJSON_GetObjectItemCaseSensitive(json, "alias");
  if (!cJSON_IsObject(alias))
    alias = json;
  const char *email =
      cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(alias, "email"));
  if (!email)
    email =
        cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(alias, "alias"));
  if (!email) {
    cJSON_Delete(json);
    return NULL;
  }

  tm_email_info_t *info = tm_email_info_new();
  info->channel = CHANNEL_TEMPGBOX;
  info->email = tm_strdup(email);
  info->token = tm_strdup(device_id);
  const char *created = cJSON_GetStringValue(
      cJSON_GetObjectItemCaseSensitive(alias, "created_at"));
  if (created)
    info->created_at = tm_strdup(created);
  cJSON_Delete(json);
  return info;
}

tm_email_t *tm_provider_tempgbox_get_emails(const char *device_id,
                                            const char *email, int *count) {
  *count = -1;
  if (!device_id || !device_id[0] || !email || !email[0])
    return NULL;
  char body[512];
  snprintf(body, sizeof(body), "{\"email\":\"%s\"}", email);
  tm_http_response_t *resp = tgbox_post("inbox", device_id, body);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }
  cJSON *json = tgbox_decode_payload(resp->body);
  tm_http_response_free(resp);
  if (!json)
    return NULL;

  cJSON *messages = cJSON_GetObjectItemCaseSensitive(json, "messages");
  int n = cJSON_IsArray(messages) ? cJSON_GetArraySize(messages) : 0;
  *count = n;
  if (n == 0) {
    cJSON_Delete(json);
    return NULL;
  }

  tm_email_t *emails = tm_emails_new(n);
  for (int i = 0; i < n; i++) {
    cJSON *raw = cJSON_GetArrayItem(messages, i);
    cJSON *flat = cJSON_CreateObject();
    const char *id =
        cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(raw, "id"));
    if (!id)
      id = cJSON_GetStringValue(
          cJSON_GetObjectItemCaseSensitive(raw, "message_id"));
    cJSON_AddStringToObject(flat, "id", id ? id : "");
    const char *from =
        cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(raw, "from"));
    if (!from)
      from =
          cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(raw, "sender"));
    cJSON_AddStringToObject(flat, "from", from ? from : "");
    cJSON_AddStringToObject(flat, "to", email);
    cJSON_AddStringToObject(
        flat, "subject",
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(raw, "subject"), ""));
    const char *text =
        cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(raw, "text"));
    if (!text)
      text = cJSON_GetStringValue(
          cJSON_GetObjectItemCaseSensitive(raw, "body_text"));
    cJSON_AddStringToObject(flat, "text", text ? text : "");
    const char *html =
        cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(raw, "html"));
    if (!html)
      html = cJSON_GetStringValue(
          cJSON_GetObjectItemCaseSensitive(raw, "body_html"));
    cJSON_AddStringToObject(flat, "html", html ? html : "");
    const char *date =
        cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(raw, "date"));
    if (!date)
      date = cJSON_GetStringValue(
          cJSON_GetObjectItemCaseSensitive(raw, "received_at"));
    cJSON_AddStringToObject(flat, "date", date ? date : "");
    cJSON *atts = cJSON_GetObjectItemCaseSensitive(raw, "attachments");
    if (!atts)
      atts = cJSON_GetObjectItemCaseSensitive(raw, "attachments_info");
    if (atts)
      cJSON_AddItemToObject(flat, "attachments", cJSON_Duplicate(atts, 1));
    emails[i] = tm_normalize_email(flat, email);
    cJSON_Delete(flat);
  }
  cJSON_Delete(json);
  return emails;
}
