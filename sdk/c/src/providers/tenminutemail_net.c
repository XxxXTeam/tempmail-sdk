/**
 * 10minutemail.net 临时邮箱渠道 — https://10minutemail.net
 *
 * 创建邮箱:
 *   1. GET / 获取 PHPSESSID 会话 Cookie
 *   2. 从首页 HTML 输入框正则提取随机邮箱地址（value="xxx@xxx.com"）
 * 获取邮件:
 *   1. GET /mailbox.ajax.php?_={毫秒时间戳}（带 Cookie）→ HTML 表格
 *   2. 逐行解析 <tr>：提取 mid、发件人、主题、收件时间、已读状态
 *   3. 对每封邮件 GET /readmail.html?mid={id} 获取整页，提取 mailinhtml
 * 正文片段 表格列顺序: 寄件人 | 主题 | 收件日期；未读行 <tr> 含 font-weight:
 * bold 发件人/主题可能被 Cloudflare 邮箱保护混淆（data-cfemail），需解码。
 *
 * token 存储会话 Cookie 串（明文）
 */
#include "tempmail_internal.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TENNET_BASE "https://10minutemail.net"
#define TENNET_MAX_COOKIE 8192
#define TENNET_MAX_MAILS 128

static const char *tennet_ua =
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";

/* ========== 邮箱地址提取 ========== */

/* 从首页 HTML 提取 value="xxx@yyy" 形式的邮箱地址 */
static char *tennet_extract_email(const char *html) {
  if (!html)
    return NULL;
  const char *p = html;
  while ((p = strstr(p, "value=\"")) != NULL) {
    const char *start = p + strlen("value=\"");
    const char *end = strchr(start, '"');
    if (!end)
      break;
    size_t len = (size_t)(end - start);
    /* 校验是否含 @ 且为合理邮箱 */
    const char *at = memchr(start, '@', len);
    if (at && at != start && at != end - 1) {
      char *email = (char *)malloc(len + 1);
      if (!email)
        return NULL;
      memcpy(email, start, len);
      email[len] = '\0';
      /* 去首尾空白 */
      char *a = email;
      while (*a == ' ' || *a == '\t')
        a++;
      char *b = email + strlen(email);
      while (b > a && (b[-1] == ' ' || b[-1] == '\t'))
        b--;
      *b = '\0';
      if (a != email)
        memmove(email, a, strlen(a) + 1);
      return email;
    }
    p = end + 1;
  }
  return NULL;
}

/* ========== Cloudflare 邮箱解混淆 ========== */

