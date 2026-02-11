/**
 * 邮件数据标准化模块
 */

#include "tempmail_internal.h"

tm_email_t tm_normalize_email(const cJSON *raw, const char *recipient) {
    tm_email_t email;
    memset(&email, 0, sizeof(email));

    /* ID */
    const char *id_keys[] = {"id", "eid", "_id", "mailboxId", "messageId", "mail_id"};
    email.id = tm_json_get_str(raw, id_keys, 6);

    /* From */
    const char *from_keys[] = {"from_address", "address_from", "from", "messageFrom", "sender"};
    email.from_addr = tm_json_get_str(raw, from_keys, 5);

    /* To */
    const char *to_keys[] = {"to", "to_address", "name_to", "email_address", "address"};
    email.to = tm_json_get_str(raw, to_keys, 5);
    if (!email.to || email.to[0] == '\0') {
        free(email.to);
        email.to = tm_strdup(recipient);
    }

    /* Subject */
    const char *subj_keys[] = {"subject", "e_subject"};
    email.subject = tm_json_get_str(raw, subj_keys, 2);

    /* Text */
    const char *text_keys[] = {"text", "body", "content", "body_text", "text_content"};
    email.text = tm_json_get_str(raw, text_keys, 5);

    /* HTML */
    const char *html_keys[] = {"html", "html_content", "body_html"};
    email.html = tm_json_get_str(raw, html_keys, 3);

    /* Date */
    const char *date_keys[] = {"received_at", "created_at", "createdAt", "date", "timestamp", "e_date"};
    email.date = tm_json_get_str(raw, date_keys, 6);

    /* IsRead */
    email.is_read = false;
    const char *read_keys[] = {"seen", "read", "isRead", "is_read"};
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
