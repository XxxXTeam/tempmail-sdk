/**
 * anonbox.net — GET /en/ 解析页面；mbox 明文收信
 */
#include "tempmail_internal.h"
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif
#endif

#define AB_PAGE "https://anonbox.net/en/"
#define AB_BASE "https://anonbox.net"

static const char *ab_headers_html[] = {
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
    "Accept: text/html,application/xhtml+xml",
    NULL
};

static const char *ab_headers_plain[] = {
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
    "Accept: text/plain,*/*",
    NULL
};

static int region_has(const char *start, const char *end, const char *sub) {
    size_t len = (size_t)(end - start);
    size_t sl = strlen(sub);
    if (sl == 0 || sl > len) return 0;
    for (size_t i = 0; i + sl <= len; i++) {
        if (memcmp(start + i, sub, sl) == 0) return 1;
    }
    return 0;
}

static void strip_tags_to(const char *in, char *out, size_t cap) {
    size_t o = 0;
    while (*in && o + 1 < cap) {
        if (*in == '<') {
            while (*in && *in != '>') in++;
            if (*in == '>') in++;
            continue;
        }
        out[o++] = *in++;
    }
    out[o] = '\0';
    /* minimal entities */
    char *p = out;
    while ((p = strstr(p, "&nbsp;")) != NULL) {
        memmove(p + 1, p + 6, strlen(p + 6) + 1);
        *p = ' ';
    }
}

static int parse_mail_link(const char *html, char *inbox, size_t inbox_cap, char *secret, size_t secret_cap) {
    const char *needle = "href=\"https://anonbox.net/";
    const char *p = strstr(html, needle);
    if (!p) return -1;
    p += strlen(needle);
    size_t ii = 0;
    while (*p && *p != '/' && ii + 1 < inbox_cap) {
        inbox[ii++] = *p++;
    }
    inbox[ii] = '\0';
    if (*p != '/') return -1;
    p++;
    ii = 0;
    while (*p && *p != '\"' && *p != '?' && ii + 1 < secret_cap) {
        secret[ii++] = *p++;
    }
    secret[ii] = '\0';
    return (inbox[0] && secret[0]) ? 0 : -1;
}

static int find_address_local(const char *html, char *local_out, size_t local_cap) {
    const char *region_start = strstr(html, "Your one-day-email-address is:");
    if (!region_start) return -1;
    const char *region_end = strstr(region_start, "You can check your mail here:");
    if (!region_end) region_end = html + strlen(html);

    for (const char *p = region_start; p < region_end && (p = strstr(p, "<dd")) != NULL;) {
        const char *gt = strchr(p, '>');
        if (!gt || gt > region_end) break;
        if (region_has(p, gt, "display") && region_has(p, gt, "none")) {
            p = gt + 1;
            continue;
        }
        const char *close = strstr(gt, "</dd>");
        if (!close || close > region_end) break;
        const char *pb = strstr(gt, "<p>");
        if (!pb || pb > close) {
            p = close + 5;
            continue;
        }
        pb += 3;
        const char *pe = strstr(pb, "</p>");
        if (!pe || pe > close) {
            p = close + 5;
            continue;
        }
        char buf[2048];
        size_t chunk = (size_t)(pe - pb);
        if (chunk >= sizeof(buf)) chunk = sizeof(buf) - 1;
        memcpy(buf, pb, chunk);
        buf[chunk] = '\0';
        if (!strstr(buf, "@")) {
            p = close + 5;
            continue;
        }
        if (strstr(buf, "googlemail.com") != NULL) {
            p = close + 5;
            continue;
        }
        if (strstr(buf, "anonbox") == NULL && strstr(buf, "Anonbox") == NULL) {
            p = close + 5;
            continue;
        }
        char stripped[1024];
        strip_tags_to(buf, stripped, sizeof(stripped));
        const char *at = strchr(stripped, '@');
        if (!at) return -1;
        size_t loc_len = (size_t)(at - stripped);
        while (loc_len > 0 && isspace((unsigned char)stripped[loc_len - 1])) loc_len--;
        if (loc_len == 0 || loc_len >= local_cap) return -1;
        memcpy(local_out, stripped, loc_len);
        local_out[loc_len] = '\0';
        return 0;
    }
    return -1;
}

static void u32_to_base36(uint32_t n, char *out, size_t cap) {
    static const char dig[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    if (cap < 2) {
        if (cap == 1) out[0] = '\0';
        return;
    }
    if (n == 0) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }
    char tmp[16];
    int ti = 0;
    while (n > 0 && ti < (int)sizeof(tmp)) {
        tmp[ti++] = dig[n % 36];
        n /= 36;
    }
    size_t o = 0;
    while (ti > 0 && o + 1 < cap) {
        out[o++] = tmp[--ti];
    }
    out[o] = '\0';
}

