/**
 * Maildrop — https://maildrop.cx
 */
#include "tempmail_internal.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define MD_STRICMP _stricmp
#else
#include <strings.h>
#define MD_STRICMP strcasecmp
#endif

#define MD_BASE "https://maildrop.cx"
#define MD_EXCLUDED "transformer.edu.kg"

static const char *md_headers[] = {
    "Accept: application/json, text/plain, */*",
    "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
    "Cache-Control: no-cache",
    "DNT: 1",
    "Pragma: no-cache",
    "Referer: https://maildrop.cx/zh-cn/app",
    "sec-ch-ua: \"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft "
    "Edge\";v=\"146\"",
    "sec-ch-ua-mobile: ?0",
    "sec-ch-ua-platform: \"Windows\"",
    "sec-fetch-dest: empty",
    "sec-fetch-mode: cors",
    "sec-fetch-site: same-origin",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    "x-requested-with: XMLHttpRequest",
    NULL};

static void md_random_local(char *buf, int len) {
  const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
  srand((unsigned)time(NULL));
  for (int i = 0; i < len; i++)
    buf[i] = chars[rand() % (int)(sizeof(chars) - 1)];
  buf[len] = '\0';
}

static char *md_curl_escape(const char *s) {
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

static void md_pick_domain(char domains[][256], int nd, const char *preferred,
                           char *out, size_t cap) {
  if (!out || cap < 2)
    return;
  out[0] = '\0';
  if (nd <= 0)
    return;
  if (preferred && preferred[0]) {
    for (int i = 0; i < nd; i++) {
      if (MD_STRICMP(domains[i], preferred) == 0) {
        strncpy(out, domains[i], cap - 1);
        out[cap - 1] = '\0';
        return;
      }
    }
    return;
  }
  int pick = rand() % nd;
  strncpy(out, domains[pick], cap - 1);
  out[cap - 1] = '\0';
}

tm_email_info_t *tm_provider_maildrop_generate(const char *domain) {
  srand((unsigned)time(NULL));
  tm_http_response_t *resp = tm_http_request(
      TM_HTTP_GET, MD_BASE "/api/suffixes.php", md_headers, NULL, 15);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }

  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!root || !cJSON_IsArray(root)) {
    cJSON_Delete(root);
    return NULL;
  }

  char domains[64][256];
  int nd = 0;
  int n = cJSON_GetArraySize(root);
  for (int i = 0; i < n && nd < 64; i++) {
    cJSON *it = cJSON_GetArrayItem(root, i);
    const char *s = cJSON_GetStringValue(it);
    if (s && s[0] && MD_STRICMP(s, MD_EXCLUDED) != 0) {
      strncpy(domains[nd], s, sizeof(domains[0]) - 1);
      domains[nd][sizeof(domains[0]) - 1] = '\0';
      nd++;
    }
  }
  cJSON_Delete(root);
  if (nd == 0)
    return NULL;

  char pickdom[256];
  md_pick_domain(domains, nd, domain, pickdom, sizeof(pickdom));
  if (!pickdom[0])
    return NULL;

  char local[16];
  md_random_local(local, 10);

  char email[320];
  snprintf(email, sizeof(email), "%s@%s", local, pickdom);

  tm_email_info_t *info = tm_email_info_new();
  info->channel = CHANNEL_MAILDROP;
  info->email = tm_strdup(email);
  info->token = tm_strdup(email);
  return info;
}

/**
 * 通过详情接口获取单封邮件完整内容
 * GET /api/email_content.php?id={id}
 * 详情响应字段（从前端代码确认）:
 *   - content: 完整 HTML 正文
 *   - subject / from_addr / date: 邮件元数据
 *   - attachment: JSON 字符串数组 [{filename, path, size}]（可能为空）
 * 返回解析后的 cJSON 对象（调用方负责释放），失败返回 NULL
 */
static cJSON *md_fetch_detail(const char *id) {
  if (!id || !id[0]) return NULL;
  char *esc = md_curl_escape(id);
  char url[800];
  snprintf(url, sizeof(url), "%s/api/email_content.php?id=%s", MD_BASE, esc);
  free(esc);
  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, md_headers, NULL, 15);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }
  cJSON *detail = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!detail || !cJSON_IsObject(detail)) {
    cJSON_Delete(detail);
    return NULL;
  }
  return detail;
}

/**
 * 将详情接口返回的 content 字段映射为 html 字段
 * 详情的其他字段（subject/from_addr/date）优先级更高
 */
static void md_merge_detail_into_raw(cJSON *raw, cJSON *detail) {
  if (!raw || !detail) return;
  /* content -> html */
  cJSON *content = cJSON_GetObjectItemCaseSensitive(detail, "content");
  if (cJSON_IsString(content) && content->valuestring && content->valuestring[0]) {
    cJSON_DeleteItemFromObjectCaseSensitive(raw, "html");
    cJSON_AddStringToObject(raw, "html", content->valuestring);
  }
  /* from_addr / subject / date 使用详情覆盖 */
  for (int i = 0; i < 3; i++) {
    const char *keys[] = {"from_addr", "subject", "date"};
    cJSON *item = cJSON_GetObjectItemCaseSensitive(detail, keys[i]);
    if (cJSON_IsString(item) && item->valuestring && item->valuestring[0]) {
      cJSON_DeleteItemFromObjectCaseSensitive(raw, keys[i]);
      cJSON_AddStringToObject(raw, keys[i], item->valuestring);
    }
  }
  /* attachment（JSON字符串）解析为附件数组 */
  cJSON *att = cJSON_GetObjectItemCaseSensitive(detail, "attachment");
  if (cJSON_IsString(att) && att->valuestring && att->valuestring[0]) {
    cJSON *att_arr = cJSON_Parse(att->valuestring);
    if (att_arr && cJSON_IsArray(att_arr)) {
      cJSON_DeleteItemFromObjectCaseSensitive(raw, "attachments");
      cJSON_AddItemToObject(raw, "attachments", att_arr);
    } else {
      cJSON_Delete(att_arr);
    }
  }
}

tm_email_t *tm_provider_maildrop_get_emails(const char *token,
                                            const char *email, int *count) {
  *count = -1;
  const char *addr = (email && email[0]) ? email : token;
  if (!addr || !addr[0])
    return NULL;

  char *esc = md_curl_escape(addr);
  char url[800];
  snprintf(url, sizeof(url), "%s/api/emails.php?addr=%s&page=1&limit=20",
           MD_BASE, esc);
  free(esc);

  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, md_headers, NULL, 15);
  if (!resp || resp->status < 200 || resp->status >= 300) {
    tm_http_response_free(resp);
    return NULL;
  }

  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!root)
    return NULL;

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

    /* 提取邮件 ID 用于详情二拉 */
    char id_buf[64] = {0};
    cJSON *id_item = cJSON_GetObjectItemCaseSensitive(raw, "id");
    if (cJSON_IsString(id_item) && id_item->valuestring) {
      strncpy(id_buf, id_item->valuestring, sizeof(id_buf) - 1);
    } else if (cJSON_IsNumber(id_item)) {
      snprintf(id_buf, sizeof(id_buf), "%.0f", id_item->valuedouble);
    }

    /* 无条件调用详情接口，用 content/from_addr/subject/date/attachment 覆盖列表字段 */
    if (id_buf[0]) {
      cJSON *detail = md_fetch_detail(id_buf);
      if (detail) {
        md_merge_detail_into_raw(raw, detail);
        cJSON_Delete(detail);
      }
    }

    emails[i] = tm_normalize_email(raw, addr);
  }
  cJSON_Delete(root);
  return emails;
}
