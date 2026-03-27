/**
 * maildrop.cc 渠道实现 (GraphQL)
 */
#include "tempmail_internal.h"
#include <ctype.h>
#include <time.h>

#ifdef _WIN32
#define MD_STRNICMP _strnicmp
#else
#include <strings.h>
#define MD_STRNICMP strncasecmp
#endif

#define MD_GQL "https://api.maildrop.cc/graphql"

static const char *md_headers[] = {
    "Content-Type: application/json",
    "Origin: https://maildrop.cc",
    "Referer: https://maildrop.cc/",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
    NULL
};

static void md_random_username(char *buf, int len) {
    const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    srand((unsigned)time(NULL));
    for (int i = 0; i < len; i++) {
        buf[i] = chars[rand() % (sizeof(chars) - 1)];
    }
    buf[len] = '\0';
}

static const char *md_memstri(const char *hay, size_t hay_len, const char *needle, size_t nlen) {
    if (!hay || !needle || nlen == 0 || hay_len < nlen) return NULL;
    for (size_t i = 0; i + nlen <= hay_len; i++) {
        if (MD_STRNICMP(hay + i, needle, (int)nlen) == 0) return hay + i;
    }
    return NULL;
}

static int md_split_mime(const char *raw, const char **hdr, size_t *hdr_len, const char **body, size_t *body_len) {
    if (!raw) return 0;
    size_t n = strlen(raw);
    const char *p = strstr(raw, "\r\n\r\n");
    if (p) {
        *hdr = raw;
        *hdr_len = (size_t)(p - raw);
        *body = p + 4;
        *body_len = n - *hdr_len - 4;
        return 1;
    }
    p = strstr(raw, "\n\n");
    if (p) {
        *hdr = raw;
        *hdr_len = (size_t)(p - raw);
        *body = p + 2;
        *body_len = n - *hdr_len - 2;
        return 1;
    }
    return 0;
}

static int md_boundary_from_hdr(const char *hdr, size_t hdr_len, char *out, size_t cap) {
    const char *b = md_memstri(hdr, hdr_len, "boundary=", 9);
    if (!b) return 0;
    b += 9;
    while (b < hdr + hdr_len && (*b == ' ' || *b == '\t')) b++;
    size_t i = 0;
    if (b < hdr + hdr_len && *b == '"') {
        b++;
        while (b < hdr + hdr_len && *b != '"' && i + 1 < cap) out[i++] = *b++;
    } else {
        while (b < hdr + hdr_len && *b && *b != ';' && !isspace((unsigned char)*b) && i + 1 < cap)
            out[i++] = *b++;
    }
    out[i] = '\0';
    return (int)(i > 0);
}

static char *md_alloc_copy(const char *s, size_t len) {
    char *r = (char *)malloc(len + 1);
    if (!r) return NULL;
    memcpy(r, s, len);
    r[len] = '\0';
    return r;
}

