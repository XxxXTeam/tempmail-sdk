/**
 * maildrop.cc 临时邮箱渠道 — https://maildrop.cc
 *
 * GraphQL API（api.maildrop.cc/graphql），无认证。
 * 创建邮箱: 无需 API，直接生成随机用户名 +
 * "@maildrop.cc"（公共邮箱，任何人可查看任意地址收件箱） 获取邮件:
 *   1. inbox(mailbox) 查询邮件列表（id/headerfrom/subject/date，无正文）
 *   2. message(mailbox,id) 查询单封详情（含 html 正文）
 * token 未使用（公共邮箱），传空字符串即可。
 */
#include "tempmail_internal.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MDCC_DOMAIN "maildrop.cc"
#define MDCC_GRAPHQL_URL "https://api.maildrop.cc/graphql"
#define MDCC_MAX_MAILS 128

static const char *mdcc_headers[] = {
    "Accept: application/json",
    "Content-Type: application/json",
    "Origin: https://maildrop.cc",
    "Referer: https://maildrop.cc/",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    NULL};

/* 生成随机用户名（小写字母 + 数字） */
static void mdcc_random_local(char *buf, int len) {
  static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
  for (int i = 0; i < len; i++) {
    buf[i] = chars[rand() % (int)(sizeof(chars) - 1)];
  }
  buf[len] = '\0';
}

/* 提取地址 @ 前的用户名部分（mailbox） */
static char *mdcc_mailbox(const char *email) {
  if (!email)
    return tm_strdup("");
  const char *at = strchr(email, '@');
  if (!at)
    return tm_strdup(email);
  size_t n = (size_t)(at - email);
  char *out = (char *)malloc(n + 1);
  if (!out)
    return tm_strdup("");
  memcpy(out, email, n);
  out[n] = '\0';
  return out;
}

/*
 * 发送 GraphQL 请求并返回 data 下指定字段的 cJSON（调用者持有返回的根对象，需
 * cJSON_Delete） query 通过 cJSON 序列化为 {"query": "..."}，避免转义问题
 */
static cJSON *mdcc_do_graphql(const char *query, int timeout) {
  cJSON *reqObj = cJSON_CreateObject();
  if (!reqObj)
    return NULL;
  cJSON_AddStringToObject(reqObj, "query", query);
  char *payload = cJSON_PrintUnformatted(reqObj);
  cJSON_Delete(reqObj);
  if (!payload)
    return NULL;

  tm_http_response_t *resp = tm_http_request(TM_HTTP_POST, MDCC_GRAPHQL_URL,
                                             mdcc_headers, payload, timeout);
  free(payload);
  if (!resp || resp->status < 200 || resp->status >= 300 || !resp->body) {
    tm_http_response_free(resp);
    return NULL;
  }
  cJSON *root = cJSON_Parse(resp->body);
  tm_http_response_free(resp);
  return root;
}

/*
 * 构造 inbox 查询语句，mailbox 经 cJSON 转义为合法 JSON 字符串字面量
 * 形如: query { inbox(mailbox: "user") { id headerfrom subject date } }
 */
static char *mdcc_build_inbox_query(const char *mailbox) {
  cJSON *s = cJSON_CreateString(mailbox);
  if (!s)
    return NULL;
  char *quoted = cJSON_PrintUnformatted(s); /* 含两端引号且已转义 */
  cJSON_Delete(s);
  if (!quoted)
    return NULL;
  size_t cap = strlen(quoted) + 96;
  char *q = (char *)malloc(cap);
  if (!q) {
    free(quoted);
    return NULL;
  }
  snprintf(q, cap,
           "query { inbox(mailbox: %s) { id headerfrom subject date } }",
           quoted);
  free(quoted);
  return q;
}

/* 构造 message 单封详情查询语句 */
static char *mdcc_build_message_query(const char *mailbox, const char *id) {
  cJSON *sm = cJSON_CreateString(mailbox);
  cJSON *si = cJSON_CreateString(id);
  if (!sm || !si) {
    cJSON_Delete(sm);
    cJSON_Delete(si);
    return NULL;
  }
  char *qm = cJSON_PrintUnformatted(sm);
  char *qi = cJSON_PrintUnformatted(si);
  cJSON_Delete(sm);
  cJSON_Delete(si);
  if (!qm || !qi) {
    free(qm);
    free(qi);
    return NULL;
  }
  size_t cap = strlen(qm) + strlen(qi) + 96;
  char *q = (char *)malloc(cap);
  if (!q) {
    free(qm);
    free(qi);
    return NULL;
  }
  snprintf(q, cap,
           "query { message(mailbox: %s, id: %s) { id headerfrom subject date "
           "html } }",
           qm, qi);
  free(qm);
  free(qi);
  return q;
}

