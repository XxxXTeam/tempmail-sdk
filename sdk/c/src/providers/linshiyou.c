/**
 * linshiyou.com 临时邮
 */
#include "tempmail_internal.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LSY_ORIGIN "https://linshiyou.com"
#define LSY_NEXT "<-----TMAILNEXTMAIL----->"
#define LSY_CHOP "<-----TMAILCHOPPER----->"

static const char *lsy_headers[] = {
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    "accept-language: zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
    "cache-control: no-cache",
    "dnt: 1",
    "pragma: no-cache",
    "priority: u=1, i",
    "referer: https://linshiyou.com/",
    "sec-ch-ua: \"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"146\"",
    "sec-ch-ua-mobile: ?0",
    "sec-ch-ua-platform: \"Windows\"",
    "sec-fetch-dest: empty",
    "sec-fetch-mode: cors",
    "sec-fetch-site: same-origin",
    NULL
};

static void tm_lsy_trim_crlf(char *s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\r' || s[n - 1] == '\n' || s[n - 1] == ' ' || s[n - 1] == '\t'))
        s[--n] = '\0';
}

static int tm_lsy_extract_nexus(const char *cookies, char *out, size_t cap) {
    if (!cookies || !out || cap < 2) return -1;
    const char *needle = "NEXUS_TOKEN=";
    const char *p = strstr(cookies, needle);
    if (!p) return -1;
    p += strlen(needle);
    const char *end = strchr(p, ';');
    size_t len = end ? (size_t)(end - p) : strlen(p);
    while (len > 0 && (p[len - 1] == ' ' || p[len - 1] == '\t')) len--;
    if (len == 0 || len >= cap) return -1;
    memcpy(out, p, len);
    out[len] = '\0';
    return 0;
}

static void tm_lsy_decode_basic(char *s) {
    /* 原地简化实体解码（常见几项） */
    char *r, *w;
    for (r = w = s; *r; ) {
        if (strncmp(r, "&quot;", 6) == 0) {
            *w++ = '"';
            r += 6;
        } else if (strncmp(r, "&lt;", 4) == 0) {
            *w++ = '<';
            r += 4;
        } else if (strncmp(r, "&gt;", 4) == 0) {
            *w++ = '>';
            r += 4;
        } else if (strncmp(r, "&amp;", 5) == 0) {
            *w++ = '&';
            r += 5;
        } else {
            *w++ = *r++;
        }
    }
    *w = '\0';
}

static void tm_lsy_strip_tags(const char *in, char *out, size_t cap) {
    if (!in || !out || cap < 2) {
        if (out && cap) out[0] = '\0';
        return;
    }
    size_t o = 0;
    int in_tag = 0;
    for (const unsigned char *p = (const unsigned char *)in; *p && o + 1 < cap; p++) {
        if (*p == '<') in_tag = 1;
        else if (*p == '>') { in_tag = 0; continue; }
        else if (!in_tag) {
            if (o > 0 && isspace(*p) && isspace((unsigned char)out[o - 1]))
                continue;
            out[o++] = (char)*p;
        }
    }
    out[o] = '\0';
    tm_lsy_decode_basic(out);
}

static void tm_lsy_pick_div(const char *html, const char *cls, char *out, size_t cap) {
    char marker[96];
    snprintf(marker, sizeof(marker), "class=\"%s\"", cls);
    const char *p = strstr(html, marker);
    out[0] = '\0';
    if (!p) return;
    p = strchr(p, '>');
    if (!p) return;
    p++;
    const char *end = strstr(p, "</div>");
    if (!end) return;
    size_t len = (size_t)(end - p);
    char tmp[4096];
    if (len >= sizeof(tmp)) len = sizeof(tmp) - 1;
    memcpy(tmp, p, len);
    tmp[len] = '\0';
    tm_lsy_strip_tags(tmp, out, cap);
}