static int md_b64_val(int c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static char *md_b64_decode_alloc(const char *in, size_t len) {
    size_t j = 0;
    unsigned char *clean = (unsigned char *)malloc(len + 1);
    if (!clean) return NULL;
    for (size_t i = 0; i < len; i++) {
        if (!isspace((unsigned char)in[i])) clean[j++] = (unsigned char)in[i];
    }
    clean[j] = 0;
    unsigned char *out = (unsigned char *)malloc((j / 4) * 3 + 4);
    if (!out) {
        free(clean);
        return NULL;
    }
    size_t o = 0;
    for (size_t i = 0; i < j;) {
        int a = md_b64_val(clean[i++]);
        if (a < 0) break;
        if (i >= j) break;
        int b = md_b64_val(clean[i++]);
        if (b < 0) break;
        out[o++] = (unsigned char)((a << 2) | ((b & 0x30) >> 4));
        if (i >= j || clean[i] == '=') {
            if (i < j && clean[i] == '=') i++;
            break;
        }
        int c = md_b64_val(clean[i++]);
        if (c < 0) break;
        out[o++] = (unsigned char)(((b & 0x0f) << 4) | ((c & 0x3c) >> 2));
        if (i >= j || clean[i] == '=') {
            if (i < j && clean[i] == '=') i++;
            break;
        }
        int d = md_b64_val(clean[i++]);
        if (d < 0) break;
        out[o++] = (unsigned char)(((c & 0x03) << 6) | d);
    }
    free(clean);
    out[o] = 0;
    return (char *)out;
}

static char *md_qp_decode_alloc(const char *in, size_t len) {
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    size_t o = 0;
    for (size_t i = 0; i < len; i++) {
        if (in[i] == '=' && i + 2 < len) {
            if (in[i + 1] == '\r' && in[i + 2] == '\n') {
                i += 2;
                continue;
            }
            if (in[i + 1] == '\n') {
                i += 1;
                continue;
            }
            unsigned char h1 = (unsigned char)in[i + 1];
            unsigned char h2 = (unsigned char)in[i + 2];
            int v1 = -1, v2 = -1;
            if (isxdigit(h1) && isxdigit(h2)) {
                v1 = isdigit(h1) ? h1 - '0' : (tolower(h1) - 'a' + 10);
                v2 = isdigit(h2) ? h2 - '0' : (tolower(h2) - 'a' + 10);
                out[o++] = (char)((v1 << 4) | v2);
                i += 2;
                continue;
            }
        }
        out[o++] = in[i];
    }
    out[o] = '\0';
    return out;
}

static int md_hdr_cte_base64(const char *h, size_t hl) {
    return md_memstri(h, hl, "content-transfer-encoding", 25) != NULL
        && md_memstri(h, hl, "base64", 6) != NULL;
}

static int md_hdr_cte_qp(const char *h, size_t hl) {
    return md_memstri(h, hl, "content-transfer-encoding", 25) != NULL
        && md_memstri(h, hl, "quoted-printable", 16) != NULL;
}

static char *md_decode_body_cte(const char *body, size_t blen, const char *ph, size_t phl) {
    if (md_hdr_cte_base64(ph, phl)) {
        char *d = md_b64_decode_alloc(body, blen);
        return d ? d : md_alloc_copy(body, blen);
    }
    if (md_hdr_cte_qp(ph, phl)) {
        char *d = md_qp_decode_alloc(body, blen);
        return d ? d : md_alloc_copy(body, blen);
    }
    return md_alloc_copy(body, blen);
}

static void md_trim_crlf_dash(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\r' || s[n - 1] == '\n' || s[n - 1] == '-' || s[n - 1] == ' '))
        s[--n] = '\0';
}

static char *md_try_part(const char *part, size_t part_len, const char *want_sub, size_t want_len) {
    const char *pe = part + part_len;
    const char *inner_h = part;
    size_t inner_hl = 0;
    const char *inner_b = NULL;
    size_t inner_bl = 0;
    const char *s = strstr(part, "\r\n\r\n");
    if (s && s < pe) {
        inner_hl = (size_t)(s - part);
        inner_b = s + 4;
        inner_bl = (size_t)(pe - inner_b);
    } else {
        s = strstr(part, "\n\n");
        if (!s || s >= pe) return NULL;
        inner_hl = (size_t)(s - part);
        inner_b = s + 2;
        inner_bl = (size_t)(pe - inner_b);
    }
    if (!md_memstri(inner_h, inner_hl, want_sub, want_len)) return NULL;
    char *ph = md_alloc_copy(inner_h, inner_hl);
    char *dec = md_decode_body_cte(inner_b, inner_bl, inner_h, inner_hl);
    free(ph);
    if (!dec) return NULL;
    md_trim_crlf_dash(dec);
    return dec;
}

static char *md_extract_mime_inner(const char *raw, int want_html) {
    const char *hdr;
    size_t hlen;
    const char *body;
    size_t blen;
    if (!md_split_mime(raw, &hdr, &hlen, &body, &blen)) {
        if (want_html) return tm_strdup("");
        return tm_strdup(raw ? raw : "");
    }

    char boundary[256];
    int had_boundary = md_boundary_from_hdr(hdr, hlen, boundary, sizeof(boundary));
    if (had_boundary) {
        char delim[280];
        snprintf(delim, sizeof(delim), "--%s", boundary);
        size_t dlen = strlen(delim);
        const char *end = body + blen;
        const char *cursor = body;
        if ((size_t)(end - cursor) < dlen || memcmp(cursor, delim, dlen) != 0) {
            const char *found = NULL;
            for (const char *p = body; p + dlen <= end; p++) {
                if (memcmp(p, delim, dlen) != 0) continue;
                if (p + dlen + 1 < end && p[dlen] == '-' && p[dlen + 1] == '-') continue;
                found = p;
                break;
            }
            cursor = found ? found : end;
        }
        while (cursor + dlen <= end) {
            if (memcmp(cursor, delim, dlen) != 0) break;
            cursor += dlen;
            if (cursor < end && *cursor == '\r') cursor++;
            if (cursor < end && *cursor == '\n') cursor++;
            const char *next = cursor;
            while (next + dlen <= end) {
                if (memcmp(next, delim, dlen) == 0) {
                    if (next + dlen + 1 < end && next[dlen] == '-' && next[dlen + 1] == '-')
                        next = end;
                    break;
                }
                next++;
            }
            size_t plen = (size_t)(next - cursor);
            char *tmp = md_alloc_copy(cursor, plen);
            if (!tmp) return want_html ? tm_strdup("") : tm_strdup("");
            char *got = md_try_part(tmp, plen, want_html ? "text/html" : "text/plain",
                                     want_html ? 9 : 10);
            free(tmp);
            if (got) return got;
            cursor = next;
        }
    }

    if (want_html) {
        if (md_memstri(hdr, hlen, "content-type:", 13) != NULL
            && md_memstri(hdr, hlen, "text/html", 9) != NULL) {
            char *dec = md_decode_body_cte(body, blen, hdr, hlen);
            if (dec) {
                md_trim_crlf_dash(dec);
                return dec;
            }
        }
        return tm_strdup("");
    }

    if (!had_boundary && (md_hdr_cte_base64(hdr, hlen) || md_hdr_cte_qp(hdr, hlen))) {
        char *dec = md_decode_body_cte(body, blen, hdr, hlen);
        if (dec) {
            md_trim_crlf_dash(dec);
            return dec;
        }
    }
    return md_alloc_copy(body, blen);
}

static char *md_strip_html_alloc(const char *html) {
    if (!html || !*html) return tm_strdup("");
    size_t n = strlen(html);
    char *out = (char *)malloc(n + 2);
    if (!out) return tm_strdup("");
    size_t o = 0;
    int in_tag = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)html[i];
        if (c == '<') in_tag = 1;
        else if (c == '>') {
            in_tag = 0;
            out[o++] = ' ';
        } else if (!in_tag) out[o++] = (char)c;
    }
    out[o] = '\0';
    /* collapse spaces */
    size_t w = 0;
    int sp = 0;
    for (size_t i = 0; i < o; i++) {
        if (isspace((unsigned char)out[i])) {
            if (!sp) {
                out[w++] = ' ';
                sp = 1;
            }
        } else {
            out[w++] = out[i];
            sp = 0;
        }
    }
    out[w] = '\0';
    while (w > 0 && out[w - 1] == ' ') out[--w] = '\0';
    return out;
}