static void simple_hash(const char *s, size_t max_len, char *out, size_t cap) {
    uint32_t h = 0;
    for (size_t i = 0; s[i] && i < max_len; i++) {
        h = h * 31u + (unsigned char)s[i];
    }
    u32_to_base36(h, out, cap);
}

static void qp_decode(const char *body, const char *hdrs, char *out, size_t cap) {
    const char *enc = strstr(hdrs, "Content-Transfer-Encoding");
    if (!enc) enc = strstr(hdrs, "content-transfer-encoding");
    int is_qp = 0;
    if (enc) {
        if (strstr(enc, "quoted-printable") != NULL || strstr(enc, "Quoted-Printable") != NULL)
            is_qp = 1;
    }
    if (!is_qp) {
        size_t n = strlen(body);
        if (n >= cap) n = cap - 1;
        memcpy(out, body, n);
        out[n] = '\0';
        return;
    }
    size_t o = 0;
    const char *p = body;
    while (*p && o + 1 < cap) {
        if (p[0] == '=' && (p[1] == '\r' || p[1] == '\n')) {
            if (p[1] == '\r' && p[2] == '\n') p += 3;
            else p += 2;
            continue;
        }
        if (p[0] == '=' && isxdigit((unsigned char)p[1]) && isxdigit((unsigned char)p[2])) {
            char hx[3] = { p[1], p[2], 0 };
            out[o++] = (char)strtoul(hx, NULL, 16);
            p += 3;
            continue;
        }
        out[o++] = *p++;
    }
    out[o] = '\0';
}

