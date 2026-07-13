/**
 * Mailinator -- https://mailinator.com
 */

#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAILINATOR_BASE "https://mailinator.com"
#define MAILINATOR_PUBLIC_DOMAIN "public"

static const char *mailinator_headers[] = {
    "Accept: application/json",
    "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
    "Cache-Control: no-cache",
    "Pragma: no-cache",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    NULL};

static void mailinator_seed_random_once(void) {
  static int seeded = 0;
  if (!seeded) {
    srand((unsigned)time(NULL) ^ (unsigned)clock());
    seeded = 1;
  }
}

static void mailinator_random_string(char *buf, int len) {
  const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
  for (int i = 0; i < len; i++) {
    buf[i] = chars[rand() % (int)(sizeof(chars) - 1)];
  }
  buf[len] = '\0';
}

static cJSON *mailinator_request_json(const char *url) {
  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, mailinator_headers, NULL, 15);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }

  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  return root;
}

static cJSON *mailinator_parse_messages(cJSON *root) {
  if (cJSON_IsArray(root)) {
    return root;
  }
  if (!root) {
    return NULL;
  }
  cJSON *msgs = cJSON_GetObjectItemCaseSensitive(root, "msgs");
  if (cJSON_IsArray(msgs)) {
    return msgs;
  }
  cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
  if (cJSON_IsArray(data)) {
    return data;
  }
  return NULL;
}

static char *mailinator_json_to_string(const cJSON *item) {
  if (!item) {
    return tm_strdup("");
  }
  if (cJSON_IsString(item) && item->valuestring) {
    return tm_strdup(item->valuestring);
  }
  if (cJSON_IsNumber(item)) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%g", item->valuedouble);
    return tm_strdup(buf);
  }
  if (cJSON_IsBool(item)) {
    return tm_strdup(cJSON_IsTrue(item) ? "true" : "false");
  }
  return tm_strdup("");
}

static char *mailinator_attachment_url(const cJSON *item) {
  char *url = mailinator_json_to_string(item);
  if (!url) {
    return NULL;
  }
  if (url[0] && strncmp(url, "http://", 7) != 0 &&
      strncmp(url, "https://", 8) != 0) {
    size_t cap = strlen(MAILINATOR_BASE) + strlen(url) + 1;
    char *full = (char *)malloc(cap);
    if (!full) {
      free(url);
      return NULL;
    }
    snprintf(full, cap, "%s%s", MAILINATOR_BASE, url);
    free(url);
    return full;
  }
  return url;
}

static char *mailinator_first_text(const cJSON *obj, const char **keys,
                                   int key_count) {
  for (int i = 0; i < key_count; i++) {
    char *s = mailinator_json_to_string(
        cJSON_GetObjectItemCaseSensitive((cJSON *)obj, keys[i]));
    if (s && s[0]) {
      return s;
    }
    free(s);
  }
  return tm_strdup("");
}

static cJSON *mailinator_flatten_message(const cJSON *summary,
                                         const char *recipient,
                                         const cJSON *text_payload,
                                         const cJSON *html_payload,
                                         const cJSON *attachments_payload) {
  cJSON *raw = cJSON_CreateObject();
  cJSON *attachments = cJSON_CreateArray();
  cJSON *created_at = NULL;
  char *id = NULL;
  char *from = NULL;
  char *to = NULL;
  char *subject = NULL;
  char *text = NULL;
  char *html = NULL;
  const cJSON *id_item = NULL;

  if (!raw) {
    cJSON_Delete(attachments);
    return NULL;
  }

  id_item = cJSON_GetObjectItemCaseSensitive((cJSON *)summary, "id");
  if (!id_item)
    id_item = cJSON_GetObjectItemCaseSensitive((cJSON *)summary, "messageId");
  id = mailinator_json_to_string(id_item);

  from = mailinator_json_to_string(
      cJSON_GetObjectItemCaseSensitive((cJSON *)summary, "from"));
  if (!from[0]) {
    free(from);
    from = mailinator_json_to_string(
        cJSON_GetObjectItemCaseSensitive((cJSON *)summary, "origfrom"));
  }

  to = mailinator_json_to_string(
      cJSON_GetObjectItemCaseSensitive((cJSON *)summary, "to"));
  if (!to[0]) {
    free(to);
    to = tm_strdup(recipient ? recipient : "");
  }

  subject = mailinator_json_to_string(
      cJSON_GetObjectItemCaseSensitive((cJSON *)summary, "subject"));

  text = mailinator_first_text(
      text_payload, (const char *[]){"text/plain", "text", "body"}, 3);
  html = mailinator_first_text(
      html_payload, (const char *[]){"text/html", "html", "body"}, 3);

  created_at = cJSON_Duplicate(
      cJSON_GetObjectItemCaseSensitive((cJSON *)summary, "time"), 1);
  if (!created_at) {
    created_at = cJSON_Duplicate(
        cJSON_GetObjectItemCaseSensitive((cJSON *)summary, "date"), 1);
  }
  if (!created_at) {
    created_at = cJSON_CreateNull();
  }

  if (!id || !id[0]) {
    char fallback[1024];
    snprintf(fallback, sizeof(fallback), "%s\n%s\n%s\n%s", from ? from : "",
             subject ? subject : "", recipient ? recipient : "", to ? to : "");
    free(id);
    id = tm_strdup(fallback);
  }

  cJSON_AddStringToObject(raw, "id", id ? id : "");
  cJSON_AddStringToObject(raw, "from", from ? from : "");
  cJSON_AddStringToObject(raw, "to", to ? to : (recipient ? recipient : ""));
  cJSON_AddStringToObject(raw, "subject", subject ? subject : "");
  cJSON_AddStringToObject(raw, "text", text ? text : "");
  cJSON_AddStringToObject(raw, "html", html ? html : "");
  cJSON_AddItemToObject(raw, "createdAt", created_at);
  cJSON_AddBoolToObject(raw, "isRead", 0);

  if (attachments_payload) {
    cJSON *src = cJSON_GetObjectItemCaseSensitive((cJSON *)attachments_payload,
                                                  "attachments");
    if (cJSON_IsArray(src)) {
      const cJSON *att;
      cJSON_ArrayForEach(att, src) {
        cJSON *item = cJSON_CreateObject();
        if (!item)
          continue;
        {
          char *filename = mailinator_json_to_string(
              cJSON_GetObjectItemCaseSensitive((cJSON *)att, "name"));
          cJSON_AddStringToObject(item, "filename", filename ? filename : "");
          free(filename);
        }
        {
          const cJSON *size =
              cJSON_GetObjectItemCaseSensitive((cJSON *)att, "size");
          if (!size)
            size = cJSON_GetObjectItemCaseSensitive((cJSON *)att, "filesize");
          if (size && cJSON_IsNumber(size)) {
            cJSON_AddNumberToObject(item, "size", size->valuedouble);
          } else {
            cJSON_AddNullToObject(item, "size");
          }
        }
        {
          char *content_type = mailinator_json_to_string(
              cJSON_GetObjectItemCaseSensitive((cJSON *)att, "contentType"));
          cJSON_AddStringToObject(item, "contentType",
                                  content_type ? content_type : "");
          free(content_type);
        }
        {
          char *url = mailinator_attachment_url(
              cJSON_GetObjectItemCaseSensitive((cJSON *)att, "downloadUrl"));
          cJSON_AddStringToObject(item, "url", url ? url : "");
          free(url);
        }
        cJSON_AddItemToArray(attachments, item);
      }
    }
  }
  cJSON_AddItemToObject(raw, "attachments", attachments);

  free(id);
  free(from);
  free(to);
  free(subject);
  free(text);
  free(html);
  return raw;
}

