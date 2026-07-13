/**
 * TempMailC -- https://tempmailc.com
 */

#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEMPMAILC_BASE "https://tempmailc.com/api/v1"

static const char *tempmailc_headers[] = {"Accept: application/json", NULL};

static char *tempmailc_urlencode(const char *s) {
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

static cJSON *tempmailc_get_json(const char *url) {
  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, tempmailc_headers, NULL, 15);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }
  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  return root;
}

tm_email_info_t *tm_provider_tempmailc_generate(void) {
  cJSON *root = tempmailc_get_json(TEMPMAILC_BASE "/new");
  if (!root)
    return NULL;
  cJSON *ok = cJSON_GetObjectItemCaseSensitive(root, "ok");
  const char *email =
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "email"), "");
  if (!cJSON_IsTrue(ok) || !email[0] || !strchr(email, '@')) {
    cJSON_Delete(root);
    return NULL;
  }

  tm_email_info_t *info = tm_email_info_new();
  if (!info) {
    cJSON_Delete(root);
    return NULL;
  }
  info->channel = CHANNEL_TEMPMAILC;
  info->email = tm_strdup(email);
  cJSON_Delete(root);
  return info;
}

static cJSON *tempmailc_fetch_message(const char *email,
                                      const char *message_id) {
  char *mail_enc = tempmailc_urlencode(email);
  char *id_enc = tempmailc_urlencode(message_id);
  if (!mail_enc || !id_enc) {
    free(mail_enc);
    free(id_enc);
    return NULL;
  }
  size_t cap = strlen(TEMPMAILC_BASE) + strlen(mail_enc) + strlen(id_enc) + 64;
  char *url = (char *)malloc(cap);
  if (!url) {
    free(mail_enc);
    free(id_enc);
    return NULL;
  }
  snprintf(url, cap, "%s/message?email=%s&msg_id=%s", TEMPMAILC_BASE, mail_enc,
           id_enc);
  free(mail_enc);
  free(id_enc);

  cJSON *root = tempmailc_get_json(url);
  free(url);
  if (!root)
    return NULL;
  cJSON *ok = cJSON_GetObjectItemCaseSensitive(root, "ok");
  if (cJSON_IsBool(ok) && !cJSON_IsTrue(ok)) {
    cJSON_Delete(root);
    return NULL;
  }
  return root;
}

static cJSON *tempmailc_flatten_list_message(const cJSON *raw,
                                             const char *recipient) {
  cJSON *flat = cJSON_CreateObject();
  if (!flat)
    return NULL;

  cJSON *id = cJSON_GetObjectItemCaseSensitive((cJSON *)raw, "id");
  cJSON *from = cJSON_GetObjectItemCaseSensitive((cJSON *)raw, "from");
  cJSON *subject = cJSON_GetObjectItemCaseSensitive((cJSON *)raw, "subject");
  cJSON *ts = cJSON_GetObjectItemCaseSensitive((cJSON *)raw, "ts");
  cJSON *read = cJSON_GetObjectItemCaseSensitive((cJSON *)raw, "read");

  cJSON_AddStringToObject(flat, "id", TM_JSON_STR(id, ""));
  cJSON_AddStringToObject(flat, "from", TM_JSON_STR(from, ""));
  cJSON_AddStringToObject(flat, "to", recipient);
  cJSON_AddStringToObject(flat, "subject", TM_JSON_STR(subject, ""));
  if (ts)
    cJSON_AddItemToObject(flat, "timestamp", cJSON_Duplicate(ts, 1));
  if (read)
    cJSON_AddItemToObject(flat, "read", cJSON_Duplicate(read, 1));
  return flat;
}

tm_email_t *tm_provider_tempmailc_get_emails(const char *email, int *count) {
  *count = -1;
  if (!email || !email[0])
    return NULL;

  char *enc = tempmailc_urlencode(email);
  if (!enc)
    return NULL;
  size_t cap = strlen(TEMPMAILC_BASE) + strlen(enc) + 32;
  char *url = (char *)malloc(cap);
  if (!url) {
    free(enc);
    return NULL;
  }
  snprintf(url, cap, "%s/inbox?email=%s", TEMPMAILC_BASE, enc);
  free(enc);

  cJSON *root = tempmailc_get_json(url);
  free(url);
  if (!root)
    return NULL;

  cJSON *ok = cJSON_GetObjectItemCaseSensitive(root, "ok");
  if (!cJSON_IsTrue(ok)) {
    cJSON_Delete(root);
    return NULL;
  }

  cJSON *arr = cJSON_GetObjectItemCaseSensitive(root, "messages");
  if (!arr || !cJSON_IsArray(arr)) {
    *count = 0;
    cJSON_Delete(root);
    return NULL;
  }

  int n = cJSON_GetArraySize(arr);
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
    cJSON *item = cJSON_GetArrayItem(arr, i);
    const char *id =
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(item, "id"), "");
    cJSON *detail = id[0] ? tempmailc_fetch_message(email, id) : NULL;
    if (detail) {
      emails[i] = tm_normalize_email(detail, email);
      cJSON_Delete(detail);
      continue;
    }

    cJSON *flat = tempmailc_flatten_list_message(item, email);
    emails[i] = tm_normalize_email(flat ? flat : item, email);
    if (flat)
      cJSON_Delete(flat);
  }

  cJSON_Delete(root);
  return emails;
}