static int tennet_hexval(int c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

/*
 * 解码 Cloudflare data-cfemail 混淆字符串
 * 算法：首字节为异或密钥，其后每字节与密钥异或还原 ASCII
 * 成功且结果含 @ 时返回堆分配字符串，否则 NULL
 */
static char *tennet_cf_decode(const char *hex) {
  if (!hex)
    return NULL;
  size_t hl = strlen(hex);
  if (hl < 4 || hl % 2 != 0)
    return NULL;
  size_t nbytes = hl / 2;
  unsigned char *data = (unsigned char *)malloc(nbytes);
  if (!data)
    return NULL;
  for (size_t i = 0; i < nbytes; i++) {
    int hi = tennet_hexval((unsigned char)hex[i * 2]);
    int lo = tennet_hexval((unsigned char)hex[i * 2 + 1]);
    if (hi < 0 || lo < 0) {
      free(data);
      return NULL;
    }
    data[i] = (unsigned char)((hi << 4) | lo);
  }
  unsigned char key = data[0];
  char *out = (char *)malloc(nbytes); /* nbytes-1 字符 + 终止符 */
  if (!out) {
    free(data);
    return NULL;
  }
  size_t o = 0;
  for (size_t i = 1; i < nbytes; i++) {
    out[o++] = (char)(data[i] ^ key);
  }
  out[o] = '\0';
  free(data);
  if (!strchr(out, '@')) {
    free(out);
    return NULL;
  }
  return out;
}

/* ========== HTML 文本提取 ========== */

/* 反转义常见 HTML 实体（原地，输出长度 <= 输入） */
static void tennet_unescape_entities(char *s) {
  char *r = s, *w = s;
  while (*r) {
    if (*r == '&') {
      if (strncmp(r, "&nbsp;", 6) == 0) {
        *w++ = ' ';
        r += 6;
        continue;
      }
      if (strncmp(r, "&#160;", 6) == 0) {
        *w++ = ' ';
        r += 6;
        continue;
      }
      if (strncmp(r, "&amp;", 5) == 0) {
        *w++ = '&';
        r += 5;
        continue;
      }
      if (strncmp(r, "&lt;", 4) == 0) {
        *w++ = '<';
        r += 4;
        continue;
      }
      if (strncmp(r, "&gt;", 4) == 0) {
        *w++ = '>';
        r += 4;
        continue;
      }
      if (strncmp(r, "&quot;", 6) == 0) {
        *w++ = '"';
        r += 6;
        continue;
      }
    }
    *w++ = *r++;
  }
  *w = '\0';
}

/*
 * 从单元格 HTML 提取纯文本发件人/主题
 * 优先解码 Cloudflare 邮箱保护混淆（data-cfemail），否则去标签 + 反转义
 * 返回堆分配字符串
 */
static char *tennet_extract_text(const char *cell) {
  if (!cell)
    return tm_strdup("");
  /* 优先解码 Cloudflare 混淆 */
  const char *cf = strstr(cell, "data-cfemail=\"");
  if (cf) {
    cf += strlen("data-cfemail=\"");
    const char *cfend = strchr(cf, '"');
    if (cfend && cfend > cf) {
      size_t hl = (size_t)(cfend - cf);
      char *hex = (char *)malloc(hl + 1);
      if (hex) {
        memcpy(hex, cf, hl);
        hex[hl] = '\0';
        char *decoded = tennet_cf_decode(hex);
        free(hex);
        if (decoded)
          return decoded;
      }
    }
  }
  /* 去标签 */
  size_t len = strlen(cell);
  char *out = (char *)malloc(len + 1);
  if (!out)
    return tm_strdup("");
  size_t o = 0;
  int in_tag = 0;
  for (const char *p = cell; *p; p++) {
    if (*p == '<') {
      in_tag = 1;
      continue;
    }
    if (*p == '>') {
      in_tag = 0;
      continue;
    }
    if (!in_tag)
      out[o++] = *p;
  }
  out[o] = '\0';
  tennet_unescape_entities(out);
  /* 去首尾空白 */
  char *a = out;
  while (*a == ' ' || *a == '\t' || *a == '\n' || *a == '\r')
    a++;
  char *b = out + strlen(out);
  while (b > a &&
         (b[-1] == ' ' || b[-1] == '\t' || b[-1] == '\n' || b[-1] == '\r'))
    b--;
  *b = '\0';
  if (a != out)
    memmove(out, a, strlen(a) + 1);
  return out;
}

/* ========== 正文提取 ========== */

/*
 * 从 readmail 整页 HTML 提取正文片段
 * 正文包裹于 <div class="mailinhtml"> ... </div>，内部存在嵌套 div，
 * 以紧随其后的 email-decode.min.js script 标签作为结束锚点，再裁掉尾部 script
 * 与两个闭合 </div> 返回堆分配字符串（可能为空串）
 */
static char *tennet_extract_body(const char *page) {
  if (!page)
    return tm_strdup("");
  const char *si = strstr(page, "class=\"mailinhtml\"");
  if (!si)
    return tm_strdup("");
  const char *gt = strchr(si, '>');
  if (!gt)
    return tm_strdup("");
  const char *content = gt + 1;

  /* 结束锚点：mailinhtml 区块后紧跟的 cloudflare email-decode 脚本 */
  const char *ei = strstr(content, "email-decode.min.js");
  const char *seg_end;
  if (!ei) {
    /* 兜底：该 div 后第一个 </div> */
    const char *di = strstr(content, "</div>");
    seg_end = di ? di : content + strlen(content);
  } else {
    seg_end = ei;
  }

  size_t seg_len = (size_t)(seg_end - content);
  char *seg = (char *)malloc(seg_len + 1);
  if (!seg)
    return tm_strdup("");
  memcpy(seg, content, seg_len);
  seg[seg_len] = '\0';

  if (ei) {
    /* 裁掉结尾的 <script 起始 */
    char *sidx = NULL;
    for (char *p = seg; *p; p++) {
      if (strncmp(p, "<script", 7) == 0)
        sidx = p;
    }
    if (sidx)
      *sidx = '\0';
    /* 去掉两个包裹的 </div> */
    for (int i = 0; i < 2; i++) {
      /* trim 尾部空白 */
      size_t l = strlen(seg);
      while (l > 0 && (seg[l - 1] == ' ' || seg[l - 1] == '\t' ||
                       seg[l - 1] == '\n' || seg[l - 1] == '\r')) {
        seg[--l] = '\0';
      }
      l = strlen(seg);
      if (l >= 6 && strcmp(seg + l - 6, "</div>") == 0)
        seg[l - 6] = '\0';
    }
  }
  /* trim 首尾空白 */
  char *a = seg;
  while (*a == ' ' || *a == '\t' || *a == '\n' || *a == '\r')
    a++;
  size_t l = strlen(a);
  while (l > 0 && (a[l - 1] == ' ' || a[l - 1] == '\t' || a[l - 1] == '\n' ||
                   a[l - 1] == '\r'))
    a[--l] = '\0';
  if (a != seg)
    memmove(seg, a, strlen(a) + 1);
  return seg;
}

/* GET /readmail.html?mid={id} 获取正文 HTML 片段，失败返回 NULL */
static char *tennet_fetch_body(const char *cookie_hdr, const char *mid,
                               int timeout) {
  if (!mid || !mid[0])
    return NULL;
  char url[512];
  snprintf(url, sizeof(url), "%s/readmail.html?mid=%s", TENNET_BASE, mid);

  char h_ck[TENNET_MAX_COOKIE];
  snprintf(h_ck, sizeof(h_ck), "Cookie: %s", cookie_hdr ? cookie_hdr : "");

  const char *hdr[] = {
      tennet_ua,
      "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
      "Accept-Language: en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
      "Referer: " TENNET_BASE "/",
      h_ck,
      NULL};

  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, url, hdr, NULL, timeout);
  if (!resp || resp->status < 200 || resp->status >= 300 || !resp->body) {
    tm_http_response_free(resp);
    return NULL;
  }
  char *body = tennet_extract_body(resp->body);
  tm_http_response_free(resp);
  return body;
}

