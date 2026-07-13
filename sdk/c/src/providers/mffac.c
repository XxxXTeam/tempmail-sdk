/**
 * MFFAC — https://www.mffac.com/api
 */
#include "tempmail_internal.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MF_BASE "https://www.mffac.com/api"

static const char *mf_post_headers[] = {
    "Accept: */*",
    "Content-Type: application/json",
    "Origin: https://www.mffac.com",
    "Referer: https://www.mffac.com/",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    "sec-ch-ua: \"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft "
    "Edge\";v=\"146\"",
    "sec-ch-ua-mobile: ?0",
    "sec-ch-ua-platform: \"Windows\"",
    "sec-fetch-dest: empty",
    "sec-fetch-mode: cors",
    "sec-fetch-site: same-origin",
    NULL};

static const char *mf_get_headers[] = {
    "Accept: */*",
    "Origin: https://www.mffac.com",
    "Referer: https://www.mffac.com/",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    "sec-ch-ua: \"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft "
    "Edge\";v=\"146\"",
    "sec-ch-ua-mobile: ?0",
    "sec-ch-ua-platform: \"Windows\"",
    "sec-fetch-dest: empty",
    "sec-fetch-mode: cors",
    "sec-fetch-site: same-origin",
    NULL};

static char *mf_escape(const char *s) {
  if (!s)
    return tm_strdup("");
  CURL *c = curl_easy_init();
  if (!c)
    return tm_strdup(s);
  char *e = curl_easy_escape(c, s, 0);
  curl_easy_cleanup(c);
  if (!e)
    return tm_strdup(s);
  char *d = tm_strdup(e);
  curl_free(e);
  return d;
}

static void mf_local_part(const char *email, char *out, size_t cap) {
  if (!email || cap < 2) {
    if (cap)
      out[0] = '\0';
    return;
  }
  const char *at = strchr(email, '@');
  if (at && at > email) {
    size_t n = (size_t)(at - email);
    if (n >= cap)
      n = cap - 1;
    memcpy(out, email, n);
    out[n] = '\0';
  } else {
    strncpy(out, email, cap - 1);
    out[cap - 1] = '\0';
  }
}

static char *mf_received_at_to_iso(const cJSON *item) {
  if (!cJSON_IsNumber(item) || item->valuedouble <= 0)
    return tm_strdup("");
  time_t sec = (time_t)(item->valuedouble + 0.5);
  char buf[48];
  struct tm tmu;
#ifdef _WIN32
  if (gmtime_s(&tmu, &sec) != 0)
    memset(&tmu, 0, sizeof(tmu));
#else
  gmtime_r(&sec, &tmu);
#endif
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmu);
  return tm_strdup(buf);
}

static cJSON *mf_fetch_detail(const char *id) {
  if (!id || !id[0])
    return NULL;
  char *esc = mf_escape(id);
  if (!esc)
    return NULL;

  char url[640];
  snprintf(url, sizeof(url), "%s/emails/%s", MF_BASE, esc);
  free(esc);

  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, mf_get_headers, NULL, 15);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }

  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!root)
    return NULL;

  const cJSON *ok = cJSON_GetObjectItemCaseSensitive(root, "success");
  cJSON *email = cJSON_GetObjectItemCaseSensitive(root, "email");
  if (!cJSON_IsBool(ok) || !cJSON_IsTrue(ok) || !cJSON_IsObject(email)) {
    cJSON_Delete(root);
    return NULL;
  }

  cJSON *copy = cJSON_Duplicate(email, 1);
  cJSON_Delete(root);
  return copy;
}

