/**
 * GetNada -- https://getnada.net
 */

#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GETNADA_BASE "https://getnada.net/api"

static const char *getnada_json_headers[] = {"Accept: application/json", NULL};

static const char *getnada_post_headers[] = {
    "Accept: application/json", "Content-Type: application/json", NULL};

static char *getnada_urlencode(const char *s) {
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

static cJSON *getnada_json(tm_http_method_t method, const char *url,
                           const char *body) {
  const char **headers = body ? getnada_post_headers : getnada_json_headers;
  tm_http_response_t *resp = tm_http_request(method, url, headers, body, 15);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }
  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  return root;
}

static void getnada_random_local(char *buf, size_t cap) {
  static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
  if (cap < 4)
    return;
  size_t len = cap - 1;
  if (len > 19)
    len = 19;
  buf[0] = 's';
  buf[1] = 'd';
  buf[2] = 'k';
  for (size_t i = 3; i < len; i++) {
    buf[i] = chars[rand() % (sizeof(chars) - 1)];
  }
  buf[len] = '\0';
}

static char *getnada_pick_domain(const char *preferred) {
  cJSON *root = getnada_json(TM_HTTP_GET, GETNADA_BASE "/public/domains", NULL);
  if (!root)
    return NULL;
  cJSON *domains = cJSON_GetObjectItemCaseSensitive(root, "domains");
  char *first = NULL;
  char wanted[256];
  wanted[0] = '\0';
  if (preferred && preferred[0]) {
    snprintf(wanted, sizeof(wanted), "%s",
             preferred[0] == '@' ? preferred + 1 : preferred);
    for (char *p = wanted; *p; p++)
      if (*p >= 'A' && *p <= 'Z')
        *p = (char)(*p - 'A' + 'a');
  }
  if (cJSON_IsArray(domains)) {
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, domains) {
      if (!cJSON_IsString(item) || !item->valuestring || !item->valuestring[0])
        continue;
      if (!first)
        first = tm_strdup(item->valuestring);
      if (wanted[0] && strcmp(item->valuestring, wanted) == 0) {
        free(first);
        first = tm_strdup(item->valuestring);
        break;
      }
      if (!wanted[0] && strcmp(item->valuestring, "getnada.net") == 0) {
        free(first);
        first = tm_strdup("getnada.net");
        break;
      }
    }
  }
  cJSON_Delete(root);
  if (wanted[0] && (!first || strcmp(first, wanted) != 0)) {
    free(first);
    return NULL;
  }
  return first;
}

tm_email_info_t *tm_provider_getnada_generate(const char *domain_hint) {
  char *domain = getnada_pick_domain(domain_hint);
  if (!domain || !domain[0]) {
    free(domain);
    return NULL;
  }

  char local[32];
  getnada_random_local(local, sizeof(local));
  size_t email_cap = strlen(local) + strlen(domain) + 2;
  char *requested = (char *)malloc(email_cap);
  if (!requested) {
    free(domain);
    return NULL;
  }
  snprintf(requested, email_cap, "%s@%s", local, domain);
  free(domain);

  cJSON *payload = cJSON_CreateObject();
  if (!payload) {
    free(requested);
    return NULL;
  }
  cJSON_AddStringToObject(payload, "email", requested);
  char *body = cJSON_PrintUnformatted(payload);
  cJSON_Delete(payload);
  if (!body) {
    free(requested);
    return NULL;
  }

  cJSON *root = getnada_json(TM_HTTP_POST, GETNADA_BASE "/inbox/open", body);
  free(body);
  if (!root) {
    free(requested);
    return NULL;
  }

  const char *token =
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "token"), "");
  const char *recipient = TM_JSON_STR(
      cJSON_GetObjectItemCaseSensitive(root, "recipient"), requested);
  if (!token[0] || !recipient[0] || !strchr(recipient, '@')) {
    free(requested);
    cJSON_Delete(root);
    return NULL;
  }

  tm_email_info_t *info = tm_email_info_new();
  if (!info) {
    free(requested);
    cJSON_Delete(root);
    return NULL;
  }
  info->channel = CHANNEL_GETNADA;
  info->email = tm_strdup(recipient);
  info->token = tm_strdup(token);
  free(requested);
  cJSON_Delete(root);
  return info;
}

