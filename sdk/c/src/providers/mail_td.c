/**
 * mail-td 渠道
 *
 * 基于 SHA-256 Proof-of-Work 的临时邮箱服务
 * Base URL: https://api.mail.td/api
 *
 * 创建邮箱流程:
 *   1. GET /domains 获取可用域名，筛选 pro_only=false 的免费域名
 *   2. 随机生成 用户名@域名 与 password，auth_key = sha256_hex(password)
 *   3. 求解 PoW: SHA-256(address_lower + timestamp + nonce) 满足 difficulty
 * 个前导零位
 *   4. POST /accounts 携带 {address, auth_key, pow:{t,n,d}} 创建账户
 *      - 返回 status=retry 时按 required_difficulty 提升难度并携带 token 重试
 *      - 成功返回 {id, address, token(JWT)}
 *
 * 获取邮件流程:
 *   1. GET /accounts/{id}/messages?page=1，Header: Authorization: Bearer {jwt}
 *   2. 解析 messages 数组
 *
 * token 存储策略:
 *   存储 JSON {"jwt":"...","id":"..."}，与 Go SDK 一致
 */
#include "tempmail_internal.h"
#include <ctype.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAILTD_BASE "https://api.mail.td/api"
#define MAILTD_MAX_MAILS 128
#define MAILTD_INIT_DIFFICULTY 15
#define MAILTD_MAX_RETRY 4
#define MAILTD_MAX_NONCE 100000000

/* 公共 UA */
static const char *mailtd_ua =
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36";

/**
 * 生成随机小写字母+数字字符串
 */
static void mailtd_random_string(char *out, int len) {
  static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
  for (int i = 0; i < len; i++) {
    out[i] = chars[rand() % (int)(sizeof(chars) - 1)];
  }
  out[len] = '\0';
}

/**
 * 将字节数组转为小写十六进制字符串
 */
static void mailtd_hex(const unsigned char *data, size_t len, char *out) {
  static const char *hexchars = "0123456789abcdef";
  for (size_t i = 0; i < len; i++) {
    out[i * 2] = hexchars[(data[i] >> 4) & 0xF];
    out[i * 2 + 1] = hexchars[data[i] & 0xF];
  }
  out[len * 2] = '\0';
}

/**
 * 检查哈希是否满足 difficulty 个前导零位
 */
static int mailtd_check_leading_zeros(const unsigned char *hash,
                                      int difficulty) {
  int full_bytes = difficulty / 8;
  int remain_bits = difficulty % 8;
  for (int i = 0; i < full_bytes; i++) {
    if (hash[i] != 0)
      return 0;
  }
  if (remain_bits > 0 && full_bytes < 32) {
    unsigned char mask = (unsigned char)((255 << (8 - remain_bits)) & 255);
    if (hash[full_bytes] & mask)
      return 0;
  }
  return 1;
}

/**
 * 求解 Proof-of-Work
 * 输入拼接: address_lower + timestamp_str + nonce_str
 * 目标: SHA-256 哈希有 difficulty 个前导零位
 *
 * @param address_lower 小写去空格的邮箱地址
 * @param timestamp     时间戳（秒）
 * @param difficulty    难度（前导零位数）
 * @param nonce_out     输出 nonce 字符串（十进制），需足够大
 * @return 成功返回 1，超时返回 0
 */
static int mailtd_solve_pow(const char *address_lower, long long timestamp,
                            int difficulty, char *nonce_out) {
  /* base = address + timestamp */
  char base[512];
  int base_len =
      snprintf(base, sizeof(base), "%s%lld", address_lower, timestamp);
  if (base_len < 0 || (size_t)base_len >= sizeof(base))
    return 0;

  for (long long nonce = 0; nonce < MAILTD_MAX_NONCE; nonce++) {
    char input[560];
    int input_len = snprintf(input, sizeof(input), "%s%lld", base, nonce);
    if (input_len < 0)
      continue;

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char *)input, (size_t)input_len, hash);

    if (mailtd_check_leading_zeros(hash, difficulty)) {
      snprintf(nonce_out, 32, "%lld", nonce);
      return 1;
    }
  }
  return 0;
}

/* ========== 创建邮箱 ========== */

/**
 * 创建 mail-td 临时邮箱
 */