/*
 * 创建 maildrop.cc 临时邮箱
 * 无需 API 调用，直接生成随机用户名 + "@maildrop.cc"
 */
tm_email_info_t *tm_provider_maildrop_cc_generate(void) {
  srand((unsigned)time(NULL) ^ (unsigned)clock());
  char local[16];
  mdcc_random_local(local, 10);

  char email[64];
  snprintf(email, sizeof(email), "%s@%s", local, MDCC_DOMAIN);

  tm_email_info_t *info = tm_email_info_new();
  if (!info)
    return NULL;
  info->channel = CHANNEL_MAILDROP_CC;
  info->email = tm_strdup(email);
  info->token = tm_strdup("");
  if (!info->email) {
    tm_free_email_info(info);
    return NULL;
  }
  return info;
}

/*
 * 获取 maildrop.cc 邮件列表
 * 先用 inbox 查询拿到 id 列表，再用 message 查询逐封补全 html 正文
 * token 未使用（公共邮箱）
 */
tm_email_t *tm_provider_maildrop_cc_get_emails(const char *token,
                                               const char *email, int *count) {
  (void)token;
  *count = -1;
  if (!email || !email[0])
    return NULL;

  char *mailbox = mdcc_mailbox(email);
  if (!mailbox || !mailbox[0]) {
    free(mailbox);
    return NULL;
  }

  int timeout =
      tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

  /* 1. 查询邮件列表 */
  char *inboxQuery = mdcc_build_inbox_query(mailbox);
  if (!inboxQuery) {
    free(mailbox);
    return NULL;
  }
  cJSON *inboxRoot = mdcc_do_graphql(inboxQuery, timeout);
  free(inboxQuery);
  if (!inboxRoot) {
    free(mailbox);
    return NULL;
  }

  cJSON *data = cJSON_GetObjectItemCaseSensitive(inboxRoot, "data");
  cJSON *inbox = data ? cJSON_GetObjectItemCaseSensitive(data, "inbox") : NULL;
  int n = (inbox && cJSON_IsArray(inbox)) ? cJSON_GetArraySize(inbox) : 0;
  if (n == 0) {
    cJSON_Delete(inboxRoot);
    free(mailbox);
    *count = 0;
    return NULL;
  }
  if (n > MDCC_MAX_MAILS)
    n = MDCC_MAX_MAILS;

  tm_email_t *emails = tm_emails_new(n);
  if (!emails) {
    cJSON_Delete(inboxRoot);
    free(mailbox);
    return NULL;
  }

  int valid = 0;
  for (int i = 0; i < n; i++) {
    const cJSON *item = cJSON_GetArrayItem(inbox, i);
    if (!cJSON_IsObject(item))
      continue;
    const char *id =
        cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(item, "id"));
    if (!id || !id[0])
      continue;

    /* 2. 查询单封详情补全 html，失败则回退列表元信息 */
    cJSON *raw = cJSON_CreateObject();
    if (!raw)
      continue;

    char *msgQuery = mdcc_build_message_query(mailbox, id);
    cJSON *msgRoot = msgQuery ? mdcc_do_graphql(msgQuery, timeout) : NULL;
    free(msgQuery);

    const cJSON *src = item; /* 默认用列表元信息 */
    const cJSON *html = NULL;
    if (msgRoot) {
      cJSON *mdata = cJSON_GetObjectItemCaseSensitive(msgRoot, "data");
      cJSON *message =
          mdata ? cJSON_GetObjectItemCaseSensitive(mdata, "message") : NULL;
      if (cJSON_IsObject(message)) {
        src = message;
        html = cJSON_GetObjectItemCaseSensitive(message, "html");
      }
    }

    cJSON_AddStringToObject(raw, "id", id);
    cJSON_AddStringToObject(raw, "to", email);
    const char *from = cJSON_GetStringValue(
        cJSON_GetObjectItemCaseSensitive(src, "headerfrom"));
    cJSON_AddStringToObject(raw, "from", from ? from : "");
    const char *subject =
        cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(src, "subject"));
    cJSON_AddStringToObject(raw, "subject", subject ? subject : "");
    const char *date =
        cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(src, "date"));
    cJSON_AddStringToObject(raw, "date", date ? date : "");
    if (cJSON_IsString(html) && html->valuestring) {
      cJSON_AddStringToObject(raw, "html", html->valuestring);
    }

    emails[valid] = tm_normalize_email(raw, email);
    cJSON_Delete(raw);
    cJSON_Delete(msgRoot);
    valid++;
  }

  cJSON_Delete(inboxRoot);
  free(mailbox);

  if (valid == 0) {
    free(emails);
    *count = 0;
    return NULL;
  }
  *count = valid;
  return emails;
}