static cJSON *md_graphql(const char *query) {
    cJSON *payload = cJSON_CreateObject();
    cJSON_AddStringToObject(payload, "query", query);
    char *body = cJSON_PrintUnformatted(payload);
    cJSON_Delete(payload);

    tm_http_response_t *resp = tm_http_request(TM_HTTP_POST, MD_GQL, md_headers, body, 15);
    free(body);
    if (!resp || resp->status != 200) {
        tm_http_response_free(resp);
        return NULL;
    }

    cJSON *json = cJSON_Parse(resp->body);
    tm_http_response_free(resp);
    if (!json) return NULL;

    cJSON *errors = cJSON_GetObjectItemCaseSensitive(json, "errors");
    if (cJSON_IsArray(errors) && cJSON_GetArraySize(errors) > 0) {
        cJSON_Delete(json);
        return NULL;
    }

    cJSON *data = cJSON_DetachItemFromObjectCaseSensitive(json, "data");
    cJSON_Delete(json);
    return data;
}

tm_email_info_t *tm_provider_maildrop_generate(void) {
    char username[11];
    md_random_username(username, 10);
    char email[64];
    snprintf(email, sizeof(email), "%s@maildrop.cc", username);

    char query[256];
    snprintf(query, sizeof(query), "{ inbox(mailbox: \"%s\") { id } }", username);
    cJSON *data = md_graphql(query);
    if (!data) return NULL;
    cJSON_Delete(data);

    tm_email_info_t *info = tm_email_info_new();
    info->channel = CHANNEL_MAILDROP;
    info->email = tm_strdup(email);
    info->token = tm_strdup(username);
    return info;
}