/* ========== 创建邮箱 ========== */

tm_email_info_t *tm_provider_tenminutemail_net_generate(void) {
  int timeout =
      tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

  const char *hdr[] = {tennet_ua,
                       "Accept: "
                       "text/html,application/xhtml+xml,application/"
                       "xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
                       "Accept-Language: en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
                       NULL};
  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, TENNET_BASE "/", hdr, NULL, timeout);
  if (!resp || resp->status < 200 || resp->status >= 300 || !resp->body) {
    tm_http_response_free(resp);
    return NULL;
  }

  char *ck = tm_strdup(resp->cookies ? resp->cookies : "");
  char *email = tennet_extract_email(resp->body);
  tm_http_response_free(resp);

  if (!email) {
    free(ck);
    return NULL;
  }
  /* cookie 可能为空（部分情况下站点不下发），仍保留地址但 token 为空串 */
  if (!ck)
    ck = tm_strdup("");

  tm_email_info_t *info = tm_email_info_new();
  if (!info) {
    free(ck);
    free(email);
    return NULL;
  }
  info->channel = CHANNEL_TENMINUTEMAIL_NET;
  info->email = email;
  info->token = ck;
  return info;
}

/* ========== 获取邮件 ========== */

tm_email_t *tm_provider_tenminutemail_net_get_emails(const char *token,
                                                     const char *email,
                                                     int *count) {
  *count = -1;
  if (!email || !email[0])
    return NULL;

  int timeout =
      tm_get_config()->timeout_secs > 0 ? tm_get_config()->timeout_secs : 15;

  char list_url[256];
  snprintf(list_url, sizeof(list_url), "%s/mailbox.ajax.php?_=%lld",
           TENNET_BASE, (long long)time(NULL) * 1000);

  char h_ck[TENNET_MAX_COOKIE];
  snprintf(h_ck, sizeof(h_ck), "Cookie: %s", token ? token : "");

  const char *hdr[] = {tennet_ua,
                       "Accept: */*",
                       "Accept-Language: en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
                       "X-Requested-With: XMLHttpRequest",
                       "Referer: " TENNET_BASE "/",
                       h_ck,
                       NULL};

  tm_http_response_t *resp =
      tm_http_request(TM_HTTP_GET, list_url, hdr, NULL, timeout);
  if (!resp || resp->status < 200 || resp->status >= 300 || !resp->body) {
    tm_http_response_free(resp);
    return NULL;
  }
  char *html = tm_strdup(resp->body);
  tm_http_response_free(resp);
  if (!html)
    return NULL;

  /* 逐行解析 <tr> ... </tr> */
  tm_email_t *emails = tm_emails_new(TENNET_MAX_MAILS);
  if (!emails) {
    free(html);
    return NULL;
  }

  int valid = 0;
  const char *p = html;
  while (valid < TENNET_MAX_MAILS) {
    const char *tr = strstr(p, "<tr");
    if (!tr)
      break;
    const char *tr_gt = strchr(tr, '>');
    if (!tr_gt)
      break;
    const char *tr_end = strstr(tr_gt, "</tr>");
    if (!tr_end)
      break;

    /* 整行（含 <tr ...> 起始标签，用于判断未读样式）与行内内容 */
    size_t full_len = (size_t)(tr_end - tr);
    size_t inner_len = (size_t)(tr_end - (tr_gt + 1));
    const char *inner_start = tr_gt + 1;
    p = tr_end + 5; /* 移到 </tr> 之后 */

    /* 复制行内内容用于解析 */
    char *inner = (char *)malloc(inner_len + 1);
    if (!inner)
      continue;
    memcpy(inner, inner_start, inner_len);
    inner[inner_len] = '\0';

    /* 跳过表头行（含 <th） */
    if (strstr(inner, "<th") || strstr(inner, "<TH")) {
      free(inner);
      continue;
    }

    /* 提取邮件 ID: readmail.html?mid=xxx */
    const char *mp = strstr(inner, "readmail.html?mid=");
    if (!mp) {
      free(inner);
      continue;
    }
    mp += strlen("readmail.html?mid=");
    const char *mend = mp;
    while (*mend && *mend != '\'' && *mend != '"' && *mend != '&' &&
           *mend != '<' && *mend != ' ')
      mend++;
    size_t mid_len = (size_t)(mend - mp);
    if (mid_len == 0) {
      free(inner);
      continue;
    }
    char mid[128];
    if (mid_len >= sizeof(mid))
      mid_len = sizeof(mid) - 1;
    memcpy(mid, mp, mid_len);
    mid[mid_len] = '\0';

    /* 提取三个 <td> 单元格 */
    char *cells[3] = {NULL, NULL, NULL};
    int ncell = 0;
    const char *cp = inner;
    while (ncell < 3) {
      const char *td = strstr(cp, "<td");
      if (!td)
        break;
      const char *td_gt = strchr(td, '>');
      if (!td_gt)
        break;
      const char *td_end = strstr(td_gt, "</td>");
      if (!td_end)
        break;
      size_t cl = (size_t)(td_end - (td_gt + 1));
      char *cell = (char *)malloc(cl + 1);
      if (cell) {
        memcpy(cell, td_gt + 1, cl);
        cell[cl] = '\0';
        cells[ncell++] = cell;
      }
      cp = td_end + 5;
    }

    char *from_addr = cells[0] ? tennet_extract_text(cells[0]) : tm_strdup("");
    char *subject = cells[1] ? tennet_extract_text(cells[1]) : tm_strdup("");

    /* 收件时间：优先取第三单元格内 title="..." 的 UTC 绝对时间 */
    char *date = NULL;
    if (cells[2]) {
      const char *tt = strstr(cells[2], "title=\"");
      if (tt) {
        tt += strlen("title=\"");
        const char *te = strchr(tt, '"');
        if (te && te > tt) {
          size_t dl = (size_t)(te - tt);
          date = (char *)malloc(dl + 1);
          if (date) {
            memcpy(date, tt, dl);
            date[dl] = '\0';
          }
        }
      }
      if (!date)
        date = tennet_extract_text(cells[2]);
    }
    if (!date)
      date = tm_strdup("");

    /* 未读判断：整行起始标签样式含 font-weight: bold */
    int is_read = 1;
    {
      char *full = (char *)malloc(full_len + 1);
      if (full) {
        memcpy(full, tr, full_len);
        full[full_len] = '\0';
        /* 转小写后查找 */
        for (char *q = full; *q; q++)
          *q = (char)tolower((unsigned char)*q);
        if (strstr(full, "font-weight: bold") ||
            strstr(full, "font-weight:bold"))
          is_read = 0;
        free(full);
      }
    }

    /* 获取正文 HTML */
    char *body_html = tennet_fetch_body(token, mid, timeout);

    cJSON *raw = cJSON_CreateObject();
    if (raw) {
      cJSON_AddStringToObject(raw, "id", mid);
      cJSON_AddStringToObject(raw, "to", email);
      cJSON_AddStringToObject(raw, "from", from_addr);
      cJSON_AddStringToObject(raw, "subject", subject);
      cJSON_AddStringToObject(raw, "date", date);
      cJSON_AddBoolToObject(raw, "is_read", is_read ? true : false);
      if (body_html && body_html[0])
        cJSON_AddStringToObject(raw, "html", body_html);
      emails[valid] = tm_normalize_email(raw, email);
      cJSON_Delete(raw);
      valid++;
    }

    free(body_html);
    free(from_addr);
    free(subject);
    free(date);
    for (int i = 0; i < 3; i++)
      free(cells[i]);
    free(inner);
  }

  free(html);

  if (valid == 0) {
    free(emails);
    *count = 0;
    return NULL;
  }
  *count = valid;
  return emails;
}
