/**
 * Tempy Email -- https://tempy.email
 */

#include "tempmail_internal.h"
#include <ctype.h>

#define TEMPY_EMAIL_BASE "https://tempy.email/api/v1"

static const char *tempy_email_headers[] = {
    "Accept: application/json",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
    NULL};

static const char *tempy_email_post_headers[] = {
    "Accept: application/json", "Content-Type: application/json",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
    NULL};

static char *tempy_email_urlencode(const char *s) {
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

static char *tempy_email_trim_copy(const char *s) {
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

static char *tempy_email_pick_str(const cJSON *obj, const char **keys,
                                  int key_count) {
  char *raw = tm_json_get_str(obj, keys, key_count);
  char *trimmed = tempy_email_trim_copy(raw);
  free(raw);
  return trimmed;
}

static cJSON *tempy_email_json(tm_http_method_t method, const char *url,
                               const char *body) {
  const char **headers =
      method == TM_HTTP_POST ? tempy_email_post_headers : tempy_email_headers;
  tm_http_response_t *resp = tm_http_request(method, url, headers, body, 15);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }
  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  return root;
}

static cJSON *tempy_email_flatten_message(const cJSON *raw,
                                          const char *recipient) {
  cJSON *flat = cJSON_CreateObject();
  if (!flat)
    return NULL;

  char *id = tempy_email_pick_str(
      raw, (const char *[]){"id", "messageId", "message_id", "mail_id"}, 4);
  char *from = tempy_email_pick_str(
      raw, (const char *[]){"from", "sender", "from_addr", "from_address"}, 4);
  char *text = tempy_email_pick_str(
      raw, (const char *[]){"text", "body_text", "text_body", "body"}, 4);
  char *html = tempy_email_pick_str(
      raw, (const char *[]){"html", "body_html", "html_body"}, 3);
  char *date = tempy_email_pick_str(
      raw, (const char *[]){"date", "received_at", "created_at"}, 3);
  char *to = tempy_email_pick_str(raw, (const char *[]){"to"}, 1);
  char *subject =
      tempy_email_pick_str(raw, (const char *[]){"subject", "mail_title"}, 2);

  cJSON_AddStringToObject(flat, "id", id ? id : "");
  cJSON_AddStringToObject(flat, "from", from ? from : "");
  cJSON_AddStringToObject(flat, "to",
                          (to && to[0]) ? to : (recipient ? recipient : ""));
  cJSON_AddStringToObject(flat, "subject", subject ? subject : "");
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
  free(to);
  free(subject);
  return flat;
}

tm_email_info_t *tm_provider_tempy_email_generate(const char *domain) {
  cJSON *body = cJSON_CreateObject();
  if (!body)
    return NULL;
  const char *wanted = domain ? domain : "";
  while (*wanted && isspace((unsigned char)*wanted))
    wanted++;
  const char *end = wanted + strlen(wanted);
  while (end > wanted && isspace((unsigned char)end[-1]))
    end--;
  if (end > wanted) {
    char *copy = (char *)malloc((size_t)(end - wanted) + 1);
    if (!copy) {
      cJSON_Delete(body);
      return NULL;
    }
    memcpy(copy, wanted, (size_t)(end - wanted));
    copy[end - wanted] = '\0';
    cJSON_AddStringToObject(body, "domain", copy);
    free(copy);
  }

  char *body_str = cJSON_PrintUnformatted(body);
  cJSON_Delete(body);
  if (!body_str)
    return NULL;

  cJSON *root =
      tempy_email_json(TM_HTTP_POST, TEMPY_EMAIL_BASE "/mailbox", body_str);
  free(body_str);
  if (!root)
    return NULL;

  const char *email =
      TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(root, "email"), "");
  if (!email[0]) {
    cJSON_Delete(root);
    return NULL;
  }

  tm_email_info_t *info = tm_email_info_new();
  if (!info) {
    cJSON_Delete(root);
    return NULL;
  }
  info->channel = CHANNEL_TEMPY_EMAIL;
  info->email = tm_strdup(email);
  const cJSON *exp = cJSON_GetObjectItemCaseSensitive(root, "expires_at");
  if (cJSON_IsNumber(exp)) {
    info->expires_at = (long long)exp->valuedouble;
  }
  cJSON_Delete(root);
  return info;
}

tm_email_t *tm_provider_tempy_email_get_emails(const char *email, int *count) {
  *count = -1;
  if (!email || !email[0]) {
    return NULL;
  }

  char *enc = tempy_email_urlencode(email);
  if (!enc)
    return NULL;

  size_t cap = strlen(TEMPY_EMAIL_BASE) + strlen(enc) + 24;
  char *url = (char *)malloc(cap);
  if (!url) {
    free(enc);
    return NULL;
  }
  snprintf(url, cap, "%s/mailbox/%s/messages", TEMPY_EMAIL_BASE, enc);
  free(enc);

  cJSON *root = tempy_email_json(TM_HTTP_GET, url, NULL);
  free(url);
  if (!root)
    return NULL;

  cJSON *rows = cJSON_GetObjectItemCaseSensitive(root, "messages");
  if (!cJSON_IsArray(rows) && cJSON_IsArray(root)) {
    rows = root;
  }
  int n = cJSON_IsArray(rows) ? cJSON_GetArraySize(rows) : 0;
  *count = n;
  if (n <= 0) {
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
    cJSON *row = cJSON_GetArrayItem(rows, i);
    cJSON *flat = tempy_email_flatten_message(row, email);
    emails[i] = tm_normalize_email(flat ? flat : row, email);
    cJSON_Delete(flat);
  }

  cJSON_Delete(root);
  return emails;
}