tm_email_info_t *tm_provider_mail_td_generate(void) {
  int timeout =
      tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

  const char *hdrs_get[] = {mailtd_ua, "Accept: application/json", NULL};

  /* 1. 获取可用域名 */
  tm_http_response_t *resp = tm_http_request(
      TM_HTTP_GET, MAILTD_BASE "/domains", hdrs_get, NULL, timeout);
  if (!resp || resp->status != 200 || !resp->body) {
    TM_LOG_ERR("[mail-td] 获取域名失败, status=%ld", resp ? resp->status : 0);
    tm_http_response_free(resp);
    return NULL;
  }

  cJSON *dom_json = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!dom_json) {
    TM_LOG_ERR("[mail-td] 解析域名响应失败");
    return NULL;
  }

  cJSON *dom_arr = cJSON_GetObjectItemCaseSensitive(dom_json, "domains");
  if (!cJSON_IsArray(dom_arr)) {
    TM_LOG_ERR("[mail-td] 域名响应缺少 domains 数组");
    cJSON_Delete(dom_json);
    return NULL;
  }

  /* 筛选免费域名（pro_only=false 且 domain 非空） */
  int dom_total = cJSON_GetArraySize(dom_arr);
  const char **free_domains =
      (const char **)calloc(dom_total > 0 ? dom_total : 1, sizeof(char *));
  if (!free_domains) {
    cJSON_Delete(dom_json);
    return NULL;
  }
  int free_count = 0;
  for (int i = 0; i < dom_total; i++) {
    const cJSON *d = cJSON_GetArrayItem(dom_arr, i);
    if (!cJSON_IsObject(d))
      continue;
    const cJSON *pro = cJSON_GetObjectItemCaseSensitive(d, "pro_only");
    if (cJSON_IsBool(pro) && cJSON_IsTrue(pro))
      continue;
    const cJSON *dm = cJSON_GetObjectItemCaseSensitive(d, "domain");
    if (cJSON_IsString(dm) && dm->valuestring && dm->valuestring[0]) {
      free_domains[free_count++] = dm->valuestring;
    }
  }
  if (free_count == 0) {
    TM_LOG_ERR("[mail-td] 无可用免费域名");
    free(free_domains);
    cJSON_Delete(dom_json);
    return NULL;
  }

  /* 2. 随机选择域名，生成地址与 auth_key */
  const char *domain = free_domains[rand() % free_count];
  char username[16];
  mailtd_random_string(username, 10);
  char address[256];
  snprintf(address, sizeof(address), "%s@%s", username, domain);

  char password[32];
  mailtd_random_string(password, 20);
  unsigned char pw_hash[SHA256_DIGEST_LENGTH];
  SHA256((const unsigned char *)password, strlen(password), pw_hash);
  char auth_key[SHA256_DIGEST_LENGTH * 2 + 1];
  mailtd_hex(pw_hash, SHA256_DIGEST_LENGTH, auth_key);

  free(free_domains);
  cJSON_Delete(dom_json);

  /* address 转小写并去除首尾空白（用于 PoW 输入） */
  char address_lower[256];
  {
    const char *p = address;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
      p++;
    size_t j = 0;
    for (; *p && j < sizeof(address_lower) - 1; p++) {
      address_lower[j++] = (char)tolower((unsigned char)*p);
    }
    while (j > 0 &&
           (address_lower[j - 1] == ' ' || address_lower[j - 1] == '\t' ||
            address_lower[j - 1] == '\n' || address_lower[j - 1] == '\r')) {
      j--;
    }
    address_lower[j] = '\0';
  }

  int difficulty = MAILTD_INIT_DIFFICULTY;
  char pow_token[512];
  pow_token[0] = '\0';

  const char *hdrs_post[] = {mailtd_ua, "Accept: application/json",
                             "Content-Type: application/json", NULL};

  /* 3. PoW 重试循环 */
  for (int retry = 0; retry < MAILTD_MAX_RETRY; retry++) {
    long long timestamp = (long long)time(NULL);
    char nonce[32];
    if (!mailtd_solve_pow(address_lower, timestamp, difficulty, nonce)) {
      TM_LOG_ERR("[mail-td] PoW 求解超时, difficulty=%d", difficulty);
      return NULL;
    }

    /* 构建请求体 */
    cJSON *body_obj = cJSON_CreateObject();
    if (!body_obj)
      return NULL;
    cJSON_AddStringToObject(body_obj, "address", address);
    cJSON_AddStringToObject(body_obj, "auth_key", auth_key);
    cJSON *pow_obj = cJSON_CreateObject();
    if (!pow_obj) {
      cJSON_Delete(body_obj);
      return NULL;
    }
    cJSON_AddNumberToObject(pow_obj, "t", (double)timestamp);
    cJSON_AddStringToObject(pow_obj, "n", nonce);
    cJSON_AddNumberToObject(pow_obj, "d", difficulty);
    if (pow_token[0]) {
      cJSON_AddStringToObject(pow_obj, "token", pow_token);
    }
    cJSON_AddItemToObject(body_obj, "pow", pow_obj);
    char *body = cJSON_PrintUnformatted(body_obj);
    cJSON_Delete(body_obj);
    if (!body)
      return NULL;

    tm_http_response_t *acc_resp = tm_http_request(
        TM_HTTP_POST, MAILTD_BASE "/accounts", hdrs_post, body, timeout);
    free(body);

    if (!acc_resp || !acc_resp->body) {
      TM_LOG_ERR("[mail-td] 创建账户请求失败, status=%ld",
                 acc_resp ? acc_resp->status : 0);
      tm_http_response_free(acc_resp);
      return NULL;
    }

    long status_code = acc_resp->status;
    cJSON *acc_json = cJSON_Parse(acc_resp->body);
    tm_http_response_free(acc_resp);
    if (!acc_json) {
      TM_LOG_ERR("[mail-td] 解析创建账户响应失败");
      return NULL;
    }

    /* 检查是否需要提升 difficulty 重试 */
    const cJSON *j_status =
        cJSON_GetObjectItemCaseSensitive(acc_json, "status");
    if (cJSON_IsString(j_status) && j_status->valuestring &&
        strcmp(j_status->valuestring, "retry") == 0) {
      const cJSON *j_rd =
          cJSON_GetObjectItemCaseSensitive(acc_json, "required_difficulty");
      if (cJSON_IsNumber(j_rd)) {
        difficulty = (int)j_rd->valuedouble;
      } else {
        difficulty += 2;
      }
      const cJSON *j_tk = cJSON_GetObjectItemCaseSensitive(acc_json, "token");
      if (cJSON_IsString(j_tk) && j_tk->valuestring) {
        snprintf(pow_token, sizeof(pow_token), "%s", j_tk->valuestring);
      }
      cJSON_Delete(acc_json);
      continue;
    }

    if (status_code < 200 || status_code >= 300) {
      const cJSON *j_err = cJSON_GetObjectItemCaseSensitive(acc_json, "error");
      TM_LOG_ERR("[mail-td] 创建账户 HTTP %ld: %s", status_code,
                 (cJSON_IsString(j_err) && j_err->valuestring)
                     ? j_err->valuestring
                     : "");
      cJSON_Delete(acc_json);
      return NULL;
    }

    const cJSON *j_addr = cJSON_GetObjectItemCaseSensitive(acc_json, "address");
    const cJSON *j_token = cJSON_GetObjectItemCaseSensitive(acc_json, "token");
    const cJSON *j_id = cJSON_GetObjectItemCaseSensitive(acc_json, "id");
    if (!cJSON_IsString(j_addr) || !j_addr->valuestring ||
        !j_addr->valuestring[0] || !cJSON_IsString(j_token) ||
        !j_token->valuestring || !j_token->valuestring[0] ||
        !cJSON_IsString(j_id) || !j_id->valuestring || !j_id->valuestring[0]) {
      TM_LOG_ERR("[mail-td] 响应缺少必要字段");
      cJSON_Delete(acc_json);
      return NULL;
    }

    /* 构建 token JSON: {"jwt":"...","id":"..."} */
    cJSON *tok_obj = cJSON_CreateObject();
    if (!tok_obj) {
      cJSON_Delete(acc_json);
      return NULL;
    }
    cJSON_AddStringToObject(tok_obj, "jwt", j_token->valuestring);
    cJSON_AddStringToObject(tok_obj, "id", j_id->valuestring);
    char *token_str = cJSON_PrintUnformatted(tok_obj);
    cJSON_Delete(tok_obj);
    if (!token_str) {
      cJSON_Delete(acc_json);
      return NULL;
    }

    tm_email_info_t *info = tm_email_info_new();
    if (!info) {
      free(token_str);
      cJSON_Delete(acc_json);
      return NULL;
    }
    info->channel = CHANNEL_MAIL_TD;
    info->email = tm_strdup(j_addr->valuestring);
    info->token = tm_strdup(token_str);
    info->expires_at = 0;
    info->created_at = tm_strdup("");

    free(token_str);
    cJSON_Delete(acc_json);

    TM_LOG_DBG("[mail-td] 创建邮箱成功: %s", info->email);
    return info;
  }

  TM_LOG_ERR("[mail-td] PoW 重试次数超限");
  return NULL;
}

