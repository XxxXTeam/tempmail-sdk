/**
 * MailToYou / m2u -- https://m2u.io
 */

#include "tempmail_internal.h"
#include <ctype.h>

#define M2U_BASE "https://api.m2u.io"

static const char *m2u_headers[] = {
    "Accept: application/json",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
    NULL};

static const char *m2u_post_headers[] = {
    "Accept: application/json", "Content-Type: application/json",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
    NULL};

static char *m2u_urlencode(const char *s) {
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

static char *m2u_trim_copy(const char *s) {
  if (!s)
    return tm_strdup("");
  while (*s && isspace((unsigned char)*s))
    s++;
  const char *end = s + strlen(s);
  while (end > s && isspace((unsigned char)end[-1]))
    end--;
  size_t len = (size_t)(end - s);
  char *out = (char *)malloc(len + 1);
  if (!out)
    return NULL;
  memcpy(out, s, len);
  out[len] = '\0';
  return out;
}

static char *m2u_pick_str(const cJSON *obj, const char **keys, int key_count) {
  char *raw = tm_json_get_str(obj, keys, key_count);
  char *trimmed = m2u_trim_copy(raw);
  free(raw);
  return trimmed;
}

static cJSON *m2u_json(tm_http_method_t method, const char *url,
                       const char *body) {
  const char **headers =
      method == TM_HTTP_POST ? m2u_post_headers : m2u_headers;
  tm_http_response_t *resp = tm_http_request(method, url, headers, body, 15);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }
  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  return root;
}

static char *m2u_pack_token(const char *token, const char *view_token) {
  cJSON *root = cJSON_CreateObject();
  if (!root)
    return NULL;
  cJSON_AddStringToObject(root, "token", token ? token : "");
  cJSON_AddStringToObject(root, "viewToken", view_token ? view_token : "");
  char *out = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  return out;
}

static void m2u_unpack_token(const char *packed, char **token_out,
                             char **view_token_out) {
  *token_out = NULL;
  *view_token_out = NULL;
  const char *value = packed ? packed : "";
  while (*value && isspace((unsigned char)*value))
    value++;
  size_t len = strlen(value);
  while (len > 0 && isspace((unsigned char)value[len - 1]))
    len--;
  if (len == 0)
    return;

  if (value[0] == '{') {
    char *copy = (char *)malloc(len + 1);
    if (!copy)
      return;
    memcpy(copy, value, len);
    copy[len] = '\0';
    cJSON *root = cJSON_Parse(copy);
    free(copy);
    if (!root)
      return;
    *token_out = m2u_pick_str(root, (const char *[]){"token"}, 1);
    *view_token_out = m2u_pick_str(root, (const char *[]){"viewToken"}, 1);
    cJSON_Delete(root);
    if (!*token_out)
      *token_out = tm_strdup("");
    if (!*view_token_out)
      *view_token_out = tm_strdup("");
    return;
  }

  *token_out = m2u_trim_copy(value);
  *view_token_out = tm_strdup("");
}

static cJSON *m2u_flatten_message(const cJSON *raw, const char *recipient) {
  cJSON *flat = cJSON_CreateObject();
  if (!flat)
    return NULL;

  char *id = m2u_pick_str(raw, (const char *[]){"id", "message_id"}, 2);
  char *from = m2u_pick_str(
      raw, (const char *[]){"from_addr", "from_address", "from"}, 3);
  char *text =
      m2u_pick_str(raw, (const char *[]){"text_body", "body_text", "text"}, 3);
  char *html =
      m2u_pick_str(raw, (const char *[]){"html_body", "body_html", "html"}, 3);
  char *date =
      m2u_pick_str(raw, (const char *[]){"received_at", "created_at"}, 2);

  cJSON_AddStringToObject(flat, "id", id ? id : "");
  cJSON_AddStringToObject(flat, "from", from ? from : "");
  cJSON_AddStringToObject(flat, "to", recipient ? recipient : "");
  cJSON_AddStringToObject(
      flat, "subject",
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive((cJSON *)raw, "subject"),
                  ""));
  cJSON_AddStringToObject(flat, "text", text ? text : "");
  cJSON_AddStringToObject(flat, "html", html ? html : "");
  cJSON_AddStringToObject(flat, "date", date ? date : "");
  cJSON_AddBoolToObject(
      flat, "is_read",
      cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive((cJSON *)raw, "is_read")));
  if (cJSON_GetObjectItemCaseSensitive((cJSON *)raw, "attachments")) {
    cJSON_AddItemToObject(
        flat, "attachments",
        cJSON_Duplicate(
            cJSON_GetObjectItemCaseSensitive((cJSON *)raw, "attachments"), 1));
  } else {
    cJSON_AddItemToObject(flat, "attachments", cJSON_CreateArray());
  }

  free(id);
  free(from);
  free(text);
  free(html);
  free(date);
  return flat;
}