static cJSON *mbox_block_to_json(const char *block, const char *recipient) {
    const char *s = block;
    if (strncmp(s, "From ", 5) == 0) {
        s = strchr(s, '\n');
        if (!s) return NULL;
        s++;
    }
    char headers[16384];
    size_t hi = 0;
    while (*s && hi + 2 < sizeof(headers)) {
        if (s[0] == '\r') s++;
        if (s[0] == '\n') {
            s++;
            break;
        }
        const char *ln = s;
        while (*ln && *ln != '\n' && *ln != '\r' && hi + 1 < sizeof(headers)) {
            headers[hi++] = *ln++;
        }
        headers[hi++] = '\n';
        if (*ln == '\r') ln++;
        if (*ln == '\n') ln++;
        s = ln;
    }
    headers[hi] = '\0';
    const char *body = s;

    char low_hdr[16384];
    size_t i;
    for (i = 0; headers[i] && i < sizeof(low_hdr) - 1; i++) {
        low_hdr[i] = (char)tolower((unsigned char)headers[i]);
    }
    low_hdr[i] = '\0';

    char from[512] = "", to[512] = "", subj[512] = "", date[256] = "", ctype[512] = "";
    const char *line = headers;
    while (*line) {
        const char *nl = strchr(line, '\n');
        size_t len = nl ? (size_t)(nl - line) : strlen(line);
        if (len >= sizeof(from) - 1) len = sizeof(from) - 2;
        char key[64];
        const char *colon = memchr(line, ':', len);
        if (colon) {
            size_t kl = (size_t)(colon - line);
            if (kl < sizeof(key)) {
                memcpy(key, line, kl);
                key[kl] = '\0';
                for (size_t j = 0; key[j]; j++) key[j] = (char)tolower((unsigned char)key[j]);
                const char *val = colon + 1;
                while (*val == ' ' || *val == '\t') val++;
                char vbuf[512];
                size_t vl = len - (size_t)(colon + 1 - line);
                if (vl >= sizeof(vbuf)) vl = sizeof(vbuf) - 1;
                memcpy(vbuf, val, vl);
                vbuf[vl] = '\0';
                if (strcmp(key, "from") == 0) strncpy(from, vbuf, sizeof(from) - 1);
                else if (strcmp(key, "to") == 0) strncpy(to, vbuf, sizeof(to) - 1);
                else if (strcmp(key, "subject") == 0) strncpy(subj, vbuf, sizeof(subj) - 1);
                else if (strcmp(key, "date") == 0) strncpy(date, vbuf, sizeof(date) - 1);
                else if (strcmp(key, "content-type") == 0) strncpy(ctype, vbuf, sizeof(ctype) - 1);
            }
        }
        line = nl ? nl + 1 : line + len;
    }

    char text[65536] = "";
    char html[65536] = "";
    int has_mp = strstr(low_hdr, "multipart/") != NULL;
    if (has_mp) {
        const char *bnd = strstr(ctype, "boundary=");
        if (bnd) {
            bnd += 9;
            while (*bnd == ' ' || *bnd == '\"') bnd++;
            char bound[128];
            size_t bi = 0;
            while (*bnd && *bnd != '\"' && *bnd != ';' && *bnd != ' ' && bi + 1 < sizeof(bound)) {
                bound[bi++] = *bnd++;
            }
            bound[bi] = '\0';
            if (bi > 0) {
                char delim[160];
                snprintf(delim, sizeof(delim), "\n--%s", bound);
                const char *part = body;
                for (;;) {
                    const char *d = strstr(part, delim);
                    if (!d) break;
                    part = d + strlen(delim);
                    if (part[0] == '-' && part[1] == '-') break;
                    if (*part == '\r') part++;
                    if (*part == '\n') part++;
                    const char *sep = strstr(part, "\n\n");
                    if (!sep) sep = strstr(part, "\r\n\r\n");
                    if (!sep) break;
                    const char *ph = part;
                    size_t ph_len = (size_t)(sep - ph);
                    char ph_buf[4096];
                    if (ph_len >= sizeof(ph_buf)) ph_len = sizeof(ph_buf) - 1;
                    memcpy(ph_buf, ph, ph_len);
                    ph_buf[ph_len] = '\0';
                    const char *pb = sep;
                    if (pb[0] == '\r') pb++;
                    if (pb[0] == '\n') pb++;
                    char pct[64] = "";
                    const char *ctl = ph_buf;
                    while (*ctl) {
                        const char *cnl = strchr(ctl, '\n');
                        size_t cl = cnl ? (size_t)(cnl - ctl) : strlen(ctl);
                        if (cl > 12 && strncasecmp(ctl, "content-type:", 13) == 0) {
                            const char *v = ctl + 13;
                            while (*v == ' ' || *v == '\t') v++;
                            size_t pi = 0;
                            while (*v && *v != ';' && *v != ' ' && pi + 1 < sizeof(pct)) {
                                pct[pi++] = (char)tolower((unsigned char)*v++);
                            }
                            pct[pi] = '\0';
                            break;
                        }
                        ctl = cnl ? cnl + 1 : ctl + cl;
                    }
                    char dec[32768];
                    qp_decode(pb, ph_buf, dec, sizeof(dec));
                    if (strcmp(pct, "text/plain") == 0 && text[0] == '\0') strncpy(text, dec, sizeof(text) - 1);
                    else if (strcmp(pct, "text/html") == 0 && html[0] == '\0') strncpy(html, dec, sizeof(html) - 1);
                    part = pb;
                }
            }
        }
    }
    if (text[0] == '\0' && html[0] == '\0') {
        char dec[65536];
        qp_decode(body, headers, dec, sizeof(dec));
        if (strstr(low_hdr, "text/html") != NULL) strncpy(html, dec, sizeof(html) - 1);
        else strncpy(text, dec, sizeof(text) - 1);
    }

    if (to[0] == '\0') strncpy(to, recipient, sizeof(to) - 1);
    char idbuf[512];
    idbuf[0] = '\0';
    const char *mid = strstr(headers, "Message-ID:");
    if (!mid) mid = strstr(headers, "Message-Id:");
    if (mid) {
        mid = strchr(mid, ':');
        if (mid) {
            mid++;
            while (*mid == ' ' || *mid == '\t') mid++;
            strncpy(idbuf, mid, sizeof(idbuf) - 1);
            idbuf[sizeof(idbuf) - 1] = '\0';
            char *nl = strpbrk(idbuf, "\r\n");
            if (nl) *nl = '\0';
        }
    }
    if (idbuf[0] == '\0') {
        size_t bl = strlen(block);
        if (bl > 512) bl = 512;
        simple_hash(block, bl, idbuf, sizeof(idbuf));
    }

    cJSON *o = cJSON_CreateObject();
    if (!o) return NULL;
    cJSON_AddStringToObject(o, "id", idbuf[0] ? idbuf : "0");
    cJSON_AddStringToObject(o, "from", from);
    cJSON_AddStringToObject(o, "to", to);
    cJSON_AddStringToObject(o, "subject", subj);
    cJSON_AddStringToObject(o, "body_text", text);
    cJSON_AddStringToObject(o, "body_html", html);
    cJSON_AddStringToObject(o, "date", date[0] ? date : "");
    cJSON_AddBoolToObject(o, "isRead", 0);
    cJSON_AddItemToObject(o, "attachments", cJSON_CreateArray());
    return o;
}

