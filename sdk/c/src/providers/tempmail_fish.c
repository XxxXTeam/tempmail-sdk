/**
 * tempmail-fish — https://tempmail.fish
 *
 * 生成: POST /emails/new-email → {"email":"xxx","authKey":"hex","emails":[]}
 * 获取: GET /emails/emails?emailAddress={email}  Authorization: {authKey}
 * token 存储: JSON {"email":"...","authKey":"..."}
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEMPMAIL_FISH_BASE "https://api.tempmail.fish"

static const char *tmfish_post_headers[] = {
    "Accept: application/json", "Content-Type: application/json", NULL};

static const char *tmfish_get_headers_base[] = {"Accept: application/json",
                                                NULL};

static char *tmfish_pack_token(const char *email, const char *auth_key) {
  cJSON *o = cJSON_CreateObject();
  if (!o)
    return NULL;
  cJSON_AddStringToObject(o, "email", email);
  cJSON_AddStringToObject(o, "authKey", auth_key);
  char *s = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return s;
}

static int tmfish_unpack_token(const char *token, char **email_out,
                               char **auth_key_out) {
  *email_out = NULL;
  *auth_key_out = NULL;
  if (!token || !token[0])
    return -1;
  cJSON *j = cJSON_Parse(token);
  if (!j)
    return -1;
  const char *e =
      cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(j, "email"));
  const char *a =
      cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(j, "authKey"));
  if (!e || !e[0] || !a || !a[0]) {
    cJSON_Delete(j);
    return -1;
  }
  *email_out = tm_strdup(e);
  *auth_key_out = tm_strdup(a);
  cJSON_Delete(j);
  return 0;
}

tm_email_info_t *tm_provider_tempmail_fish_generate(void) {
  tm_http_response_t *resp = tm_http_request(
      TM_HTTP_POST, TEMPMAIL_FISH_BASE "/emails/new-email", tmfish_post_headers,
      NULL, 15);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }
  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!root)
    return NULL;

  const char *email =
      cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(root, "email"));
  const char *auth_key =
      cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(root, "authKey"));
  if (!email || !email[0] || !auth_key || !auth_key[0] || !strchr(email, '@')) {
    cJSON_Delete(root);
    return NULL;
  }

  char *packed = tmfish_pack_token(email, auth_key);
  cJSON_Delete(root);
  if (!packed)
    return NULL;

  tm_email_info_t *info = tm_email_info_new();
  info->channel = CHANNEL_TEMPMAIL_FISH;
  info->email = tm_strdup(email);
  info->token = packed;
  return info;
}

tm_email_t *tm_provider_tempmail_fish_get_emails(const char *token,
                                                 const char *email,
                                                 int *count) {
  *count = -1;
  char *addr = NULL;
  char *auth_key = NULL;
  if (tmfish_unpack_token(token, &addr, &auth_key) != 0) {
    free(addr);
    free(auth_key);
    return NULL;
  }

  char url[512];
  char *enc = tm_strdup(addr);
  snprintf(url, sizeof(url),
           TEMPMAIL_FISH_BASE "/emails/emails?emailAddress=%s", enc);
  free(enc);

  char auth_hdr[256];
  snprintf(auth_hdr, sizeof(auth_hdr), "Authorization: %s", auth_key);
  const char *headers[] = {"Accept: application/json", auth_hdr, NULL};

  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, headers, NULL, 15);
  free(addr);
  free(auth_key);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }

  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!root)
    return NULL;

  cJSON *arr = cJSON_IsArray(root) ? root : NULL;
  if (!arr) {
    cJSON_Delete(root);
    *count = 0;
    return NULL;
  }

  int n = cJSON_GetArraySize(arr);
  *count = n;
  if (n == 0) {
    cJSON_Delete(root);
    return NULL;
  }

  tm_email_t *out = tm_emails_new(n);
  for (int i = 0; i < n; i++) {
    cJSON *item = cJSON_GetArrayItem(arr, i);
    /* 映射 textBody/htmlBody/timestamp 为标准字段 */
    cJSON *flat = cJSON_CreateObject();
    const char *from_val =
        cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(item, "from"));
    const char *to_val =
        cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(item, "to"));
    const char *subj =
        cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(item, "subject"));
    const char *text_body = cJSON_GetStringValue(
        cJSON_GetObjectItemCaseSensitive(item, "textBody"));
    const char *html_body = cJSON_GetStringValue(
        cJSON_GetObjectItemCaseSensitive(item, "htmlBody"));

    cJSON_AddStringToObject(flat, "from", from_val ? from_val : "");
    cJSON_AddStringToObject(flat, "to", to_val ? to_val : email);
    cJSON_AddStringToObject(flat, "subject", subj ? subj : "");
    cJSON_AddStringToObject(flat, "text", text_body ? text_body : "");
    cJSON_AddStringToObject(flat, "html", html_body ? html_body : "");

    cJSON *ts = cJSON_GetObjectItemCaseSensitive(item, "timestamp");
    if (cJSON_IsNumber(ts)) {
      cJSON_AddNumberToObject(flat, "date", ts->valuedouble);
    }

    out[i] = tm_normalize_email(flat, email);
    cJSON_Delete(flat);
  }

  cJSON_Delete(root);
  return out;
}
