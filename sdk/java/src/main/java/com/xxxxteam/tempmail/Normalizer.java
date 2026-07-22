package com.xxxxteam.tempmail;

import java.time.Instant;
import java.time.OffsetDateTime;
import java.time.ZoneOffset;
import java.time.format.DateTimeFormatter;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.regex.Pattern;

/**
 * 邮件数据标准化模块。
 *
 * <p>不同渠道 API 返回字段名各异，本模块通过多候选字段策略将其统一映射为标准 {@link Email} 结构。
 * provider 先把渠道原始字段塞进 {@link Map}（键沿用渠道原名），再交由本模块归一。</p>
 */
public final class Normalizer {

    private static final Pattern TAG_RE = Pattern.compile("<[^>]+>", Pattern.DOTALL);
    private static final Pattern WS_RE = Pattern.compile("\\s+");

    /** "yyyy-MM-dd HH:mm:ss" 格式化器，预建避免每次解析日期时重复构造。 */
    private static final DateTimeFormatter LOCAL_DT_FMT = DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss");

    /** 日期字段候选键（字符串或秒/毫秒时间戳）。 */
    private static final String[] DATE_KEYS = {"received_at", "receivedAt", "created_at", "createdAt", "date"};

    /** 数值时间戳候选键。 */
    private static final String[] TIMESTAMP_KEYS = {"timestamp", "e_date"};

    /** 布尔已读候选键。 */
    private static final String[] READ_BOOL_KEYS = {"seen", "read", "isRead"};

    /** 数值/字符串已读候选键。 */
    private static final String[] READ_NUM_KEYS = {"is_read", "is_seen"};

    private Normalizer() {
    }

    /**
     * 将渠道原始字段字典标准化为统一 {@link Email} 结构。
     *
     * @param raw            渠道原始字段字典
     * @param recipientEmail 收件人地址（用于补全 to 字段）
     * @return 标准化后的 Email
     */
    public static Email normalizeEmail(Map<String, Object> raw, String recipientEmail) {
        String text = normalizeText(raw);
        String html = normalizeHtml(raw);

        // 修正 text/html 错配
        if (!text.isEmpty() && html.isEmpty() && isHtmlContent(text)) {
            html = text;
            text = "";
        }
        if (text.isEmpty() && !html.isEmpty()) {
            text = htmlToText(html);
        }
        if (!text.isEmpty() && html.isEmpty()) {
            html = textToHtml(text);
        }

        Email email = new Email();
        email.setId(getStr(raw, "id", "eid", "_id", "mailboxId", "messageId", "mail_id"));
        email.setFrom(getStr(raw, "from_addr", "from_address", "fromAddress", "mail_sender",
                "sender", "address_from", "from_email", "from", "messageFrom"));
        email.setTo(normalizeTo(raw, recipientEmail));
        email.setSubject(getStr(raw, "subject", "e_subject", "mail_title"));
        email.setText(text);
        email.setHtml(html);
        email.setDate(normalizeDate(raw));
        email.setRead(normalizeIsRead(raw));
        email.setAttachments(normalizeAttachments(raw.get("attachments")));
        return email;
    }

    /**
     * 多候选字段取字符串，命中第一个非 null 者。
     *
     * @param raw  原始字典
     * @param keys 候选键
     * @return 命中字符串，未命中返回空串
     */
    private static String getStr(Map<String, Object> raw, String... keys) {
        for (String key : keys) {
            Object val = raw.get(key);
            if (val != null) {
                String s = valueToString(val);
                if (s != null) {
                    return s;
                }
            }
        }
        return "";
    }

    /**
     * 原生值转字符串。
     *
     * @param val 原生值
     * @return 字符串表示
     */
    private static String valueToString(Object val) {
        if (val instanceof String) {
            return (String) val;
        }
        if (val instanceof Boolean) {
            return ((Boolean) val) ? "true" : "false";
        }
        if (val instanceof Double) {
            double d = (Double) val;
            if (d == Math.floor(d) && !Double.isInfinite(d)) {
                return Long.toString((long) d);
            }
            return Double.toString(d);
        }
        return String.valueOf(val);
    }

