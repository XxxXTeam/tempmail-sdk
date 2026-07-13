/**
 * mytempmail-cc 渠道
 *
 * 纯 JSON REST API
 * 域名: @nilvaro.com
 * 网站: mytempmail.cc
 *
 * 创建邮箱流程:
 *   1. POST https://api.mytempmail.cc/api/address
 *      body: {"domain":"nilvaro.com","name":"<random>","expiry":600}
 *   2. 解析响应提取邮箱地址和 token
 *
 * 获取邮件流程:
 *   1. GET https://api.mytempmail.cc/api/mails/<token>
 *   2. 解析响应中的邮件数组
 *
 * token 存储策略:
 *   直接存储 API 返回的 token 字符串
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MTCC_BASE "https://api.mytempmail.cc"
#define MTCC_MAX_MAILS 128

/* 公共 UA */
static const char *mtcc_ua =
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36";

/**
 * 生成随机用户名（小写字母 + 数字，10 位）
 */
static void mtcc_random_name(char *out, size_t cap) {
  static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
  int len = 10;
  if ((size_t)len >= cap)
    len = (int)(cap - 1);
  for (int i = 0; i < len; i++) {
    out[i] = chars[rand() % (sizeof(chars) - 1)];
  }
  out[len] = '\0';
}

/* ========== 创建邮箱 ========== */

/**
 * 创建 mytempmail-cc 临时邮箱
 *
 * 流程:
 *   1. 生成随机用户名
 *   2. POST /api/address -> 获取邮箱地址和 token
 */
tm_email_info_t *tm_provider_mytempmail_cc_generate(void) {
  int timeout =
      tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

  /* 生成随机用户名 */
  char name[32];
  mtcc_random_name(name, sizeof(name));

  /* 构建 POST body */
  cJSON *body_obj = cJSON_CreateObject();
  if (!body_obj)
    return NULL;
  cJSON_AddStringToObject(body_obj, "domain", "nilvaro.com");
  cJSON_AddStringToObject(body_obj, "name", name);
  cJSON_AddNumberToObject(body_obj, "expiry", 600);
  char *body = cJSON_PrintUnformatted(body_obj);
  cJSON_Delete(body_obj);
  if (!body)
    return NULL;

  /* 构建请求头 */
  const char *hdrs[] = {mtcc_ua, "Accept: application/json",
                        "Content-Type: application/json", NULL};

  tm_http_response_t *resp = tm_http_request(
      TM_HTTP_POST, MTCC_BASE "/api/address", hdrs, body, timeout);
  free(body);

  if (!resp || resp->status != 200 || !resp->body) {
    TM_LOG_ERR("[mytempmail-cc] 创建邮箱请求失败, status=%ld",
               resp ? resp->status : 0);
    tm_http_response_free(resp);
    return NULL;
  }

  /* 解析响应 JSON */
  cJSON *json = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!json) {
    TM_LOG_ERR("[mytempmail-cc] 解析创建邮箱响应 JSON 失败");
    return NULL;
  }

  /* 提取邮箱地址 */
  const cJSON *j_email = cJSON_GetObjectItemCaseSensitive(json, "email");
  if (!j_email)
    j_email = cJSON_GetObjectItemCaseSensitive(json, "address");
  if (!cJSON_IsString(j_email) || !j_email->valuestring ||
      !j_email->valuestring[0]) {
    /* 尝试拼接 name@domain */
    const cJSON *j_name = cJSON_GetObjectItemCaseSensitive(json, "name");
    const cJSON *j_domain = cJSON_GetObjectItemCaseSensitive(json, "domain");
    if (cJSON_IsString(j_name) && j_name->valuestring &&
        j_name->valuestring[0] && cJSON_IsString(j_domain) &&
        j_domain->valuestring && j_domain->valuestring[0]) {
      /* 使用返回的 name 和 domain 拼接 */
    } else {
      TM_LOG_ERR("[mytempmail-cc] 响应中缺少邮箱地址字段");
      cJSON_Delete(json);
      return NULL;
    }
  }

  char email_buf[256];
  if (cJSON_IsString(j_email) && j_email->valuestring &&
      j_email->valuestring[0]) {
    snprintf(email_buf, sizeof(email_buf), "%s", j_email->valuestring);
  } else {
    const cJSON *j_name = cJSON_GetObjectItemCaseSensitive(json, "name");
    const cJSON *j_domain = cJSON_GetObjectItemCaseSensitive(json, "domain");
    snprintf(email_buf, sizeof(email_buf), "%s@%s", j_name->valuestring,
             j_domain->valuestring);
  }

  /* 提取 token */
  const cJSON *j_token = cJSON_GetObjectItemCaseSensitive(json, "token");
  if (!cJSON_IsString(j_token) || !j_token->valuestring ||
      !j_token->valuestring[0]) {
    TM_LOG_ERR("[mytempmail-cc] 响应中缺少 token 字段");
    cJSON_Delete(json);
    return NULL;
  }

  /* 构建结果 */
  tm_email_info_t *info = tm_email_info_new();
  if (!info) {
    cJSON_Delete(json);
    return NULL;
  }

  info->channel = CHANNEL_MYTEMPMAIL_CC;
  info->email = tm_strdup(email_buf);
  info->token = tm_strdup(j_token->valuestring);
  info->expires_at = 0;
  info->created_at = tm_strdup("");

  cJSON_Delete(json);

  TM_LOG_DBG("[mytempmail-cc] 创建邮箱成功: %s", info->email);
  return info;
}