tm_email_t *tm_provider_maildrop_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    char query[512];
    snprintf(query, sizeof(query),
             "{ inbox(mailbox: \"%s\") { id headerfrom subject date } }", token);

    cJSON *data = md_graphql(query);
    if (!data) return NULL;

    cJSON *inbox = cJSON_GetObjectItemCaseSensitive(data, "inbox");
    int n = cJSON_IsArray(inbox) ? cJSON_GetArraySize(inbox) : 0;
    *count = 0;
    if (n == 0) {
        cJSON_Delete(data);
        return NULL;
    }

    tm_email_t *emails = tm_emails_new(n);
    int actual = 0;

    for (int i = 0; i < n; i++) {
        cJSON *item = cJSON_GetArrayItem(inbox, i);
        const char *id = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(item, "id"));
        if (!id) continue;

        char msg_query[512];
        snprintf(msg_query, sizeof(msg_query),
                 "{ message(mailbox: \"%s\", id: \"%s\") { id headerfrom subject date data html } }", token,
                 id);
        cJSON *msg_data = md_graphql(msg_query);
        if (!msg_data) continue;

        cJSON *msg = cJSON_GetObjectItemCaseSensitive(msg_data, "message");
        if (msg) {
            const char *raw_data = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(msg, "data"), "");
            char *plain = md_extract_mime_inner(raw_data, 0);
            char *from_mime = md_extract_mime_inner(raw_data, 1);
            const char *api_html = TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(msg, "html"), "");
            char *html_out = NULL;
            if (api_html && api_html[0]) {
                html_out = tm_strdup(api_html);
                if (from_mime) free(from_mime);
            } else {
                html_out = from_mime ? from_mime : tm_strdup("");
            }

            char *text_out;
            if (plain && plain[0]) {
                text_out = plain;
            } else {
                if (plain) free(plain);
                text_out = md_strip_html_alloc(html_out);
            }

            emails[actual].id = tm_strdup(TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(msg, "id"), id));
            emails[actual].from_addr = tm_strdup(TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(msg, "headerfrom"), ""));
            emails[actual].to = tm_strdup(email);
            emails[actual].subject = tm_strdup(TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(msg, "subject"), ""));
            emails[actual].text = text_out ? text_out : tm_strdup("");
            emails[actual].html = html_out ? html_out : tm_strdup("");
            emails[actual].date = tm_strdup(TM_JSON_STR(cJSON_GetObjectItemCaseSensitive(msg, "date"), ""));
            emails[actual].is_read = false;
            emails[actual].attachments = NULL;
            emails[actual].attachment_count = 0;
            actual++;
        }
        cJSON_Delete(msg_data);
    }

    cJSON_Delete(data);
    *count = actual;
    return emails;
}