static cJSON *mf_flatten_email(const cJSON *raw, const char *recipient) {
  cJSON *flat = cJSON_CreateObject();
  if (!flat)
    return NULL;

  cJSON *id = cJSON_GetObjectItemCaseSensitive((cJSON *)raw, "id");
  cJSON *from = cJSON_GetObjectItemCaseSensitive((cJSON *)raw, "fromAddress");
  cJSON *to = cJSON_GetObjectItemCaseSensitive((cJSON *)raw, "toAddress");
  cJSON *subject = cJSON_GetObjectItemCaseSensitive((cJSON *)raw, "subject");
  cJSON *text = cJSON_GetObjectItemCaseSensitive((cJSON *)raw, "textContent");
  cJSON *html = cJSON_GetObjectItemCaseSensitive((cJSON *)raw, "htmlContent");
  cJSON *received_at =
      cJSON_GetObjectItemCaseSensitive((cJSON *)raw, "receivedAt");
  cJSON *is_read = cJSON_GetObjectItemCaseSensitive((cJSON *)raw, "isRead");

  char *date = mf_received_at_to_iso(received_at);
  cJSON_AddStringToObject(flat, "id", TM_JSON_STR(id, ""));
  cJSON_AddStringToObject(flat, "from", TM_JSON_STR(from, ""));
  cJSON_AddStringToObject(flat, "to", TM_JSON_STR(to, recipient));
  cJSON_AddStringToObject(flat, "subject", TM_JSON_STR(subject, ""));
  cJSON_AddStringToObject(flat, "text", TM_JSON_STR(text, ""));
  cJSON_AddStringToObject(flat, "html", TM_JSON_STR(html, ""));
  cJSON_AddStringToObject(flat, "date", date ? date : "");
  cJSON_AddBoolToObject(flat, "isRead",
                        cJSON_IsBool(is_read) ? cJSON_IsTrue(is_read) : 0);
  cJSON_AddItemToObject(flat, "attachments", cJSON_CreateArray());
  free(date);
  return flat;
}

tm_email_info_t *tm_provider_mffac_generate(void) {
  const char *body = "{\"expiresInHours\":24}";
  tm_http_response_t *resp = tm_http_request(TM_HTTP_POST, MF_BASE "/mailboxes",
                                             mf_post_headers, body, 15);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }

  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!root)
    return NULL;

  const cJSON *ok = cJSON_GetObjectItemCaseSensitive(root, "success");
  if (!cJSON_IsBool(ok) || !cJSON_IsTrue(ok)) {
    cJSON_Delete(root);
    return NULL;
  }

  cJSON *mb = cJSON_GetObjectItemCaseSensitive(root, "mailbox");
  if (!cJSON_IsObject(mb)) {
    cJSON_Delete(root);
    return NULL;
  }

  const char *addr =
      cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(mb, "address"));
  const char *mid =
      cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(mb, "id"));
  if (!addr || !addr[0] || !mid || !mid[0]) {
    cJSON_Delete(root);
    return NULL;
  }

  char email[320];
  snprintf(email, sizeof(email), "%s@mffac.com", addr);

  tm_email_info_t *info = tm_email_info_new();
  info->channel = CHANNEL_MFFAC;
  info->email = tm_strdup(email);
  info->token = tm_strdup(mid);

  const cJSON *ca = cJSON_GetObjectItemCaseSensitive(mb, "createdAt");
  if (cJSON_IsString(ca) && ca->valuestring)
    info->created_at = tm_strdup(ca->valuestring);

  cJSON_Delete(root);
  return info;
}

tm_email_t *tm_provider_mffac_get_emails(const char *token, const char *email,
                                         int *count) {
  (void)token;
  *count = -1;
  if (!email || !email[0])
    return NULL;

  char local[256];
  mf_local_part(email, local, sizeof(local));
  if (!local[0])
    return NULL;

  char *esc = mf_escape(local);
  char url[640];
  snprintf(url, sizeof(url), "%s/mailboxes/%s/emails", MF_BASE, esc);
  free(esc);

  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, mf_get_headers, NULL, 15);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }

  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!root)
    return NULL;

  const cJSON *ok = cJSON_GetObjectItemCaseSensitive(root, "success");
  if (!cJSON_IsBool(ok) || !cJSON_IsTrue(ok)) {
    cJSON_Delete(root);
    return NULL;
  }

  cJSON *arr = cJSON_GetObjectItemCaseSensitive(root, "emails");
  int n = (arr && cJSON_IsArray(arr)) ? cJSON_GetArraySize(arr) : 0;
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
    cJSON *raw = cJSON_GetArrayItem(arr, i);
    const char *id =
        TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(raw, "id"), "");
    cJSON *detail = id[0] ? mf_fetch_detail(id) : NULL;
    cJSON *flat = mf_flatten_email(detail ? detail : raw, email);
    emails[i] = tm_normalize_email(flat ? flat : raw, email);
    if (flat)
      cJSON_Delete(flat);
    if (detail)
      cJSON_Delete(detail);
  }
  cJSON_Delete(root);
  return emails;
}