static void tm_lsy_extract_srcdoc(const char *body, char *out, size_t cap) {
    out[0] = '\0';
    const char *p = strstr(body, "srcdoc=\"");
    if (!p) return;
    p += 9;
    const char *end = strchr(p, '"');
    if (!end) return;
    size_t len = (size_t)(end - p);
    if (len >= cap) len = cap - 1;
    memcpy(out, p, len);
    out[len] = '\0';
    tm_lsy_decode_basic(out);
}

static void tm_lsy_parse_time_iso(const char *s, char *out, size_t cap) {
    out[0] = '\0';
    if (!s || !*s) return;
    /* 期望 2026-03-31 05:08:02 → ISO 近似 */
    int y, mo, d, h, mi, se;
    if (sscanf(s, "%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &se) == 6) {
        snprintf(out, cap, "%04d-%02d-%02dT%02d:%02d:%02d.000Z", y, mo, d, h, mi, se);
    }
}

static int tm_lsy_extract_list_id(const char *list_part, char *id_out, size_t cap) {
    const char *needle = "id=\"tmail-email-list-";
    const char *p = strstr(list_part, needle);
    if (!p) return -1;
    p += strlen(needle);
    size_t i = 0;
    while (p[i] && p[i] != '"' && i + 1 < cap) {
        id_out[i] = p[i];
        i++;
    }
    id_out[i] = '\0';
    return (i > 0) ? 0 : -1;
}

static tm_email_t *tm_lsy_parse_one(const char *list_part, const char *body_part, const char *recipient,
                                    tm_attachment_t **atts, int *att_count) {
    char idbuf[128];
    if (tm_lsy_extract_list_id(list_part, idbuf, sizeof(idbuf)) != 0) return NULL;

    char from_list[512], subj_list[512], prev_list[512];
    char from_body[512], time_body[512], title_body[512], htmlbuf[16384];

    tm_lsy_pick_div(list_part, "name", from_list, sizeof(from_list));
    tm_lsy_pick_div(list_part, "subject", subj_list, sizeof(subj_list));
    tm_lsy_pick_div(list_part, "body", prev_list, sizeof(prev_list));
    tm_lsy_pick_div(body_part, "tmail-email-sender", from_body, sizeof(from_body));
    tm_lsy_pick_div(body_part, "tmail-email-time", time_body, sizeof(time_body));
    tm_lsy_pick_div(body_part, "tmail-email-title", title_body, sizeof(title_body));
    tm_lsy_extract_srcdoc(body_part, htmlbuf, sizeof(htmlbuf));

    const char *from = from_body[0] ? from_body : from_list;
    const char *subject = title_body[0] ? title_body : subj_list;
    char textbuf[16384];
    if (prev_list[0]) {
        strncpy(textbuf, prev_list, sizeof(textbuf) - 1);
        textbuf[sizeof(textbuf) - 1] = '\0';
    } else {
        tm_lsy_strip_tags(htmlbuf, textbuf, sizeof(textbuf));
    }

    char date_iso[64];
    tm_lsy_parse_time_iso(time_body, date_iso, sizeof(date_iso));

    tm_email_t *em = (tm_email_t *)calloc(1, sizeof(tm_email_t));
    if (!em) return NULL;
    em->id = tm_strdup(idbuf);
    em->from_addr = tm_strdup(from);
    em->to = tm_strdup(recipient);
    em->subject = tm_strdup(subject);
    em->text = tm_strdup(textbuf);
    em->html = tm_strdup(htmlbuf);
    em->date = tm_strdup(date_iso);
    em->is_read = false;
    *att_count = 0;
    *atts = NULL;

    const char *dl = strstr(body_part, "/api/download?id=");
    if (dl) {
        const char *dq = strchr(dl, '"');
        size_t ulen = dq ? (size_t)(dq - dl) : strlen(dl);
        char *url = (char *)malloc(strlen(LSY_ORIGIN) + ulen + 1);
        if (url) {
            memcpy(url, LSY_ORIGIN, strlen(LSY_ORIGIN));
            memcpy(url + strlen(LSY_ORIGIN), dl, ulen);
            url[strlen(LSY_ORIGIN) + ulen] = '\0';
            tm_attachment_t *a = (tm_attachment_t *)calloc(1, sizeof(tm_attachment_t));
            if (a) {
                a->url = url;
                a->filename = tm_strdup("");
                a->size = -1;
                *atts = a;
                *att_count = 1;
            } else {
                free(url);
            }
        }
    }
    return em;
}

tm_email_info_t *tm_provider_linshiyou_generate(void) {
    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, LSY_ORIGIN "/api/user?user", lsy_headers, NULL, 15);
    if (!resp || resp->status != 200) {
        tm_http_response_free(resp);
        return NULL;
    }

    char nexus[128];
    if (!resp->cookies || tm_lsy_extract_nexus(resp->cookies, nexus, sizeof(nexus)) != 0) {
        tm_http_response_free(resp);
        return NULL;
    }

    if (!resp->body) {
        tm_http_response_free(resp);
        return NULL;
    }
    char *email = tm_strdup(resp->body);
    tm_http_response_free(resp);
    tm_lsy_trim_crlf(email);
    if (!email[0] || !strchr(email, '@')) {
        free(email);
        return NULL;
    }

    size_t toklen = strlen(nexus) + strlen(email) + 48;
    char *token = (char *)malloc(toklen);
    if (!token) {
        free(email);
        return NULL;
    }
    snprintf(token, toklen, "NEXUS_TOKEN=%s; tmail-emails=%s", nexus, email);

    tm_email_info_t *info = tm_email_info_new();
    info->channel = CHANNEL_LINSHIYOU;
    info->email = email;
    info->token = token;
    return info;
}

