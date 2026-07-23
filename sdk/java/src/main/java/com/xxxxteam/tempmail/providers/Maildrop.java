package com.xxxxteam.tempmail.providers;

import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.xxxxteam.tempmail.Email;
import com.xxxxteam.tempmail.EmailInfo;
import com.xxxxteam.tempmail.HttpResult;
import com.xxxxteam.tempmail.HttpClient;
import com.xxxxteam.tempmail.Json;
import com.xxxxteam.tempmail.Normalizer;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ThreadLocalRandom;

/**
 * Maildrop 渠道 — https://maildrop.cx
 *
 * <p>GET /api/suffixes.php 获取域名，GET /api/emails.php 获取邮件。</p>
 */
public final class Maildrop {

    private static final String BASE = "https://maildrop.cx";
    private static final String EXCLUDED = "transformer.edu.kg";

    private Maildrop() {
    }

    private static Map<String, String> headers() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("Accept", "application/json, text/plain, */*");
        h.put("Referer", "https://maildrop.cx/zh-cn/app");
        h.put("User-Agent", "Mozilla/5.0");
        h.put("X-Requested-With", "XMLHttpRequest");
        return h;
    }

    /**
     * 创建 maildrop 临时邮箱。
     *
     * @param domain 首选域名，可为 null
     * @return 邮箱信息
     */
    public static EmailInfo generate(String domain) {
        HttpResult resp = HttpClient.get(BASE + "/api/suffixes.php", headers());
        resp.ensureSuccess();
        JsonElement parsed = Json.parse(resp.getBody());
        if (parsed == null || !parsed.isJsonArray()) {
            throw new RuntimeException("maildrop: 域名响应无效");
        }

        List<String> suffixes = new ArrayList<>();
        for (JsonElement el : parsed.getAsJsonArray()) {
            if (el.isJsonPrimitive()) {
                String s = el.getAsString().trim();
                if (!s.isEmpty() && !s.equalsIgnoreCase(EXCLUDED)) {
                    suffixes.add(s);
                }
            }
        }
        if (suffixes.isEmpty()) throw new RuntimeException("maildrop: 无可用域名");

        String dom;
        if (domain != null && !domain.isBlank()) {
            String p = domain.trim().toLowerCase();
            dom = suffixes.stream().filter(s -> s.equalsIgnoreCase(p)).findFirst()
                    .orElseThrow(() -> new RuntimeException("maildrop: 域名不可用: " + p));
        } else {
            dom = suffixes.get(ThreadLocalRandom.current().nextInt(suffixes.size()));
        }

        String local = ProviderUtil.randomString(10);
        String email = local + "@" + dom;
        return new EmailInfo("maildrop", email, email, null, null);
    }

    /**
     * 通过详情接口获取单封邮件完整内容。
     * GET /api/email_content.php?id={id}
     * 详情响应字段（从前端代码确认）:
     * - content: 完整 HTML 正文
     * - subject / from_addr / date: 邮件元数据
     * - attachment: JSON 字符串数组 [{filename, path, size}]
     *
     * @param id 邮件 ID
     * @return 详情 JsonObject（含 content 等字段），失败返回 null
     */
    private static JsonObject fetchDetail(String id) {
        if (id == null || id.isBlank()) return null;
        try {
            HttpResult resp = HttpClient.get(
                    BASE + "/api/email_content.php?id=" + ProviderUtil.urlEncode(id),
                    headers());
            if (resp.getStatusCode() < 200 || resp.getStatusCode() >= 300) return null;
            return Json.parseObject(resp.getBody());
        } catch (RuntimeException e) {
            return null;
        }
    }

    /**
     * 解析详情接口的 attachment 字段（JSON 字符串）为附件列表。
     *
     * @param raw attachment 字段原始 JSON 字符串
     * @return 附件 Map 列表
     */
    private static List<Map<String, Object>> parseAttachments(String raw) {
        List<Map<String, Object>> out = new ArrayList<>();
        if (raw == null || raw.isBlank()) return out;
        try {
            JsonElement el = Json.parse(raw);
            if (el == null || !el.isJsonArray()) return out;
            for (JsonElement item : el.getAsJsonArray()) {
                if (!item.isJsonObject()) continue;
                JsonObject obj = item.getAsJsonObject();
                String filename = Json.str(obj, "filename").trim();
                if (filename.isEmpty()) continue;
                Map<String, Object> att = new LinkedHashMap<>();
                att.put("filename", filename);
                if (obj.has("size") && obj.get("size").isJsonPrimitive()
                        && obj.get("size").getAsJsonPrimitive().isNumber()) {
                    att.put("size", obj.get("size").getAsLong());
                }
                out.add(att);
            }
        } catch (RuntimeException e) {
            /* 解析失败返回空列表 */
        }
        return out;
    }

    /**
     * 获取邮件列表并对每封邮件补拉详情。
     * 1. GET /api/emails.php 拉取列表（仅含 description 摘要）
     * 2. 对每封邮件 GET /api/email_content.php?id={id} 拉取详情（含 content 完整 HTML）
     * 3. 详情失败时保留列表 description 作为回退
     *
     * @param token 邮箱地址
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        String addr = (email != null && !email.isBlank()) ? email.trim()
                : (token != null ? token.trim() : "");
        if (addr.isEmpty()) throw new RuntimeException("maildrop: 地址为空");

        String url = BASE + "/api/emails.php?addr=" + ProviderUtil.urlEncode(addr)
                + "&page=1&limit=20";
        HttpResult resp = HttpClient.get(url, headers());
        resp.ensureSuccess();

        JsonObject body = Json.parseObject(resp.getBody());
        if (body == null) return new ArrayList<>();
        var rows = Json.arr(body, "emails");
        if (rows == null) return new ArrayList<>();

        List<Email> out = new ArrayList<>();
        for (JsonElement item : rows) {
            if (!item.isJsonObject()) continue;
            JsonObject row = item.getAsJsonObject();

            String id = Json.str(row, "id");
            String desc = Json.str(row, "description");
            String from = Json.str(row, "from_addr");
            String subject = Json.str(row, "subject");
            String date = Json.str(row, "date");
            String html = "";
            List<Map<String, Object>> attachments = new ArrayList<>();

            /* 拉取详情覆盖 html/from/subject/date/attachments */
            if (!id.isEmpty()) {
                JsonObject detail = fetchDetail(id);
                if (detail != null) {
                    String dContent = Json.str(detail, "content");
                    if (!dContent.isBlank()) html = dContent;
                    String dFrom = Json.str(detail, "from_addr");
                    if (!dFrom.isBlank()) from = dFrom;
                    String dSubj = Json.str(detail, "subject");
                    if (!dSubj.isBlank()) subject = dSubj;
                    String dDate = Json.str(detail, "date");
                    if (!dDate.isBlank()) date = dDate;
                    String dAtt = Json.str(detail, "attachment");
                    attachments = parseAttachments(dAtt);
                }
            }

            Map<String, Object> flat = new LinkedHashMap<>();
            flat.put("id", id);
            flat.put("from", from);
            flat.put("to", addr);
            flat.put("subject", subject);
            flat.put("text", desc);
            flat.put("html", html);
            flat.put("date", date);
            flat.put("attachments", attachments);
            out.add(Normalizer.normalizeEmail(flat, addr));
        }
        return out;
    }
}