tm_email_info_t *tm_provider_mailinator_generate(void) {
  mailinator_seed_random_once();

  char local[13];
  mailinator_random_string(local, 12);

  tm_email_info_t *info = tm_email_info_new();
  if (!info) {
    return NULL;
  }
  info->channel = CHANNEL_MAILINATOR;
  {
    size_t cap = strlen(local) + strlen("@mailinator.com") + 1;
    info->email = (char *)malloc(cap);
    if (!info->email) {
      tm_free_email_info(info);
      return NULL;
    }
    snprintf(info->email, cap, "%s@mailinator.com", local);
  }
  return info;
}

static cJSON *mailinator_fetch_detail(const char *message_id,
                                      const char *suffix) {
  char url[768];
  snprintf(url, sizeof(url), "%s/api/v2/domains/%s/messages/%s/%s",
           MAILINATOR_BASE, MAILINATOR_PUBLIC_DOMAIN, message_id, suffix);
  return mailinator_request_json(url);
}

tm_email_t *tm_provider_mailinator_get_emails(const char *email, int *count) {
  *count = -1;
  if (!email || !email[0])
    return NULL;

  char inbox[256];
  const char *at = strchr(email, '@');
  size_t inbox_len = at ? (size_t)(at - email) : strlen(email);
  if (inbox_len == 0 || inbox_len >= sizeof(inbox)) {
    return NULL;
  }
  memcpy(inbox, email, inbox_len);
  inbox[inbox_len] = '\0';

  char url[768];
  snprintf(url, sizeof(url), "%s/api/v2/domains/%s/inboxes/%s", MAILINATOR_BASE,
           MAILINATOR_PUBLIC_DOMAIN, inbox);

  cJSON *root = mailinator_request_json(url);
  if (!root)
    return NULL;

  cJSON *messages = mailinator_parse_messages(root);
  if (!messages) {
    cJSON_Delete(root);
    *count = 0;
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
    cJSON_Delete(root);
    *count = -1;
    return NULL;
  }

  for (int i = 0; i < n; i++) {
    cJSON *msg = cJSON_GetArrayItem(messages, i);
    const char *mid =
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(msg, "id"), "");
    if (!mid[0]) {
      mid = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(msg, "messageId"), "");
    }

    cJSON *text_payload = mid[0] ? mailinator_fetch_detail(mid, "text") : NULL;
    cJSON *html_payload =
        mid[0] ? mailinator_fetch_detail(mid, "texthtml") : NULL;
    cJSON *attachments_payload =
        mid[0] ? mailinator_fetch_detail(mid, "attachments") : NULL;

    cJSON *flat = mailinator_flatten_message(msg, email, text_payload,
                                             html_payload, attachments_payload);
    emails[i] = tm_normalize_email(flat ? flat : msg, email);

    cJSON_Delete(flat);
    cJSON_Delete(text_payload);
    cJSON_Delete(html_payload);
    cJSON_Delete(attachments_payload);
  }

  cJSON_Delete(root);
  return emails;
}