/* ========== 获取邮件 ========== */

/**
 * 获取 mail-td 邮件列表
 *
 * @param token  存储的 token JSON {"jwt":"...","id":"..."}
 * @param email  完整邮箱地址
 * @param count  输出邮件数量，-1 表示请求失败
 */
tm_email_t *tm_provider_mail_td_get_emails(const char *token, const char *email,
                                           int *count) {
  *count = -1;
  if (!token || !email)
    return NULL;

  /* 解析 token JSON 提取 jwt 与 id */
  cJSON *tok_json = cJSON_Parse(token);
  if (!tok_json) {
    TM_LOG_ERR("[mail-td] token 格式无效");
    return NULL;
  }
  const cJSON *j_jwt = cJSON_GetObjectItemCaseSensitive(tok_json, "jwt");
  const cJSON *j_id = cJSON_GetObjectItemCaseSensitive(tok_json, "id");
  if (!cJSON_IsString(j_jwt) || !j_jwt->valuestring || !j_jwt->valuestring[0] ||
      !cJSON_IsString(j_id) || !j_id->valuestring || !j_id->valuestring[0]) {
    TM_LOG_ERR("[mail-td] token 缺少 jwt 或 id");
    cJSON_Delete(tok_json);
    return NULL;
  }

  int timeout =
      tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

  char url[512];
  snprintf(url, sizeof(url), MAILTD_BASE "/accounts/%s/messages?page=1",
           j_id->valuestring);

  char auth_line[2048];
  snprintf(auth_line, sizeof(auth_line), "Authorization: Bearer %s",
           j_jwt->valuestring);

  const char *hdrs[] = {mailtd_ua, "Accept: application/json", auth_line, NULL};

  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, hdrs, NULL, timeout);
  cJSON_Delete(tok_json);

  if (!resp || resp->status < 200 || resp->status >= 300 || !resp->body) {
    TM_LOG_ERR("[mail-td] 获取邮件请求失败, status=%ld",
               resp ? resp->status : 0);
    tm_http_response_free(resp);
    return NULL;
  }

  cJSON *json = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  if (!json) {
    TM_LOG_ERR("[mail-td] 解析邮件列表失败");
    return NULL;
  }

  cJSON *arr = cJSON_GetObjectItemCaseSensitive(json, "messages");
  if (!cJSON_IsArray(arr)) {
    TM_LOG_ERR("[mail-td] 响应中未找到 messages 数组");
    cJSON_Delete(json);
    return NULL;
  }

  int msg_count = cJSON_GetArraySize(arr);
  if (msg_count <= 0) {
    cJSON_Delete(json);
    *count = 0;
    return NULL;
  }
  if (msg_count > MAILTD_MAX_MAILS)
    msg_count = MAILTD_MAX_MAILS;

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

    cJSON *raw = cJSON_CreateObject();
    if (!raw)
      continue;

    const char *id_keys[] = {"id"};
    char *id_str = tm_json_get_str(msg, id_keys, 1);
    cJSON_AddStringToObject(raw, "id", id_str);
    free(id_str);

    /* from 为对象 {address,name}，提取 address */
    const cJSON *from_obj = cJSON_GetObjectItemCaseSensitive(msg, "from");
    char *from_str = NULL;
    if (cJSON_IsObject(from_obj)) {
      const char *fa_keys[] = {"address"};
      from_str = tm_json_get_str(from_obj, fa_keys, 1);
    } else {
      from_str = tm_strdup("");
    }
    cJSON_AddStringToObject(raw, "from", from_str);
    free(from_str);

    cJSON_AddStringToObject(raw, "to", email);

    const char *subj_keys[] = {"subject"};
    char *subj_str = tm_json_get_str(msg, subj_keys, 1);
    cJSON_AddStringToObject(raw, "subject", subj_str);
    free(subj_str);

    const char *text_keys[] = {"text"};
    char *text_str = tm_json_get_str(msg, text_keys, 1);
    cJSON_AddStringToObject(raw, "text", text_str);
    free(text_str);

    const char *html_keys[] = {"html"};
    char *html_str = tm_json_get_str(msg, html_keys, 1);
    cJSON_AddStringToObject(raw, "html", html_str);
    free(html_str);

    const char *date_keys[] = {"created_at"};
    char *date_str = tm_json_get_str(msg, date_keys, 1);
    cJSON_AddStringToObject(raw, "created_at", date_str);
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
