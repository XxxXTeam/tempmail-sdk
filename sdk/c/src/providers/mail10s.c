/**
 * Mail10s -- https://mail10s.com
 */

#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAIL10S_BASE "https://mail10s.com"
#define MAIL10S_DOMAIN "mail10s.com"

static const char *mail10s_headers[] = {"Accept: application/json",
                                        "User-Agent: Mozilla/5.0", NULL};

static char *mail10s_urlencode(const char *s) {
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

static char *mail10s_random_local(void) {
  const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
  char *out = (char *)malloc(20);
  if (!out)
    return NULL;
  memcpy(out, "sdk", 3);
  for (int i = 3; i < 19; i++) {
    out[i] = chars[rand() % (int)(sizeof(chars) - 1)];
  }
  out[19] = '\0';
  return out;
}

static cJSON *mail10s_message_raw(const cJSON *msg, const char *recipient) {
  cJSON *raw = cJSON_CreateObject();
  if (!raw)
    return NULL;
  cJSON_AddStringToObject(
      raw, "id",
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "id"), ""));
  cJSON_AddStringToObject(
      raw, "from",
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "sender"),
                  ""));
  cJSON_AddStringToObject(raw, "to", recipient ? recipient : "");
  cJSON_AddStringToObject(
      raw, "subject",
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "subject"),
                  ""));
  cJSON_AddStringToObject(
      raw, "text",
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "body_text"),
                  ""));
  cJSON_AddStringToObject(
      raw, "html",
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "body_html"),
                  ""));
  cJSON_AddStringToObject(
      raw, "date",
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "received_at"),
                  ""));
  const cJSON *attachments =
      cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "attachments");
  if (attachments)
    cJSON_AddItemToObject(raw, "attachments",
                          cJSON_Duplicate((cJSON *)attachments, 1));
  return raw;
}

tm_email_info_t *tm_provider_mail10s_generate(void) {
  char *local = mail10s_random_local();
  if (!local)
    return NULL;
  size_t need = strlen(local) + 1 + strlen(MAIL10S_DOMAIN) + 1;
  char *email = (char *)malloc(need);
  if (!email) {
    free(local);
    return NULL;
  }
  snprintf(email, need, "%s@%s", local, MAIL10S_DOMAIN);
  free(local);

  tm_email_info_t *info = tm_email_info_new();
  if (!info) {
    free(email);
    return NULL;
  }
  info->channel = CHANNEL_MAIL10S;
  info->email = email;
  info->token = tm_strdup(email);
  return info;
}

tm_email_t *tm_provider_mail10s_get_emails(const char *email, int *count) {
  *count = -1;
  if (!email || !email[0])
    return NULL;

  char *addr_enc = mail10s_urlencode(email);
  if (!addr_enc)
    return NULL;
  size_t cap = strlen(MAIL10S_BASE) + strlen(addr_enc) + 28;
  char *url = (char *)malloc(cap);
  if (!url) {
    free(addr_enc);
    return NULL;
  }
  snprintf(url, cap, "%s/api/emails/%s/inbox", MAIL10S_BASE, addr_enc);
  free(addr_enc);

  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, mail10s_headers, NULL, 15);
  free(url);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }
  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!root)
    return NULL;

  cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
  cJSON *messages = cJSON_GetObjectItemCaseSensitive(data, "messages");
  if (!cJSON_IsArray(messages)) {
    *count = 0;
    cJSON_Delete(root);
    return NULL;
  }

  int n = cJSON_GetArraySize(messages);
  *count = n;
  if (n == 0) {
    cJSON_Delete(root);
    return NULL;
  }
  tm_email_t *emails = tm_emails_new(n);
  if (!emails) {
    *count = -1;
    cJSON_Delete(root);
    return NULL;
  }
  for (int i = 0; i < n; i++) {
    cJSON *item = cJSON_GetArrayItem(messages, i);
    cJSON *raw = mail10s_message_raw(item, email);
    emails[i] = tm_normalize_email(raw ? raw : item, email);
    if (raw)
      cJSON_Delete(raw);
  }
  cJSON_Delete(root);
  return emails;
}
