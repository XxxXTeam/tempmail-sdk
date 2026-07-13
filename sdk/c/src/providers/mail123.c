/**
 * Mail123 -- https://mail123.fr
 */

#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAIL123_BASE "https://mail123.fr/api/v1"

static const char *mail123_headers[] = {"Accept: application/json",
                                        "User-Agent: Mozilla/5.0", NULL};

static char *mail123_urlencode(const char *s) {
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

static cJSON *mail123_json(const char *url) {
  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, mail123_headers, NULL, 15);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }
  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  return root;
}

tm_email_info_t *tm_provider_mail123_generate(void) {
  cJSON *root = mail123_json(MAIL123_BASE "/mailbox/new");
  if (!root)
    return NULL;
  const char *address =
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "address"), "");
  if (!address[0] || !strchr(address, '@')) {
    cJSON_Delete(root);
    return NULL;
  }
  tm_email_info_t *info = tm_email_info_new();
  if (!info) {
    cJSON_Delete(root);
    return NULL;
  }
  info->channel = CHANNEL_MAIL123;
  info->email = tm_strdup(address);
  info->token = tm_strdup(address);
  cJSON *days = cJSON_GetObjectItemCaseSensitive(root, "expires_in_days");
  if (cJSON_IsNumber(days) && days->valueint > 0) {
    info->expires_at =
        ((long long)time(NULL) + (long long)days->valueint * 86400LL) * 1000LL;
  }
  cJSON_Delete(root);
  return info;
}

static cJSON *mail123_fetch_detail(const char *address,
                                   const char *message_id) {
  char *addr_enc = mail123_urlencode(address);
  char *id_enc = mail123_urlencode(message_id);
  if (!addr_enc || !id_enc) {
    free(addr_enc);
    free(id_enc);
    return NULL;
  }
  size_t cap = strlen(MAIL123_BASE) + strlen(addr_enc) + strlen(id_enc) + 32;
  char *url = (char *)malloc(cap);
  if (!url) {
    free(addr_enc);
    free(id_enc);
    return NULL;
  }
  snprintf(url, cap, "%s/mailbox/%s/messages/%s", MAIL123_BASE, addr_enc,
           id_enc);
  free(addr_enc);
  free(id_enc);
  cJSON *root = mail123_json(url);
  free(url);
  if (!root)
    return NULL;
  cJSON *message = cJSON_GetObjectItemCaseSensitive(root, "message");
  cJSON *dup = message ? cJSON_Duplicate(message, 1) : NULL;
  cJSON_Delete(root);
  return dup;
}

static cJSON *mail123_message_raw(const cJSON *msg, const char *recipient) {
  cJSON *raw = cJSON_CreateObject();
  if (!raw)
    return NULL;
  const cJSON *id = cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "id");
  const cJSON *from = cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "from");
  const cJSON *subject =
      cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "subject");
  const cJSON *text = cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "text");
  if (!text)
    text = cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "preview");
  const cJSON *html = cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "html");
  const cJSON *date = cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "date");
  cJSON_AddStringToObject(raw, "id", TM_JSON_STR(id, ""));
  cJSON_AddStringToObject(raw, "from", TM_JSON_STR(from, ""));
  cJSON_AddStringToObject(
      raw, "to",
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "to"),
                  recipient));
  cJSON_AddStringToObject(raw, "subject", TM_JSON_STR(subject, ""));
  cJSON_AddStringToObject(raw, "text", TM_JSON_STR(text, ""));
  cJSON_AddStringToObject(raw, "html", TM_JSON_STR(html, ""));
  cJSON_AddStringToObject(raw, "date", TM_JSON_STR(date, ""));
  const cJSON *unread =
      cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "is_unread");
  if (cJSON_IsBool(unread))
    cJSON_AddBoolToObject(raw, "isRead", !cJSON_IsTrue(unread));
  const cJSON *attachments =
      cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "attachments");
  if (attachments)
    cJSON_AddItemToObject(raw, "attachments",
                          cJSON_Duplicate((cJSON *)attachments, 1));
  return raw;
}

tm_email_t *tm_provider_mail123_get_emails(const char *email, int *count) {
  *count = -1;
  if (!email || !email[0])
    return NULL;
  char *addr_enc = mail123_urlencode(email);
  if (!addr_enc)
    return NULL;
  size_t cap = strlen(MAIL123_BASE) + strlen(addr_enc) + 36;
  char *url = (char *)malloc(cap);
  if (!url) {
    free(addr_enc);
    return NULL;
  }
  snprintf(url, cap, "%s/mailbox/%s/messages?limit=50", MAIL123_BASE, addr_enc);
  free(addr_enc);
  cJSON *root = mail123_json(url);
  free(url);
  if (!root)
    return NULL;
  cJSON *messages = cJSON_GetObjectItemCaseSensitive(root, "messages");
  if (!cJSON_IsArray(messages)) {
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
    const char *id =
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(item, "id"), "");
    cJSON *detail = id[0] ? mail123_fetch_detail(email, id) : NULL;
    cJSON *raw = mail123_message_raw(detail ? detail : item, email);
    emails[i] = tm_normalize_email(raw ? raw : (detail ? detail : item), email);
    if (raw)
      cJSON_Delete(raw);
    if (detail)
      cJSON_Delete(detail);
  }
  cJSON_Delete(root);
  return emails;
}
