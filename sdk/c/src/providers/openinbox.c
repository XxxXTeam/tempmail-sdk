/**
 * OpenInbox -- https://openinbox.io
 */

#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OPENINBOX_BASE "https://api.openinbox.io/api"

static const char *openinbox_headers[] = {
    "Accept: application/json, text/plain, */*", "Origin: https://openinbox.io",
    "Referer: https://openinbox.io/", "User-Agent: Mozilla/5.0", NULL};

static const char *openinbox_post_headers[] = {
    "Accept: application/json, text/plain, */*",
    "Content-Type: application/json",
    "Origin: https://openinbox.io",
    "Referer: https://openinbox.io/",
    "User-Agent: Mozilla/5.0",
    NULL};

static char *openinbox_urlencode(const char *s) {
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

static cJSON *openinbox_json(tm_http_method_t method, const char *url) {
  const char **headers =
      method == TM_HTTP_POST ? openinbox_post_headers : openinbox_headers;
  tm_http_response_t *resp =
      tm_http_request_ipv4(method, url, headers, NULL, 15);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }
  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  return root;
}

tm_email_info_t *tm_provider_openinbox_generate(void) {
  cJSON *root = openinbox_json(TM_HTTP_POST, OPENINBOX_BASE "/inbox");
  if (!root)
    return NULL;

  const char *id =
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "id"), "");
  const char *email =
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "email"), "");
  if (!id[0] || !email[0] || !strchr(email, '@')) {
    cJSON_Delete(root);
    return NULL;
  }

  tm_email_info_t *info = tm_email_info_new();
  if (!info) {
    cJSON_Delete(root);
    return NULL;
  }
  info->channel = CHANNEL_OPENINBOX;
  info->email = tm_strdup(email);
  info->token = tm_strdup(id);
  info->created_at = tm_strdup(
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "createdAt"), ""));
  cJSON_Delete(root);
  return info;
}

static cJSON *openinbox_fetch_detail(const char *message_id) {
  char *id_enc = openinbox_urlencode(message_id);
  if (!id_enc)
    return NULL;
  size_t cap = strlen(OPENINBOX_BASE) + strlen(id_enc) + 10;
  char *url = (char *)malloc(cap);
  if (!url) {
    free(id_enc);
    return NULL;
  }
  snprintf(url, cap, "%s/emails/%s", OPENINBOX_BASE, id_enc);
  free(id_enc);
  cJSON *root = openinbox_json(TM_HTTP_GET, url);
  free(url);
  return root;
}

static cJSON *openinbox_message_raw(const cJSON *msg, const char *recipient) {
  cJSON *raw = cJSON_CreateObject();
  if (!raw)
    return NULL;

  cJSON_AddStringToObject(
      raw, "id",
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "id"), ""));
  cJSON_AddStringToObject(
      raw, "from",
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "from"), ""));
  cJSON_AddStringToObject(raw, "to", recipient);
  cJSON_AddStringToObject(
      raw, "subject",
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "subject"),
                  ""));
  cJSON_AddStringToObject(
      raw, "text",
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "textBody"),
                  ""));
  cJSON_AddStringToObject(
      raw, "html",
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "htmlBody"),
                  ""));
  cJSON_AddStringToObject(
      raw, "date",
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "receivedAt"),
                  ""));

  const cJSON *read = cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "isRead");
  if (cJSON_IsBool(read)) {
    cJSON_AddBoolToObject(raw, "isRead", cJSON_IsTrue(read));
  } else if (cJSON_IsNumber(read)) {
    cJSON_AddBoolToObject(raw, "isRead", read->valueint != 0);
  } else {
    cJSON_AddBoolToObject(raw, "isRead", 0);
  }
  cJSON_AddItemToObject(raw, "attachments", cJSON_CreateArray());
  return raw;
}

tm_email_t *tm_provider_openinbox_get_emails(const char *token,
                                             const char *email, int *count) {
  *count = -1;
  if (!token || !token[0] || !email || !email[0])
    return NULL;

  char *box_enc = openinbox_urlencode(token);
  if (!box_enc)
    return NULL;
  size_t cap = strlen(OPENINBOX_BASE) + strlen(box_enc) + 34;
  char *url = (char *)malloc(cap);
  if (!url) {
    free(box_enc);
    return NULL;
  }
  snprintf(url, cap, "%s/emails/inbox/%s?page=1&limit=50", OPENINBOX_BASE,
           box_enc);
  free(box_enc);

  cJSON *root = openinbox_json(TM_HTTP_GET, url);
  free(url);
  if (!root)
    return NULL;

  cJSON *messages = cJSON_GetObjectItemCaseSensitive(root, "emails");
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
    cJSON *row = cJSON_GetArrayItem(messages, i);
    const char *id =
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(row, "id"), "");
    cJSON *detail = id[0] ? openinbox_fetch_detail(id) : NULL;
    cJSON *raw = openinbox_message_raw(detail ? detail : row, email);
    emails[i] = tm_normalize_email(raw ? raw : (detail ? detail : row), email);
    if (raw)
      cJSON_Delete(raw);
    if (detail)
      cJSON_Delete(detail);
  }

  cJSON_Delete(root);
  return emails;
}