static char *find_expires(const char *html) {
    const char *p = strstr(html, "Your mail address is valid until:</dt>");
    if (!p) return NULL;
    p = strstr(p, "<dd><p>");
    if (!p) return NULL;
    p += 7;
    const char *e = strstr(p, "</p>");
    if (!e) return NULL;
    size_t n = (size_t)(e - p);
    char *out = (char*)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, p, n);
    out[n] = '\0';
    return out;
}

tm_email_info_t* tm_provider_anonbox_generate(void) {
    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, AB_PAGE, ab_headers_html, NULL, 15);
    if (!resp || resp->status != 200) {
        tm_http_response_free(resp);
        return NULL;
    }
    char inbox[64], secret[64], local[256];
    if (parse_mail_link(resp->body, inbox, sizeof(inbox), secret, sizeof(secret)) != 0) {
        tm_http_response_free(resp);
        return NULL;
    }
    if (find_address_local(resp->body, local, sizeof(local)) != 0) {
        tm_http_response_free(resp);
        return NULL;
    }
    char *exp = find_expires(resp->body);
    tm_http_response_free(resp);

    tm_email_info_t *info = tm_email_info_new();
    if (!info) {
        free(exp);
        return NULL;
    }
    info->channel = CHANNEL_ANONBOX;
    size_t el = strlen(local) + strlen(inbox) + 32;
    info->email = (char*)malloc(el);
    if (!info->email) {
        free(exp);
        free(info);
        return NULL;
    }
    snprintf(info->email, el, "%s@%s.anonbox.net", local, inbox);
    size_t tl = strlen(inbox) + strlen(secret) + 4;
    info->token = (char*)malloc(tl);
    if (!info->token) {
        free(info->email);
        free(exp);
        free(info);
        return NULL;
    }
    snprintf(info->token, tl, "%s/%s", inbox, secret);
    info->expires_at = 0;
    info->created_at = exp;
    return info;
}

tm_email_t* tm_provider_anonbox_get_emails(const char *token, const char *email, int *count) {
    *count = -1;
    if (!token || !email) return NULL;

    char url[512];
    if (strchr(token, '/')) {
        if (token[strlen(token) - 1] == '/')
            snprintf(url, sizeof(url), AB_BASE "/%s", token);
        else
            snprintf(url, sizeof(url), AB_BASE "/%s/", token);
    } else {
        snprintf(url, sizeof(url), AB_BASE "/%s/", token);
    }

    tm_http_response_t *resp = tm_http_request(TM_HTTP_GET, url, ab_headers_plain, NULL, 15);
    if (!resp) return NULL;
    if (resp->status == 404) {
        tm_http_response_free(resp);
        *count = 0;
        return NULL;
    }
    if (resp->status != 200) {
        tm_http_response_free(resp);
        return NULL;
    }
    if (!resp->body || resp->size == 0) {
        tm_http_response_free(resp);
        *count = 0;
        return NULL;
    }

    char *raw = tm_strdup(resp->body);
    tm_http_response_free(resp);
    if (!raw) return NULL;

    int nblk = 0;
    for (char *p = raw; *p; ) {
        char *q = strstr(p, "\nFrom ");
        if (!q) {
            nblk++;
            break;
        }
        nblk++;
        p = q + 1;
    }

    cJSON **blocks = (cJSON**)calloc((size_t)nblk, sizeof(cJSON*));
    if (!blocks) {
        free(raw);
        return NULL;
    }
    int nb = 0;
    char *p = raw;
    while (*p) {
        char *q = strstr(p, "\nFrom ");
        char save = 0;
        if (q) {
            save = *q;
            *q = '\0';
        }
        while (*p == '\n' || *p == '\r' || *p == ' ') p++;
        if (*p) {
            cJSON *j = mbox_block_to_json(p, email);
            if (j) blocks[nb++] = j;
        }
        if (!q) break;
        *q = save;
        p = q + 1;
    }
    free(raw);

    if (nb == 0) {
        free(blocks);
        *count = 0;
        return NULL;
    }

    tm_email_t *emails = tm_emails_new(nb);
    if (!emails) {
        for (int i = 0; i < nb; i++) cJSON_Delete(blocks[i]);
        free(blocks);
        return NULL;
    }
    for (int i = 0; i < nb; i++) {
        emails[i] = tm_normalize_email(blocks[i], email);
        cJSON_Delete(blocks[i]);
    }
    free(blocks);
    *count = nb;
    return emails;
}