static void getnada_add_dup(cJSON *dst, const char *dst_key, const cJSON *src,
                            const char *src_key) {
  const cJSON *item = cJSON_GetObjectItemCaseSensitive((cJSON *)src, src_key);
  if (item)
    cJSON_AddItemToObject(dst, dst_key, cJSON_Duplicate((cJSON *)item, 1));
}

static cJSON *getnada_message_raw(const cJSON *msg, const char *recipient) {
  cJSON *raw = cJSON_CreateObject();
  if (!raw)
    return NULL;

  getnada_add_dup(raw, "id", msg, "id");
  getnada_add_dup(raw, "subject", msg, "subject");
  getnada_add_dup(raw, "received_at", msg, "received_at");
  getnada_add_dup(raw, "text", msg, "text_plain");
  if (!cJSON_GetObjectItemCaseSensitive(raw, "text"))
    getnada_add_dup(raw, "text", msg, "text");
  getnada_add_dup(raw, "html", msg, "html_sanitized");
  if (!cJSON_GetObjectItemCaseSensitive(raw, "html"))
    getnada_add_dup(raw, "html", msg, "html");

  const char *from = TM_JSON_STR(
      cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "from_addr"), "");
  if (!from[0])
    from =
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "from"), "");
  cJSON_AddStringToObject(raw, "from", from);

  const char *to = TM_JSON_STR(
      cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "to"), recipient);
  cJSON_AddStringToObject(raw, "to", to && to[0] ? to : recipient);

  const cJSON *read_at =
      cJSON_GetObjectItemCaseSensitive((cJSON *)msg, "read_at");
  if (cJSON_IsString(read_at) && read_at->valuestring &&
      read_at->valuestring[0]) {
    cJSON_AddBoolToObject(raw, "read", 1);
  } else {
    getnada_add_dup(raw, "read", msg, "read");
  }
  return raw;
}

static cJSON *getnada_fetch_message(const char *token, const char *message_id) {
  char *id_enc = getnada_urlencode(message_id);
  char *token_enc = getnada_urlencode(token);
  if (!id_enc || !token_enc) {
    free(id_enc);
    free(token_enc);
    return NULL;
  }
  size_t cap = strlen(GETNADA_BASE) + strlen(id_enc) + strlen(token_enc) + 40;
  char *url = (char *)malloc(cap);
  if (!url) {
    free(id_enc);
    free(token_enc);
    return NULL;
  }
  snprintf(url, cap, "%s/inbox/message?id=%s&token=%s", GETNADA_BASE, id_enc,
           token_enc);
  free(id_enc);
  free(token_enc);

  cJSON *root = getnada_json(TM_HTTP_GET, url, NULL);
  free(url);
  if (!root)
    return NULL;
  cJSON *message = cJSON_GetObjectItemCaseSensitive(root, "message");
  cJSON *dup = message ? cJSON_Duplicate(message, 1) : NULL;
  cJSON_Delete(root);
  return dup;
}

tm_email_t *tm_provider_getnada_get_emails(const char *token, const char *email,
                                           int *count) {
  *count = -1;
  if (!token || !token[0] || !email || !email[0])
    return NULL;

  char *token_enc = getnada_urlencode(token);
  if (!token_enc)
    return NULL;
  size_t cap = strlen(GETNADA_BASE) + strlen(token_enc) + 32;
  char *url = (char *)malloc(cap);
  if (!url) {
    free(token_enc);
    return NULL;
  }
  snprintf(url, cap, "%s/inbox/messages?token=%s", GETNADA_BASE, token_enc);
  free(token_enc);

  cJSON *root = getnada_json(TM_HTTP_GET, url, NULL);
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
    cJSON *detail = id[0] ? getnada_fetch_message(token, id) : NULL;
    cJSON *raw = getnada_message_raw(detail ? detail : item, email);
    emails[i] = tm_normalize_email(raw ? raw : (detail ? detail : item), email);
    if (raw)
      cJSON_Delete(raw);
    if (detail)
      cJSON_Delete(detail);
  }

  cJSON_Delete(root);
  return emails;
}
