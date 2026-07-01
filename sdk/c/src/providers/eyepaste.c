/**
 * EyePaste — https://eyepaste.com
 * RSS XML API，无认证
 * 域名固定 eyepaste.com，创建邮箱无需 API 调用
 * 获取邮件通过 RSS 2.0 XML 解析
 */
#include "tempmail_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define EP_BASE "https://www.eyepaste.com"

static const char *ep_headers[] = {
    "Accept: application/rss+xml, application/xml, text/xml, */*",
    "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    NULL
};

/* 随机生成 10 位小写字母数字用户名 */
static void ep_random_username(char *buf, size_t cap) {
    static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    size_t len = 10;
    if (cap < len + 1) len = cap - 1;
    for (size_t i = 0; i < len; i++) {
        buf[i] = chars[rand() % (sizeof(chars) - 1)];
    }
    buf[len] = '\0';
}

tm_email_info_t* tm_provider_eyepaste_generate(void) {
    srand((unsigned)time(NULL));

    char username[16];
    ep_random_username(username, sizeof(username));

    char email[128];
    snprintf(email, sizeof(email), "%s@eyepaste.com", username);

    tm_email_info_t *info = tm_email_info_new();
    info->channel = CHANNEL_EYEPASTE;
    info->email = tm_strdup(email);
    info->token = NULL;
    return info;
}

/**
 * 从 XML 中提取指定标签的内容
 * 返回新分配的字符串，调用者需要 free
 */
static char* ep_xml_extract_tag(const char *xml, const char *tag) {
    if (!xml || !tag) return NULL;

    char open_tag[128];
    char close_tag[128];
    snprintf(open_tag, sizeof(open_tag), "<%s>", tag);
    snprintf(close_tag, sizeof(close_tag), "</%s>", tag);

    const char *start = strstr(xml, open_tag);
    if (!start) return NULL;
    start += strlen(open_tag);

    /* 跳过 CDATA 包装 */
    if (strncmp(start, "<![CDATA[", 9) == 0) {
        start += 9;
        const char *cdata_end = strstr(start, "]]>");
        if (!cdata_end) return NULL;
        size_t len = (size_t)(cdata_end - start);
        char *result = (char*)malloc(len + 1);
        if (!result) return NULL;
        memcpy(result, start, len);
        result[len] = '\0';
        return result;
    }

    const char *end = strstr(start, close_tag);
    if (!end) return NULL;

    size_t len = (size_t)(end - start);
    char *result = (char*)malloc(len + 1);
    if (!result) return NULL;
    memcpy(result, start, len);
    result[len] = '\0';
    return result;
}

/**
 * 从 description 中的 <p> 标签解析邮件头信息
 * 格式: " From: {from} To: {to} Subject: {subject} Date: {date}"
 */
static void ep_parse_description_header(const char *desc,
    char *from_out, size_t from_cap,
    char *subject_out, size_t subject_cap,
    char *date_out, size_t date_cap)
{
    from_out[0] = '\0';
    subject_out[0] = '\0';
    date_out[0] = '\0';

    if (!desc) return;

    /* 查找 From: 字段 */
    const char *from_ptr = strstr(desc, "From:");
    if (!from_ptr) from_ptr = strstr(desc, "From :");
    if (from_ptr) {
        from_ptr += 5;
        while (*from_ptr == ' ' || *from_ptr == ':') from_ptr++;
        const char *end = strstr(from_ptr, " To:");
        if (!end) end = strstr(from_ptr, " To :");
        if (end) {
            size_t len = (size_t)(end - from_ptr);
            if (len >= from_cap) len = from_cap - 1;
            memcpy(from_out, from_ptr, len);
            from_out[len] = '\0';
            /* 去除尾部空格 */
            while (len > 0 && from_out[len - 1] == ' ') {
                from_out[--len] = '\0';
            }
        }
    }

    /* 查找 Subject: 字段 */
    const char *subj_ptr = strstr(desc, "Subject:");
    if (!subj_ptr) subj_ptr = strstr(desc, "Subject :");
    if (subj_ptr) {
        subj_ptr += 8;
        while (*subj_ptr == ' ' || *subj_ptr == ':') subj_ptr++;
        const char *end = strstr(subj_ptr, " Date:");
        if (!end) end = strstr(subj_ptr, " Date :");
        if (end) {
            size_t len = (size_t)(end - subj_ptr);
            if (len >= subject_cap) len = subject_cap - 1;
            memcpy(subject_out, subj_ptr, len);
            subject_out[len] = '\0';
            while (len > 0 && subject_out[len - 1] == ' ') {
                subject_out[--len] = '\0';
            }
        } else {
            /* Subject 后面没有 Date，取到行尾或 <br> */
            end = strstr(subj_ptr, "<br");
            if (!end) end = subj_ptr + strlen(subj_ptr);
            size_t len = (size_t)(end - subj_ptr);
            if (len >= subject_cap) len = subject_cap - 1;
            memcpy(subject_out, subj_ptr, len);
            subject_out[len] = '\0';
            while (len > 0 && subject_out[len - 1] == ' ') {
                subject_out[--len] = '\0';
            }
        }
    }

    /* 查找 Date: 字段 */
    const char *date_ptr = strstr(desc, "Date:");
    if (!date_ptr) date_ptr = strstr(desc, "Date :");
    if (date_ptr) {
        date_ptr += 5;
        while (*date_ptr == ' ' || *date_ptr == ':') date_ptr++;
        /* Date 后面一般到 <br> 或行尾 */
        const char *end = strstr(date_ptr, "<br");
        if (!end) end = strstr(date_ptr, "</p>");
        if (!end) end = date_ptr + strlen(date_ptr);
        size_t len = (size_t)(end - date_ptr);
        if (len >= date_cap) len = date_cap - 1;
        memcpy(date_out, date_ptr, len);
        date_out[len] = '\0';
        while (len > 0 && date_out[len - 1] == ' ') {
            date_out[--len] = '\0';
        }
    }
}