    private static String normalizeTo(Map<String, Object> raw, String recipientEmail) {
        String result = getStr(raw, "to", "to_address", "toAddress", "name_to", "email_address", "address");
        return result.isEmpty() ? (recipientEmail != null ? recipientEmail : "") : result;
    }

    private static String normalizeText(Map<String, Object> raw) {
        return getStr(raw, "text", "text_body", "preview_text", "mail_body_text", "body",
                "content", "body_text", "text_content", "description");
    }

    private static String normalizeHtml(Map<String, Object> raw) {
        return getStr(raw, "html", "html_body", "html_content", "body_html", "mail_body_html");
    }

    private static final DateTimeFormatter ISO = DateTimeFormatter.ISO_OFFSET_DATE_TIME;

    private static String normalizeDate(Map<String, Object> raw) {
        for (String key : DATE_KEYS) {
            Object val = raw.get(key);
            if (val == null) {
                continue;
            }
            if (val instanceof String) {
                String sv = (String) val;
                if (sv.isEmpty()) {
                    continue;
                }
                String parsed = tryParseString(sv);
                return parsed != null ? parsed : sv;
            }
            Double num = tryToDouble(val);
            if (num != null && num > 0) {
                String iso = fromUnix(num);
                if (iso != null) {
                    return iso;
                }
            }
        }

        for (String key : TIMESTAMP_KEYS) {
            Object val = raw.get(key);
            if (val == null) {
                continue;
            }
            Double num = tryToDouble(val);
            if (num != null && num > 0) {
                if ("timestamp".equals(key) && num < 1e12) {
                    String iso = fromUnix(num);
                    return iso != null ? iso : "";
                }
                String iso = fromUnixMillis(num);
                return iso != null ? iso : "";
            }
        }

        return "";
    }

    private static String tryParseString(String sv) {
        String normalized = sv.replace("Z", "+00:00");
        try {
            return OffsetDateTime.parse(normalized).toInstant().atOffset(ZoneOffset.UTC).format(ISO);
        } catch (RuntimeException ignored) {
            // 继续尝试其他格式
        }
        try {
            OffsetDateTime dt = java.time.LocalDateTime.parse(sv, LOCAL_DT_FMT).atOffset(ZoneOffset.UTC);
            return dt.format(ISO);
        } catch (RuntimeException ignored) {
            return null;
        }
    }

    private static String fromUnix(double val) {
        try {
            Instant inst = val > 1e12
                    ? Instant.ofEpochMilli((long) val)
                    : Instant.ofEpochSecond((long) val);
            return inst.atOffset(ZoneOffset.UTC).format(ISO);
        } catch (RuntimeException e) {
            return null;
        }
    }

    private static String fromUnixMillis(double val) {
        try {
            return Instant.ofEpochMilli((long) val).atOffset(ZoneOffset.UTC).format(ISO);
        } catch (RuntimeException e) {
            return null;
        }
    }

    private static boolean normalizeIsRead(Map<String, Object> raw) {
        for (String key : READ_BOOL_KEYS) {
            Object val = raw.get(key);
            if (val instanceof Boolean) {
                return (Boolean) val;
            }
        }
        for (String key : READ_NUM_KEYS) {
            Object val = raw.get(key);
            if (val == null) {
                continue;
            }
            if (val instanceof Boolean) {
                return (Boolean) val;
            }
            if (val instanceof String) {
                return "1".equals(val);
            }
            Double n = tryToDouble(val);
            if (n != null) {
                return (int) (double) n != 0;
            }
        }
        return false;
    }

