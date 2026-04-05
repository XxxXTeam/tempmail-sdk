/**
 * 邮件数据标准化模块
 */

#include "tempmail_internal.h"
#include <time.h>

/* 将 received_at 等字段（字符串或毫秒/秒时间戳）格式化为 RFC3339 UTC */
static char *tm_normalize_date_json(const cJSON *raw) {
    const char *k1[] = {"received_at", "receivedAt", "created_at", "createdAt", "date"};
    for (int i = 0; i < 5; i++) {
        const cJSON *item = cJSON_GetObjectItemCaseSensitive(raw, k1[i]);
        if (!item) continue;
        if (cJSON_IsString(item) && item->valuestring && item->valuestring[0] != '\0') {
            return tm_strdup(item->valuestring);
        }
        if (cJSON_IsNumber(item) && item->valuedouble > 0) {
            double v = item->valuedouble;
            time_t sec = (v > 1e12) ? (time_t)(v / 1000.0 + 0.5) : (time_t)(v + 0.5);
            char buf[48];
            struct tm tmu;
#ifdef _WIN32
            if (gmtime_s(&tmu, &sec) != 0) memset(&tmu, 0, sizeof(tmu));
#else
            gmtime_r(&sec, &tmu);
#endif
            strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmu);
            return tm_strdup(buf);
        }
    }
    const cJSON *ts = cJSON_GetObjectItemCaseSensitive(raw, "timestamp");
    if (cJSON_IsNumber(ts) && ts->valuedouble > 0) {
        double v = ts->valuedouble;
        time_t sec = (v < 1e12) ? (time_t)(v + 0.5) : (time_t)(v / 1000.0 + 0.5);
        char buf[48];
        struct tm tmu;
#ifdef _WIN32
        if (gmtime_s(&tmu, &sec) != 0) memset(&tmu, 0, sizeof(tmu));
#else
        gmtime_r(&sec, &tmu);
#endif
        strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmu);
        return tm_strdup(buf);
    }
    const cJSON *ed = cJSON_GetObjectItemCaseSensitive(raw, "e_date");
    if (cJSON_IsNumber(ed) && ed->valuedouble > 0) {
        time_t sec = (time_t)(ed->valuedouble / 1000.0 + 0.5);
        char buf[48];
        struct tm tmu;
#ifdef _WIN32
        if (gmtime_s(&tmu, &sec) != 0) memset(&tmu, 0, sizeof(tmu));
#else
        gmtime_r(&sec, &tmu);
#endif
        strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmu);
        return tm_strdup(buf);
    }
    return tm_strdup("");
}

tm_email_t tm_normalize_email(const cJSON *raw, const char *recipient) {
    tm_email_t email;
    memset(&email, 0, sizeof(email));

    /* ID */
    const char *id_keys[] = {"id", "eid", "_id", "mailboxId", "messageId", "mail_id"};
    email.id = tm_json_get_str(raw, id_keys, 6);

    /* From */
    const char *from_keys[] = {"from_addr", "from_address", "fromAddress", "mail_sender", "sender", "address_from", "from_email", "from", "messageFrom"};
    email.from_addr = tm_json_get_str(raw, from_keys, 9);

    /* To */
    const char *to_keys[] = {"to", "to_address", "toAddress", "name_to", "email_address", "address"};
    email.to = tm_json_get_str(raw, to_keys, 6);
    if (!email.to || email.to[0] == '\0') {
        free(email.to);
        email.to = tm_strdup(recipient);
    }

    /* Subject */
    const char *subj_keys[] = {"subject", "e_subject", "mail_title"};
    email.subject = tm_json_get_str(raw, subj_keys, 3);

    /* Text */
    const char *text_keys[] = {"text", "text_body", "preview_text", "mail_body_text", "body", "content", "body_text", "text_content", "description"};
    email.text = tm_json_get_str(raw, text_keys, 9);

    /* HTML */
    const char *html_keys[] = {"html", "html_body", "html_content", "body_html", "mail_body_html"};
    email.html = tm_json_get_str(raw, html_keys, 5);

    /*
     * 修正 text/html 错配：部分渠道将 HTML 放在 body/text 字段中
     * 检测 text 是否包含 HTML 标签，如果是则移动到 html 字段
     */
    if (email.text && email.text[0] != '\0' &&
        (!email.html || email.html[0] == '\0')) {
        const char *lower = email.text;
        int is_html = 0;
        if (strstr(lower, "<!DOCTYPE") || strstr(lower, "<!doctype") ||
            strstr(lower, "<html") || strstr(lower, "<HTML") ||
            strstr(lower, "<body") || strstr(lower, "<BODY") ||
            (strstr(lower, "<div") && strstr(lower, "</div>")) ||
            (strstr(lower, "<table") && strstr(lower, "</table>")) ||
            (strstr(lower, "<p") && strstr(lower, "</p>"))) {
            is_html = 1;
        }
        if (is_html) {
            free(email.html);
            email.html = email.text;
            email.text = tm_strdup("");
        }
    }

    /* Date（含数字型毫秒时间戳，如 ta-easy received_at） */
    email.date = tm_normalize_date_json(raw);

    /* IsRead */
    email.is_read = false;
    const char *read_keys[] = {"seen", "read", "isRead", "is_read", "is_seen"};
    for (int i = 0; i < 4; i++) {
        const cJSON *item = cJSON_GetObjectItemCaseSensitive(raw, read_keys[i]);
        if (cJSON_IsBool(item)) {
            email.is_read = cJSON_IsTrue(item);
            break;
        }
        if (cJSON_IsNumber(item)) {
            email.is_read = item->valueint != 0;
            break;
        }
    }

    /* Attachments */
    email.attachments = NULL;
    email.attachment_count = 0;
    const cJSON *att_arr = cJSON_GetObjectItemCaseSensitive(raw, "attachments");
    if (cJSON_IsArray(att_arr)) {
        int count = cJSON_GetArraySize(att_arr);
        if (count > 0) {
            email.attachments = (tm_attachment_t*)calloc(count, sizeof(tm_attachment_t));
            if (email.attachments) {
                email.attachment_count = count;
                int idx = 0;
                const cJSON *a;
                cJSON_ArrayForEach(a, att_arr) {
                    const char *fn_keys[] = {"filename", "name"};
                    email.attachments[idx].filename = tm_json_get_str(a, fn_keys, 2);

                    const cJSON *sz = cJSON_GetObjectItemCaseSensitive(a, "size");
                    if (!sz) sz = cJSON_GetObjectItemCaseSensitive(a, "filesize");
                    email.attachments[idx].size = sz && cJSON_IsNumber(sz) ? (long)sz->valuedouble : -1;

                    const char *ct_keys[] = {"contentType", "content_type", "mimeType", "mime_type"};
                    email.attachments[idx].content_type = tm_json_get_str(a, ct_keys, 4);

                    const char *url_keys[] = {"url", "download_url", "downloadUrl"};
                    email.attachments[idx].url = tm_json_get_str(a, url_keys, 3);
                    idx++;
                }
            }
        }
    }

    return email;
}