/**
 * 从 description 中提取邮件正文（<p> 头信息之后的内容）
 */
static char* ep_extract_body(const char *desc) {
    if (!desc) return tm_strdup("");

    /* 找到第一个 </p> 之后的内容作为正文 */
    const char *body_start = strstr(desc, "</p>");
    if (body_start) {
        body_start += 4;
        /* 跳过前导空白 */
        while (*body_start == '\n' || *body_start == '\r' || *body_start == ' ') body_start++;
        return tm_strdup(body_start);
    }

    /* 找到 <br> 或 <br/> 之后的内容 */
    body_start = strstr(desc, "<br");
    if (body_start) {
        /* 跳过 <br> 或 <br/> 标签 */
        const char *gt = strchr(body_start, '>');
        if (gt) {
            body_start = gt + 1;
            while (*body_start == '\n' || *body_start == '\r' || *body_start == ' ') body_start++;
            return tm_strdup(body_start);
        }
    }

    return tm_strdup(desc);
}

tm_email_t* tm_provider_eyepaste_get_emails(const char *email, int *count) {
    *count = -1;
    if (!email || !email[0]) return NULL;

    char url[512];
    snprintf(url, sizeof(url), "%s/inbox/%s.rss", EP_BASE, email);

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, ep_headers, NULL, 15);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        tm_http_response_free(resp);
        return NULL;
    }

    /* 解析 RSS XML，统计 <item> 数量 */
    const char *xml = resp->body;
    if (!xml) {
        tm_http_response_free(resp);
        return NULL;
    }

    /* 先统计 <item> 数量 */
    int n = 0;
    const char *p = xml;
    while ((p = strstr(p, "<item>")) != NULL) {
        n++;
        p += 6;
    }

    *count = n;
    if (n == 0) {
        tm_http_response_free(resp);
        return NULL;
    }

    tm_email_t *emails = tm_emails_new(n);
    if (!emails) {
        tm_http_response_free(resp);
        *count = -1;
        return NULL;
    }

    /* 逐个解析 <item> */
    p = xml;
    for (int i = 0; i < n; i++) {
        p = strstr(p, "<item>");
        if (!p) break;
        const char *item_end = strstr(p, "</item>");
        if (!item_end) break;

        /* 提取 item 内容 */
        size_t item_len = (size_t)(item_end - p + 7);
        char *item = (char*)malloc(item_len + 1);
        if (!item) break;
        memcpy(item, p, item_len);
        item[item_len] = '\0';

        /* 提取 title */
        char *title = ep_xml_extract_tag(item, "title");

        /* 提取 description */
        char *desc = ep_xml_extract_tag(item, "description");

        /* 从 description 解析头信息 */
        char from[512], subject[1024], date[256];
        ep_parse_description_header(desc, from, sizeof(from), subject, sizeof(subject), date, sizeof(date));

        /* 提取正文 */
        char *body_html = ep_extract_body(desc);

        /* 构建 cJSON 对象给 normalize */
        cJSON *raw = cJSON_CreateObject();
        if (raw) {
            if (from[0]) cJSON_AddStringToObject(raw, "from", from);
            /* 如果解析到了 subject 就用解析的，否则用 title */
            if (subject[0]) {
                cJSON_AddStringToObject(raw, "subject", subject);
            } else if (title) {
                cJSON_AddStringToObject(raw, "subject", title);
            }
            if (date[0]) cJSON_AddStringToObject(raw, "date", date);
            if (body_html) cJSON_AddStringToObject(raw, "html", body_html);
            cJSON_AddStringToObject(raw, "to", email);

            emails[i] = tm_normalize_email(raw, email);
            cJSON_Delete(raw);
        }

        free(title);
        free(desc);
        free(body_html);
        free(item);

        p = item_end + 7;
    }

    tm_http_response_free(resp);
    return emails;
}