tm_email_t *tm_provider_linshiyou_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    if (!token || !email) return NULL;

    char cookie_hdr[1024];
    snprintf(cookie_hdr, sizeof(cookie_hdr), "Cookie: %s", token);

    const char *headers[24];
    int hi = 0;
    headers[hi++] = cookie_hdr;
    headers[hi++] = "x-requested-with: XMLHttpRequest";
    for (int i = 0; lsy_headers[i] && hi < 22; i++) {
        headers[hi++] = lsy_headers[i];
    }
    headers[hi++] = NULL;

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, LSY_ORIGIN "/api/mail?unseen=1", headers, NULL, 15);
    if (!resp || resp->status != 200 || !resp->body) {
        tm_http_response_free(resp);
        return NULL;
    }

    const char *raw = resp->body;
    tm_email_t *list = NULL;
    int n = 0;

    const char *cursor = raw;
    for (;;) {
        const char *next = strstr(cursor, LSY_NEXT);
        size_t seglen = next ? (size_t)(next - cursor) : strlen(cursor);
        if (seglen > 0) {
            char *seg = (char *)malloc(seglen + 1);
            if (seg) {
                memcpy(seg, cursor, seglen);
                seg[seglen] = '\0';

                char *chop_pos = strstr(seg, LSY_CHOP);
                const char *list_part = seg;
                const char *body_part = "";
                if (chop_pos) {
                    *chop_pos = '\0';
                    body_part = chop_pos + strlen(LSY_CHOP);
                }

                tm_attachment_t *atts = NULL;
                int ac = 0;
                tm_email_t *one = tm_lsy_parse_one(list_part, body_part, email, &atts, &ac);
                if (one) {
                    one->attachments = atts;
                    one->attachment_count = ac;
                    tm_email_t *nl = (tm_email_t *)realloc(list, (size_t)(n + 1) * sizeof(tm_email_t));
                    if (nl) {
                        list = nl;
                        list[n] = *one;
                        free(one);
                        n++;
                    } else {
                        tm_free_email(one);
                        free(one);
                    }
                }
                free(seg);
            }
        }
        if (!next) break;
        cursor = next + strlen(LSY_NEXT);
    }

    tm_http_response_free(resp);
    *count = n;
    return list;
}
