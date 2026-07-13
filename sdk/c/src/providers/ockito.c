/**
 * ockito -- https://ockito.com/web-api
 */

#include "tempmail_internal.h"
#include <ctype.h>

#define OCKITO_BASE "https://ockito.com/web-api"

static const char *ockito_json_headers[] = {"Accept: application/json", NULL};

static const char *ockito_post_json_headers[] = {
    "Accept: application/json", "Content-Type: application/json", NULL};

static char *ockito_trim_copy(const char *s) {
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

static char *ockito_urlencode(const char *s) {
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

static char *ockito_pick_str(const cJSON *obj, const char **keys,
                             int key_count) {
  char *raw = tm_json_get_str(obj, keys, key_count);
  char *trimmed = ockito_trim_copy(raw);
  free(raw);
  return trimmed;
}

static void ockito_add_picked_str(cJSON *target, const char *name,
                                  const cJSON *source, const char **keys,
                                  int key_count) {
  char *value = ockito_pick_str(source, keys, key_count);
  cJSON_AddStringToObject(target, name, value ? value : "");
  free(value);
}

static cJSON *ockito_request_json(const char *method, const char *path,
                                  const char **headers, const char *body,
                                  long *status_out) {
  char url[1024];
  snprintf(url, sizeof(url), OCKITO_BASE "%s", path);
  tm_http_method_t m = strcmp(method, "POST") == 0 ? TM_HTTP_POST : TM_HTTP_GET;
  const char **use_headers = headers;
  if (!use_headers) {
    use_headers = ockito_json_headers;
  }
  tm_http_response_t *resp = tm_http_request(m, url, use_headers, body, 15);
  if (!resp)
    return NULL;
  if (status_out)
    *status_out = resp->status;
  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  return root;
}

static char *ockito_pack_token(const char *access_token,
                               const char *refresh_token) {
  cJSON *root = cJSON_CreateObject();
  if (!root)
    return NULL;
  cJSON_AddStringToObject(root, "access_token",
                          access_token ? access_token : "");
  cJSON_AddStringToObject(root, "refresh_token",
                          refresh_token ? refresh_token : "");
  char *out = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  return out;
}

static void ockito_unpack_token(const char *packed, char **access_token_out,
                                char **refresh_token_out) {
  *access_token_out = NULL;
  *refresh_token_out = NULL;
  const char *value = packed ? packed : "";
  while (*value && isspace((unsigned char)*value))
    value++;
  size_t len = strlen(value);
  while (len > 0 && isspace((unsigned char)value[len - 1]))
    len--;
  if (len == 0 || value[0] != '{')
    return;

  char *copy = (char *)malloc(len + 1);
  if (!copy)
    return;
  memcpy(copy, value, len);
  copy[len] = '\0';
  cJSON *root = cJSON_Parse(copy);
  free(copy);
  if (!root)
    return;

  *access_token_out =
      ockito_pick_str(root, (const char *[]){"access_token"}, 1);
  *refresh_token_out =
      ockito_pick_str(root, (const char *[]){"refresh_token"}, 1);
  cJSON_Delete(root);
  if (!*access_token_out)
    *access_token_out = tm_strdup("");
  if (!*refresh_token_out)
    *refresh_token_out = tm_strdup("");
}

static char *ockito_refresh_access_token(const char *refresh_token) {
  const char *headers[] = {"Accept: application/json",
                           "Content-Type: application/json", NULL};
  char auth[256];
  snprintf(auth, sizeof(auth), "Authorization: Bearer %s",
           refresh_token ? refresh_token : "");
  const char *req_headers[] = {headers[0], headers[1], auth, "X-PASSTHROUGH: Y",
                               NULL};
  long status = 0;
  cJSON *root =
      ockito_request_json("POST", "/grefresh", req_headers, "{}", &status);
  if (!root)
    return NULL;
  if (status < 200 || status >= 300) {
    cJSON_Delete(root);
    return NULL;
  }
  char *access_token =
      ockito_pick_str(root, (const char *[]){"access_token"}, 1);
  cJSON_Delete(root);
  return access_token;
}

static cJSON *ockito_fetch_bearer_json(const char *path, char **access_token,
                                       const char *refresh_token) {
  char auth[512];
  snprintf(auth, sizeof(auth), "Authorization: Bearer %s",
           *access_token ? *access_token : "");
  const char *req_headers[] = {"Accept: application/json", auth, NULL};
  long status = 0;
  cJSON *root = ockito_request_json("GET", path, req_headers, NULL, &status);
  if (!root)
    return NULL;
  if (status == 401) {
    cJSON_Delete(root);
    char *refreshed = ockito_refresh_access_token(refresh_token);
    if (!refreshed)
      return NULL;
    free(*access_token);
    *access_token = refreshed;
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", *access_token);
    const char *retry_headers[] = {"Accept: application/json", auth, NULL};
    root = ockito_request_json("GET", path, retry_headers, NULL, &status);
    if (!root)
      return NULL;
  }
  if (status < 200 || status >= 300) {
    cJSON_Delete(root);
    return NULL;
  }
  return root;
}

static cJSON *ockito_flatten_inbox_row(const cJSON *raw,
                                       const char *recipient) {
  cJSON *flat = cJSON_CreateObject();
  if (!flat)
    return NULL;
  ockito_add_picked_str(flat, "id", raw, (const char *[]){"uid"}, 1);
  ockito_add_picked_str(flat, "from", raw, (const char *[]){"sender"}, 1);
  cJSON_AddStringToObject(flat, "to", recipient ? recipient : "");
  ockito_add_picked_str(flat, "subject", raw, (const char *[]){"subject"}, 1);
  ockito_add_picked_str(flat, "text", raw, (const char *[]){"snippet"}, 1);
  ockito_add_picked_str(flat, "html", raw, (const char *[]){"html"}, 1);
  cJSON_AddItemToObject(
      flat, "date",
      cJSON_Duplicate(
          cJSON_GetObjectItemCaseSensitive((cJSON *)raw, "timestamp"), 1));
  cJSON_AddBoolToObject(flat, "is_read", false);
  cJSON_AddItemToObject(flat, "attachments", cJSON_CreateArray());
  return flat;
}

static cJSON *ockito_flatten_message(const cJSON *raw, const char *recipient) {
  const cJSON *obj = cJSON_GetObjectItemCaseSensitive((cJSON *)raw, "obj");
  if (!cJSON_IsObject(obj))
    obj = raw;
  cJSON *flat = cJSON_CreateObject();
  if (!flat)
    return NULL;
  ockito_add_picked_str(flat, "id", raw, (const char *[]){"uid"}, 1);
  char *from = ockito_pick_str(obj,
                               (const char *[]){"sender_email", "SenderEmail",
                                                "from_", "From", "from",
                                                "sender_name", "SenderName"},
                               7);
  char *to = ockito_pick_str(obj, (const char *[]){"to", "To"}, 2);
  cJSON_AddStringToObject(flat, "from", from ? from : "");
  cJSON_AddStringToObject(flat, "to",
                          (to && to[0]) ? to : (recipient ? recipient : ""));
  ockito_add_picked_str(flat, "subject", obj,
                        (const char *[]){"subject", "Subject"}, 2);
  ockito_add_picked_str(flat, "text", obj, (const char *[]){"text"}, 1);
  ockito_add_picked_str(flat, "html", obj, (const char *[]){"html"}, 1);
  cJSON_AddItemToObject(
      flat, "date",
      cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive((cJSON *)obj, "date"),
                      1));
  cJSON_AddBoolToObject(flat, "is_read", false);
  cJSON_AddItemToObject(flat, "attachments", cJSON_CreateArray());
  free(from);
  free(to);
  return flat;
}

tm_email_info_t *tm_provider_ockito_generate(void) {
  long status = 0;
  cJSON *login = ockito_request_json("POST", "/gtoken",
                                     ockito_post_json_headers, "{}", &status);
  if (!login)
    return NULL;
  if (status < 200 || status >= 300) {
    cJSON_Delete(login);
    return NULL;
  }

  char *access_token =
      ockito_pick_str(login, (const char *[]){"access_token"}, 1);
  char *refresh_token =
      ockito_pick_str(login, (const char *[]){"refresh_token"}, 1);
  if (!access_token || !access_token[0] || !refresh_token ||
      !refresh_token[0]) {
    free(access_token);
    free(refresh_token);
    cJSON_Delete(login);
    return NULL;
  }

  char auth[512];
  snprintf(auth, sizeof(auth), "Authorization: Bearer %s", access_token);
  const char *email_headers[] = {"Accept: application/json", auth, NULL};
  status = 0;
  cJSON *email_json =
      ockito_request_json("GET", "/email", email_headers, NULL, &status);
  if (!email_json) {
    free(access_token);
    free(refresh_token);
    cJSON_Delete(login);
    return NULL;
  }
  if (status < 200 || status >= 300) {
    free(access_token);
    free(refresh_token);
    cJSON_Delete(login);
    cJSON_Delete(email_json);
    return NULL;
  }

  char *email = NULL;
  cJSON *email_value = cJSON_GetObjectItemCaseSensitive(email_json, "email");
  if (cJSON_IsString(email_value) && email_value->valuestring) {
    email = ockito_trim_copy(email_value->valuestring);
  } else if (cJSON_IsObject(email_value)) {
    email = ockito_pick_str(email_value, (const char *[]){"email"}, 1);
  }
  if (!email || !strchr(email, '@')) {
    free(access_token);
    free(refresh_token);
    free(email);
    cJSON_Delete(login);
    cJSON_Delete(email_json);
    return NULL;
  }

  char *packed = ockito_pack_token(access_token, refresh_token);
  tm_email_info_t *info = tm_email_info_new();
  if (!info || !packed) {
    if (info)
      tm_free_email_info(info);
    free(access_token);
    free(refresh_token);
    free(email);
    free(packed);
    cJSON_Delete(login);
    cJSON_Delete(email_json);
    return NULL;
  }
  info->channel = CHANNEL_OCKITO;
  info->email = tm_strdup(email);
  info->token = packed;

  free(access_token);
  free(refresh_token);
  free(email);
  cJSON_Delete(login);
  cJSON_Delete(email_json);
  return info;
}

tm_email_t *tm_provider_ockito_get_emails(const char *token, const char *email,
                                          int *count) {
  *count = -1;
  char *access_token = NULL;
  char *refresh_token = NULL;
  ockito_unpack_token(token, &access_token, &refresh_token);
  if (!access_token || !access_token[0] || !refresh_token ||
      !refresh_token[0]) {
    free(access_token);
    free(refresh_token);
    return NULL;
  }
  if (!email || !email[0]) {
    free(access_token);
    free(refresh_token);
    return NULL;
  }

  char *enc_email = ockito_urlencode(email);
  if (!enc_email) {
    free(access_token);
    free(refresh_token);
    return NULL;
  }

  char path[1024];
  snprintf(path, sizeof(path), "/retrieve/%s/emails", enc_email);
  free(enc_email);

  cJSON *json = ockito_fetch_bearer_json(path, &access_token, refresh_token);
  if (!json) {
    free(access_token);
    free(refresh_token);
    return NULL;
  }

  cJSON *inbox = cJSON_GetObjectItemCaseSensitive(json, "inbox");
  int n = cJSON_IsArray(inbox) ? cJSON_GetArraySize(inbox) : 0;
  *count = n;
  if (n == 0) {
    cJSON_Delete(json);
    free(access_token);
    free(refresh_token);
    return NULL;
  }

  tm_email_t *out = tm_emails_new(n);
  if (!out) {
    cJSON_Delete(json);
    free(access_token);
    free(refresh_token);
    return NULL;
  }

  for (int i = 0; i < n; i++) {
    cJSON *row = cJSON_GetArrayItem(inbox, i);
    cJSON *flat = NULL;
    char *uid = ockito_pick_str(row, (const char *[]){"uid"}, 1);
    if (uid && uid[0]) {
      char *enc_uid = ockito_urlencode(uid);
      if (enc_uid) {
        char detail_path[1024];
        snprintf(detail_path, sizeof(detail_path), "/retrieve/%s/%s", email,
                 enc_uid);
        cJSON *detail =
            ockito_fetch_bearer_json(detail_path, &access_token, refresh_token);
        if (detail) {
          flat = ockito_flatten_message(detail, email);
          cJSON_Delete(detail);
        }
        free(enc_uid);
      }
    }
    if (!flat) {
      flat = ockito_flatten_inbox_row(row, email);
    }
    if (!flat) {
      cJSON *empty = cJSON_CreateObject();
      out[i] = tm_normalize_email(empty, email);
      cJSON_Delete(empty);
    } else {
      out[i] = tm_normalize_email(flat, email);
      cJSON_Delete(flat);
    }
    free(uid);
  }

  cJSON_Delete(json);
  free(access_token);
  free(refresh_token);
  return out;
}