    @SuppressWarnings("unchecked")
    private static List<EmailAttachment> normalizeAttachments(Object attachments) {
        List<EmailAttachment> result = new ArrayList<>();
        if (!(attachments instanceof List)) {
            return result;
        }
        for (Object item : (List<Object>) attachments) {
            if (!(item instanceof Map)) {
                continue;
            }
            Map<String, Object> a = (Map<String, Object>) item;
            EmailAttachment att = new EmailAttachment();
            att.setFilename(firstStr(a, "filename", "name"));
            att.setSize(firstLong(a, "size", "filesize"));
            att.setContentType(firstStrOrNull(a, "contentType", "content_type", "mimeType", "mime_type"));
            att.setUrl(firstStrOrNull(a, "url", "download_url", "downloadUrl"));
            result.add(att);
        }
        return result;
    }

    private static String firstStr(Map<String, Object> d, String... keys) {
        String s = firstStrOrNull(d, keys);
        return s != null ? s : "";
    }

    private static String firstStrOrNull(Map<String, Object> d, String... keys) {
        for (String k : keys) {
            Object v = d.get(k);
            if (v != null) {
                return valueToString(v);
            }
        }
        return null;
    }

    private static Long firstLong(Map<String, Object> d, String... keys) {
        for (String k : keys) {
            Object v = d.get(k);
            if (v != null) {
                Double n = tryToDouble(v);
                if (n != null) {
                    return (long) (double) n;
                }
            }
        }
        return null;
    }

    private static Double tryToDouble(Object val) {
        if (val instanceof Double) {
            return (Double) val;
        }
        if (val instanceof Float) {
            return ((Float) val).doubleValue();
        }
        if (val instanceof Long) {
            return ((Long) val).doubleValue();
        }
        if (val instanceof Integer) {
            return ((Integer) val).doubleValue();
        }
        if (val instanceof String) {
            try {
                return Double.parseDouble((String) val);
            } catch (NumberFormatException ignored) {
                return null;
            }
        }
        return null;
    }

    /**
     * 检测内容是否为 HTML（只取前 200 字符）。
     *
     * @param content 内容
     * @return 是 HTML 返回 true
     */
    public static boolean isHtmlContent(String content) {
        String prefix = (content.length() > 200 ? content.substring(0, 200) : content).trim().toLowerCase();
        if (prefix.startsWith("<!doctype html") || prefix.startsWith("<html") || prefix.startsWith("<body")) {
            return true;
        }
        String trimmed = content.trim().toLowerCase();
        if (trimmed.contains("<div") && trimmed.contains("</div>")) {
            return true;
        }
        if (trimmed.contains("<table") && trimmed.contains("</table>")) {
            return true;
        }
        return trimmed.contains("<p") && trimmed.contains("</p>") && trimmed.contains("<");
    }

    /**
     * 粗略 HTML 转纯文本：去标签、反转义、压缩空白。
     *
     * @param html HTML 内容
     * @return 纯文本
     */
    public static String htmlToText(String html) {
        if (html == null || html.isEmpty()) {
            return "";
        }
        String noTags = TAG_RE.matcher(html).replaceAll(" ");
        String unescaped = htmlDecode(noTags);
        return WS_RE.matcher(unescaped).replaceAll(" ").trim();
    }

    private static String textToHtml(String text) {
        return "<html><body><pre>" + htmlEncode(text) + "</pre></body></html>";
    }

    private static String htmlDecode(String s) {
        return s.replace("&amp;", "&")
                .replace("&lt;", "<")
                .replace("&gt;", ">")
                .replace("&quot;", "\"")
                .replace("&#39;", "'")
                .replace("&apos;", "'")
                .replace("&nbsp;", " ");
    }

    private static String htmlEncode(String s) {
        return s.replace("&", "&amp;")
                .replace("<", "&lt;")
                .replace(">", "&gt;")
                .replace("\"", "&quot;")
                .replace("'", "&#39;");
    }
}