tm_email_info_t *tm_provider_m2u_generate(void) {
  cJSON *root = m2u_json(TM_HTTP_POST, M2U_BASE "/v1/mailboxes/auto", "{}");
  if (!root)
    return NULL;

  cJSON *mailbox = cJSON_GetObjectItemCaseSensitive(root, "mailbox");
  if (!cJSON_IsObject(mailbox)) {
    cJSON_Delete(root);
    return NULL;
  }

  char *local_part = m2u_pick_str(mailbox, (const char *[]){"local_part"}, 1);
  char *domain = m2u_pick_str(mailbox, (const char *[]){"domain"}, 1);
  char *token = m2u_pick_str(mailbox, (const char *[]){"token"}, 1);
  char *view_token = m2u_pick_str(mailbox, (const char *[]){"view_token"}, 1);
  char *created_at = m2u_pick_str(mailbox, (const char *[]){"created_at"}, 1);
  const cJSON *expires_at =
      cJSON_GetObjectItemCaseSensitive(mailbox, "expires_at");

  char email[512];
  email[0] = '\0';
  if (local_part && domain && local_part[0] && domain[0]) {
    snprintf(email, sizeof(email), "%s@%s", local_part, domain);
  }

  if (!email[0] || !token || !token[0] || !view_token || !view_token[0]) {
    free(local_part);
    free(domain);
    free(token);
    free(view_token);
    free(created_at);
    cJSON_Delete(root);
    return NULL;
  }

  char *packed = m2u_pack_token(token, view_token);
  tm_email_info_t *info = tm_email_info_new();
  if (!info || !packed) {
    if (info)
      tm_free_email_info(info);
    free(local_part);
    free(domain);
    free(token);
    free(view_token);
    free(created_at);
    free(packed);
    cJSON_Delete(root);
    return NULL;
  }

  info->channel = CHANNEL_M2U;
  info->email = tm_strdup(email);
  info->token = packed;
  if (cJSON_IsNumber(expires_at)) {
    info->expires_at = (long long)expires_at->valuedouble;
  }
  if (created_at && created_at[0]) {
    info->created_at = tm_strdup(created_at);
  }

  free(local_part);
  free(domain);
  free(token);
  free(view_token);
  free(created_at);
  cJSON_Delete(root);
  return info;
}

static cJSON *m2u_fetch_detail(const char *token, const char *view_token,
                               const char *message_id) {
  char *token_enc = m2u_urlencode(token);
  char *id_enc = m2u_urlencode(message_id);
  char *view_enc = m2u_urlencode(view_token);
  if (!token_enc || !id_enc || !view_enc) {
    free(token_enc);
    free(id_enc);
    free(view_enc);
    return NULL;
  }

  size_t cap = strlen(M2U_BASE) + strlen(token_enc) + strlen(id_enc) +
               strlen(view_enc) + 40;
  char *url = (char *)malloc(cap);
  if (!url) {
    free(token_enc);
    free(id_enc);
    free(view_enc);
    return NULL;
  }
  snprintf(url, cap, "%s/v1/mailboxes/%s/messages/%s?view=%s", M2U_BASE,
           token_enc, id_enc, view_enc);
  free(token_enc);
  free(id_enc);
  free(view_enc);

  cJSON *root = m2u_json(TM_HTTP_GET, url, NULL);
  free(url);
  if (!root)
    return NULL;
  cJSON *message = cJSON_GetObjectItemCaseSensitive(root, "message");
  cJSON *dup = cJSON_IsObject(message) ? cJSON_Duplicate(message, 1) : NULL;
  cJSON_Delete(root);
  return dup;
}

tm_email_t *tm_provider_m2u_get_emails(const char *token, const char *email,
                                       int *count) {
  *count = -1;
  char *mailbox_token = NULL;
  char *view_token = NULL;
  m2u_unpack_token(token, &mailbox_token, &view_token);
  if (!mailbox_token || !mailbox_token[0]) {
    free(mailbox_token);
    free(view_token);
    return NULL;
  }
  if (!view_token || !view_token[0]) {
    free(mailbox_token);
    free(view_token);
    return NULL;
  }
  if (!email || !email[0]) {
    free(mailbox_token);
    free(view_token);
    return NULL;
  }

  char *mailbox_enc = m2u_urlencode(mailbox_token);
  char *view_enc = m2u_urlencode(view_token);
  if (!mailbox_enc || !view_enc) {
    free(mailbox_token);
    free(view_token);
    free(mailbox_enc);
    free(view_enc);
    return NULL;
  }

  size_t cap = strlen(M2U_BASE) + strlen(mailbox_enc) + strlen(view_enc) + 32;
  char *url = (char *)malloc(cap);
  if (!url) {
    free(mailbox_token);
    free(view_token);
    free(mailbox_enc);
    free(view_enc);
    return NULL;
  }
  snprintf(url, cap, "%s/v1/mailboxes/%s/messages?view=%s", M2U_BASE,
           mailbox_enc, view_enc);
  free(mailbox_enc);
  free(view_enc);

  cJSON *root = m2u_json(TM_HTTP_GET, url, NULL);
  free(url);
  if (!root) {
    free(mailbox_token);
    free(view_token);
    return NULL;
  }

  cJSON *messages = cJSON_GetObjectItemCaseSensitive(root, "messages");
  int n = cJSON_IsArray(messages) ? cJSON_GetArraySize(messages) : 0;
  *count = n;
  if (n <= 0) {
    cJSON_Delete(root);
    free(mailbox_token);
    free(view_token);
    return NULL;
  }

  tm_email_t *emails = tm_emails_new(n);
  if (!emails) {
    *count = -1;
    cJSON_Delete(root);
    free(mailbox_token);
    free(view_token);
    return NULL;
  }

  for (int i = 0; i < n; i++) {
    cJSON *row = cJSON_GetArrayItem(messages, i);
    cJSON *raw = row;
    cJSON *detail = NULL;
    const char *message_id =
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(row, "id"), "");
    if (message_id[0]) {
      detail = m2u_fetch_detail(mailbox_token, view_token, message_id);
      if (detail)
        raw = detail;
    }
    cJSON *flat = m2u_flatten_message(raw, email);
    emails[i] = tm_normalize_email(flat ? flat : raw, email);
    cJSON_Delete(flat);
    cJSON_Delete(detail);
  }

  cJSON_Delete(root);
  free(mailbox_token);
  free(view_token);
  return emails;
}
