/**
 * ThrowawayMail -- https://throwawaymail.app
 */

#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define THROWAWAYMAIL_BASE "https://throwawaymail.app/api"

static const char *throwawaymail_headers[] = {"Accept: application/json", NULL};

static char *throwawaymail_urlencode(const char *s) {
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

static cJSON *throwawaymail_json(tm_http_method_t method, const char *url) {
  tm_http_response_t *resp =
      tm_http_request(method, url, throwawaymail_headers, NULL, 15);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }
  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  return root;
}

tm_email_info_t *tm_provider_throwawaymail_generate(void) {
  cJSON *root =
      throwawaymail_json(TM_HTTP_POST, THROWAWAYMAIL_BASE "/mailboxes");
  if (!root)
    return NULL;

  const char *mailbox_id =
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "mailbox_id"), "");
  const char *address =
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "address"), "");
  const char *created_at =
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "created_at"), "");
  if (!mailbox_id[0] || !address[0] || !strchr(address, '@')) {
    cJSON_Delete(root);
    return NULL;
  }

  tm_email_info_t *info = tm_email_info_new();
  if (!info) {
    cJSON_Delete(root);
    return NULL;
  }
  info->channel = CHANNEL_THROWAWAYMAIL;
  info->email = tm_strdup(address);
  info->token = tm_strdup(mailbox_id);
  info->created_at = tm_strdup(created_at);
  cJSON_Delete(root);
  return info;
}

static cJSON *throwawaymail_fetch_message(const char *mailbox_id,
                                          const char *message_id) {
  char *mid_enc = throwawaymail_urlencode(mailbox_id);
  char *msg_enc = throwawaymail_urlencode(message_id);
  if (!mid_enc || !msg_enc) {
    free(mid_enc);
    free(msg_enc);
    return NULL;
  }
  size_t cap =
      strlen(THROWAWAYMAIL_BASE) + strlen(mid_enc) + strlen(msg_enc) + 32;
  char *url = (char *)malloc(cap);
  if (!url) {
    free(mid_enc);
    free(msg_enc);
    return NULL;
  }
  snprintf(url, cap, "%s/mailboxes/%s/messages/%s", THROWAWAYMAIL_BASE, mid_enc,
           msg_enc);
  free(mid_enc);
  free(msg_enc);

  cJSON *root = throwawaymail_json(TM_HTTP_GET, url);
  free(url);
  return root;
}

static cJSON *throwawaymail_message_raw(const cJSON *msg,
                                        const char *recipient) {
  cJSON *raw = cJSON_CreateObject();
  if (!raw)
    return NULL;

  const cJSON *message_id =
      cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "message_id");
  const cJSON *from_address =
      cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "from_address");
  const cJSON *from_name =
      cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "from_name");
  const cJSON *subject =
      cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "subject");
  const cJSON *received_at =
      cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "received_at");
  const cJSON *read = cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "read");
  const cJSON *text = cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "text");
  const cJSON *html = cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "html");
  const cJSON *size = cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "size");

  cJSON_AddStringToObject(raw, "id", TM_JSON_STR(message_id, ""));
  cJSON_AddStringToObject(raw, "messageId", TM_JSON_STR(message_id, ""));
  cJSON_AddStringToObject(raw, "from_address", TM_JSON_STR(from_address, ""));
  cJSON_AddStringToObject(raw, "fromName", TM_JSON_STR(from_name, ""));
  cJSON_AddStringToObject(raw, "subject", TM_JSON_STR(subject, ""));
  cJSON_AddStringToObject(raw, "received_at", TM_JSON_STR(received_at, ""));
  cJSON_AddStringToObject(raw, "text", TM_JSON_STR(text, ""));
  cJSON_AddStringToObject(raw, "html", TM_JSON_STR(html, ""));

  const cJSON *to_arr =
      cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "to_addresses");
  const cJSON *first_to =
      cJSON_IsArray(to_arr) ? cJSON_GetArrayItem((cJSON *)to_arr, 0) : NULL;
  cJSON_AddStringToObject(raw, "to", TM_JSON_STR(first_to, recipient));
  if (read)
    cJSON_AddItemToObject(raw, "read", cJSON_Duplicate((cJSON *)read, 1));
  if (size)
    cJSON_AddItemToObject(raw, "size", cJSON_Duplicate((cJSON *)size, 1));
  return raw;
}

tm_email_t *tm_provider_throwawaymail_get_emails(const char *mailbox_id,
                                                 const char *email,
                                                 int *count) {
  *count = -1;
  if (!mailbox_id || !mailbox_id[0] || !email || !email[0])
    return NULL;

  char *mid_enc = throwawaymail_urlencode(mailbox_id);
  if (!mid_enc)
    return NULL;
  size_t cap = strlen(THROWAWAYMAIL_BASE) + strlen(mid_enc) + 24;
  char *url = (char *)malloc(cap);
  if (!url) {
    free(mid_enc);
    return NULL;
  }
  snprintf(url, cap, "%s/mailboxes/%s/messages", THROWAWAYMAIL_BASE, mid_enc);
  free(mid_enc);

  cJSON *arr = throwawaymail_json(TM_HTTP_GET, url);
  free(url);
  if (!arr || !cJSON_IsArray(arr)) {
    cJSON_Delete(arr);
    return NULL;
  }

  int n = cJSON_GetArraySize(arr);
  *count = n;
  if (n == 0) {
    cJSON_Delete(arr);
    return NULL;
  }

  tm_email_t *emails = tm_emails_new(n);
  if (!emails) {
    *count = -1;
    cJSON_Delete(arr);
    return NULL;
  }

  for (int i = 0; i < n; i++) {
    cJSON *item = cJSON_GetArrayItem(arr, i);
    const char *message_id =
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(item, "message_id"), "");
    cJSON *detail = message_id[0]
                        ? throwawaymail_fetch_message(mailbox_id, message_id)
                        : NULL;
    cJSON *raw = throwawaymail_message_raw(detail ? detail : item, email);
    emails[i] = tm_normalize_email(raw ? raw : (detail ? detail : item), email);
    if (raw)
      cJSON_Delete(raw);
    if (detail)
      cJSON_Delete(detail);
  }

  cJSON_Delete(arr);
  return emails;
}
