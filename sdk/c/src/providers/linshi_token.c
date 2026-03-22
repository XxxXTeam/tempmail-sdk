#include "tm_sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int pick(int n) {
    if (n <= 0) return 0;
    return rand() % n;
}

static const char *UA[] = {
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/121.0.0.0 Safari/537.36 Edg/121.0.0.0",
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/119.0.0.0 Safari/537.36",
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:122.0) Gecko/20100101 Firefox/122.0",
};
static const char *PLAT[] = { "Win32", "MacIntel", "Linux x86_64" };
static const char *LANG[] = { "zh-CN", "en-US", "en-GB", "ja-JP" };
static const char *LANGS[] = { "zh-CN,zh,en", "en-US,en", "ja-JP,ja,en" };
static const char *TZ[] = { "Asia/Shanghai", "America/New_York", "Europe/London", "UTC" };
static const int MEM[] = { 4, 8, 16 };
static const int DEPTH[] = { 24, 30 };
static const double RATIO[] = { 1.0, 1.25, 1.5, 2.0 };
static const int W0[] = { 1920, 2560, 1680, 1536, 1366 };
static const int H0[] = { 1080, 1440, 1050, 864, 768 };

static void fmt_ratio(double r, char *buf, size_t cap) {
    if (r == 1.0 || r == 2.0) snprintf(buf, cap, "%.0f", r);
    else snprintf(buf, cap, "%g", r);
}

void tm_linshi_derive_path_key(const char *visitor_id, char *out, size_t cap) {
    size_t n;
    char e[96];
    int t, i;
    char ts0, ts1;
    if (!out || cap < 2) return;
    out[0] = 0;
    if (!visitor_id) return;
    n = strlen(visitor_id);
    if (n < 4 || n - 2 >= sizeof(e)) return;
    memcpy(e, visitor_id, n - 2);
    e[n - 2] = 0;
    t = 0;
    for (i = 0; e[i]; i++) t += (unsigned char)e[i] % 5;
    if (t < 10) t = 10;
    if (t >= 100) t = 99;
    ts0 = (char)('0' + t / 10);
    ts1 = (char)('0' + t % 10);
    /* insert at 3 and 12 in e */
    if (strlen(e) + 2 >= sizeof(e)) return;
    memmove(e + 4, e + 3, strlen(e + 3) + 1);
    e[3] = ts0;
    memmove(e + 13, e + 12, strlen(e + 12) + 1);
    e[12] = ts1;
    snprintf(out, cap, "%s", e);
}

int tm_linshi_random_api_path_key(char *out, size_t cap) {
    char json[1024];
    char rbuf[24];
    unsigned char salt[16];
    unsigned char hash[32];
    unsigned char *combined;
    size_t jl;
    char vid[36];
    int w, h, ah, i;
    double pr;

    if (!out || cap < 8) return -1;

    w = W0[pick((int)(sizeof(W0) / sizeof(W0[0])))] + pick(81);
    h = H0[pick((int)(sizeof(H0) / sizeof(H0[0])))] + pick(41);
    ah = h - 24 - pick(97);
    if (ah < 0) ah = h;
    pr = RATIO[pick(4)];
    fmt_ratio(pr, rbuf, sizeof(rbuf));

    if (snprintf(json, sizeof(json),
        "{\"colorDepth\":%d,\"deviceMemory\":%d,\"hardwareConcurrency\":%d,"
        "\"language\":\"%s\",\"languages\":\"%s\",\"pixelRatio\":%s,"
        "\"platform\":\"%s\",\"screen\":{\"width\":%d,\"height\":%d,\"availWidth\":%d,\"availHeight\":%d},"
        "\"timezone\":\"%s\",\"userAgent\":\"%s\"}",
        DEPTH[pick(2)], MEM[pick(3)], 4 + pick(29),
        LANG[pick(4)], LANGS[pick(3)], rbuf,
        PLAT[pick(3)], w, h, w, ah,
        TZ[pick(4)], UA[pick(5)]) >= (int)sizeof(json))
        return -1;

    for (i = 0; i < 16; i++) salt[i] = (unsigned char)(rand() & 0xff);

    jl = strlen(json);
    combined = (unsigned char *)malloc(jl + 16);
    if (!combined) return -1;
    memcpy(combined, json, jl);
    memcpy(combined + jl, salt, 16);
    tm_sha256(combined, jl + 16, hash);
    free(combined);

    for (i = 0; i < 16; i++)
        snprintf(vid + i * 2, 3, "%02x", hash[i]);
    vid[32] = 0;

    tm_linshi_derive_path_key(vid, out, cap);
    return 0;
}
