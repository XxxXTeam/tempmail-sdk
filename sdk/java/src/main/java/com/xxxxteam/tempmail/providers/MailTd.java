package com.xxxxteam.tempmail.providers;

import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.xxxxteam.tempmail.Email;
import com.xxxxteam.tempmail.EmailInfo;
import com.xxxxteam.tempmail.HttpResult;
import com.xxxxteam.tempmail.HttpClient;
import com.xxxxteam.tempmail.Json;
import com.xxxxteam.tempmail.Normalizer;

import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ThreadLocalRandom;

/**
 * Mail.TD 渠道 — https://mail.td
 *
 * <p>使用 SHA-256 Proof-of-Work 创建账户。
 * GET /domains → POST /accounts → GET /accounts/{id}/messages。</p>
 */
public final class MailTd {

    private static final String BASE_URL = "https://api.mail.td/api";
    private static final int INITIAL_DIFFICULTY = 15;
    private static final int MAX_DIFFICULTY = 24;
    private static final int MAX_RETRIES = 5;

    private MailTd() {
    }

    private static Map<String, String> headers() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("Content-Type", "application/json");
        h.put("Accept", "application/json");
        h.put("User-Agent", "Mozilla/5.0");
        return h;
    }

    private static int leadingZeroBits(byte[] digest) {
        int bits = 0;
        for (byte b : digest) {
            if (b == 0) { bits += 8; continue; }
            int unsigned = b & 0xFF;
            for (int i = 7; i >= 0; i--) {
                if (((unsigned >> i) & 1) == 1) return bits;
                bits++;
            }
            return bits;
        }
        return bits;
    }

    private static String solvePow(String address, long timestamp, int difficulty) {
        String prefix = address.toLowerCase().trim() + timestamp;
        long nonce = 0;
        try {
            MessageDigest md = MessageDigest.getInstance("SHA-256");
            while (true) {
                String candidate = prefix + nonce;
                byte[] digest = md.digest(candidate.getBytes(StandardCharsets.UTF_8));
                md.reset();
                if (leadingZeroBits(digest) >= difficulty) return String.valueOf(nonce);
                nonce++;
            }
        } catch (Exception e) {
            throw new RuntimeException("mail-td: PoW 计算失败", e);
        }
    }

    /**
     * 创建 mail.td 临时邮箱（含 PoW 求解）。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        // 获取域名
        HttpResult domResp = HttpClient.get(BASE_URL + "/domains", headers());
        domResp.ensureSuccess();
        JsonObject domBody = Json.parseObject(domResp.getBody());
        if (domBody == null) throw new RuntimeException("mail-td: 域名响应格式无效");
        var domArr = Json.arr(domBody, "domains");
        if (domArr == null) throw new RuntimeException("mail-td: 未获取到域名列表");

        List<String> freeDomains = new ArrayList<>();
        for (JsonElement el : domArr) {
            if (!el.isJsonObject()) continue;
            JsonObject d = el.getAsJsonObject();
            if (d.has("pro_only") && d.get("pro_only").getAsBoolean()) continue;
            String dn = Json.str(d, "domain");
            if (!dn.isEmpty()) freeDomains.add(dn);
        }
        if (freeDomains.isEmpty()) throw new RuntimeException("mail-td: 无可用域名");

        String domain = freeDomains.get(ThreadLocalRandom.current().nextInt(freeDomains.size()));
        String address = ProviderUtil.randomString(12) + "@" + domain;

        // 生成密码并计算 auth_key
        String password = ProviderUtil.randomString(16);
        String authKey;
        try {
            MessageDigest md = MessageDigest.getInstance("SHA-256");
            byte[] hash = md.digest(password.getBytes(StandardCharsets.UTF_8));
            StringBuilder hex = new StringBuilder();
            for (byte b : hash) hex.append(String.format("%02x", b & 0xFF));
            authKey = hex.toString();
        } catch (Exception e) {
            throw new RuntimeException("mail-td: SHA-256 计算失败", e);
        }

        int difficulty = INITIAL_DIFFICULTY;
        for (int i = 0; i <= MAX_RETRIES; i++) {
            if (difficulty > MAX_DIFFICULTY) {
                throw new RuntimeException("mail-td: PoW 难度超出上限");
            }
            long timestamp = System.currentTimeMillis() / 1000;
            String nonce = solvePow(address, timestamp, difficulty);

            Map<String, Object> powObj = new LinkedHashMap<>();
            powObj.put("t", timestamp);
            powObj.put("n", nonce);
            powObj.put("d", difficulty);
            Map<String, Object> reqBody = new LinkedHashMap<>();
            reqBody.put("address", address);
            reqBody.put("auth_key", authKey);
            reqBody.put("pow", powObj);

            HttpResult resp = HttpClient.post(BASE_URL + "/accounts",
                    Json.serialize(reqBody), "application/json", headers());

            JsonObject data = Json.parseObject(resp.getBody());
            if (data == null) throw new RuntimeException("mail-td: 创建账户响应格式无效");

            if ("retry".equals(Json.str(data, "status"))) {
                if (data.has("required_difficulty")) {
                    int req = data.get("required_difficulty").getAsInt();
                    difficulty = Math.max(difficulty + 1, req);
                } else {
                    difficulty++;
                }
                continue;
            }
            resp.ensureSuccess();

            String accountId = Json.str(data, "id");
            String jwt = Json.str(data, "token");
            String addr = Json.str(data, "address");
            if (addr.isEmpty()) addr = address;
            if (accountId.isEmpty() || jwt.isEmpty()) {
                throw new RuntimeException("mail-td: 创建账户失败");
            }

            String token = Json.serialize(Map.of("jwt", jwt, "id", accountId));
            return new EmailInfo("mail-td", addr, token, null, null);
        }
        throw new RuntimeException("mail-td: PoW 重试次数已用尽");
    }

    /**
     * 获取邮件列表。
     *
     * @param token JSON 编码的 {jwt, id}
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        if (token == null || token.isBlank()) throw new RuntimeException("mail-td: token 为空");

        JsonObject info = Json.parseObject(token);
        if (info == null) throw new RuntimeException("mail-td: token 格式无效");
        String jwt = Json.str(info, "jwt");
        String accountId = Json.str(info, "id");
        if (jwt.isEmpty() || accountId.isEmpty()) {
            throw new RuntimeException("mail-td: token 缺少 jwt 或 id");
        }

        Map<String, String> h = headers();
        h.put("Authorization", "Bearer " + jwt);

        HttpResult resp = HttpClient.get(
                BASE_URL + "/accounts/" + accountId + "/messages?page=1", h);
        resp.ensureSuccess();

        JsonObject body = Json.parseObject(resp.getBody());
        if (body == null) return new ArrayList<>();
        var messages = Json.arr(body, "messages");
        if (messages == null) return new ArrayList<>();

        List<Email> out = new ArrayList<>();
        for (JsonElement item : messages) {
            if (!item.isJsonObject()) continue;
            JsonObject msg = item.getAsJsonObject();

            String from;
            if (msg.has("from") && msg.get("from").isJsonObject()) {
                from = Json.str(msg.getAsJsonObject("from"), "address");
            } else {
                from = Json.str(msg, "from");
            }

            Map<String, Object> row = new LinkedHashMap<>();
            row.put("id", Json.str(msg, "id"));
            row.put("from", from);
            row.put("to", email);
            row.put("subject", Json.str(msg, "subject"));
            row.put("text", Json.str(msg, "text"));
            row.put("html", Json.str(msg, "html"));
            row.put("created_at", Json.str(msg, "created_at"));
            out.add(Normalizer.normalizeEmail(row, email));
        }
        return out;
    }
}