/* ========== 获取邮件 ========== */

/**
 * 获取 mytempmail-cc 邮件列表
 *
 * 流程:
 *   1. GET /api/mails/<token> -> JSON 数组
 *
 * @param token  API 返回的 token
 * @param email  完整邮箱地址
 * @param count  输出邮件数量，-1 表示请求失败
 */
tm_email_t *tm_provider_mytempmail_cc_get_emails(const char *token,
                                                 const char *email,
                                                 int *count) {
  *count = -1;
  if (!token || !email)
    return NULL;

  int timeout =
      tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

  /* 构建请求 URL */
  char url[512];
  snprintf(url, sizeof(url), MTCC_BASE "/api/mails/%s", token);

  const char *hdrs[] = {mtcc_ua, "Accept: application/json", NULL};

  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, hdrs, NULL, timeout);
  if (!resp || resp->status != 200 || !resp->body) {
    TM_LOG_ERR("[mytempmail-cc] 获取邮件请求失败, status=%ld",
               resp ? resp->status : 0);
    tm_http_response_free(resp);
    return NULL;
  }

  /* 解析 JSON */
  cJSON *json = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!json) {
    TM_LOG_ERR("[mytempmail-cc] 解析邮件列表 JSON 失败");
    return NULL;
  }

  /* 响应可能是数组或包含数组的对象 */
  cJSON *arr = json;
  if (!cJSON_IsArray(json)) {
    arr = cJSON_GetObjectItemCaseSensitive(json, "mails");
    if (!arr)
      arr = cJSON_GetObjectItemCaseSensitive(json, "emails");
    if (!arr)
      arr = cJSON_GetObjectItemCaseSensitive(json, "data");
    if (!arr || !cJSON_IsArray(arr)) {
      TM_LOG_ERR("[mytempmail-cc] 响应中未找到邮件数组");
      cJSON_Delete(json);
      return NULL;
    }
  }

  int msg_count = cJSON_GetArraySize(arr);
  if (msg_count <= 0) {
    cJSON_Delete(json);
    *count = 0;
    return NULL;
  }

  if (msg_count > MTCC_MAX_MAILS)
    msg_count = MTCC_MAX_MAILS;

  tm_email_t *emails = tm_emails_new(msg_count);
  if (!emails) {
    cJSON_Delete(json);
    return NULL;
  }

  int valid = 0;
  for (int i = 0; i < msg_count; i++) {
    const cJSON *msg = cJSON_GetArrayItem(arr, i);
    if (!cJSON_IsObject(msg))
      continue;

    /* 构建标准化用的 cJSON 对象 */
    cJSON *raw = cJSON_CreateObject();
    if (!raw)
      continue;

    /* 提取字段并映射到标准化格式 */
    const char *id_keys[] = {"id", "_id", "mail_id"};
    char *id_str = tm_json_get_str(msg, id_keys, 3);
    cJSON_AddStringToObject(raw, "id", id_str);
    free(id_str);

    const char *from_keys[] = {"from", "from_address", "sender", "mail_from"};
    char *from_str = tm_json_get_str(msg, from_keys, 4);
    cJSON_AddStringToObject(raw, "from", from_str);
    free(from_str);

    cJSON_AddStringToObject(raw, "to", email);

    const char *subj_keys[] = {"subject", "mail_subject", "title"};
    char *subj_str = tm_json_get_str(msg, subj_keys, 3);
    cJSON_AddStringToObject(raw, "subject", subj_str);
    free(subj_str);

    const char *text_keys[] = {"text", "body_text", "text_body", "body"};
    char *text_str = tm_json_get_str(msg, text_keys, 4);
    cJSON_AddStringToObject(raw, "text", text_str);
    free(text_str);

    const char *html_keys[] = {"html", "body_html", "html_body", "body"};
    char *html_str = tm_json_get_str(msg, html_keys, 4);
    cJSON_AddStringToObject(raw, "html", html_str);
    free(html_str);

    const char *date_keys[] = {"date", "created_at", "receivedAt", "timestamp"};
    char *date_str = tm_json_get_str(msg, date_keys, 4);
    cJSON_AddStringToObject(raw, "date", date_str);
    free(date_str);

    emails[valid] = tm_normalize_email(raw, email);
    cJSON_Delete(raw);
    valid++;
  }

  cJSON_Delete(json);

  if (valid == 0) {
    free(emails);
    *count = 0;
    return NULL;
  }

  *count = valid;
  return emails;
}
