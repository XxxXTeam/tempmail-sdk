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
import java.util.ArrayList;
import java.util.Base64;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ThreadLocalRandom;

/**
 * apihz（接口盒子）渠道 — https://apihz.cn
 *
 * <p>需 id/key 凭据。内置官方公共账号 88888888 作为默认。
 * token 编码: "apihz1:" + base64(JSON{mail,pwd})。</p>
 */
public final class Apihz {

    private static final String BASE_URL = "https://cn.apihz.cn";
    private static final String TOK_PREFIX = "apihz1:";
    private static final String PUBLIC_ID = "88888888";
    private static final String PUBLIC_KEY = "88888888";
    private static final String[] DOMAINS = {"apimail.email", "apimail.vip"};

    private Apihz() {
    }

    private static Map<String, String> headers() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("User-Agent", "Mozilla/5.0");
        h.put("Accept", "application/json");
        return h;
    }

    private static String[] getCredentials() {
        String id = System.getenv("APIHZ_ID");
        String key = System.getenv("APIHZ_KEY");
        if (id == null || id.isBlank()) id = PUBLIC_ID;
        if (key == null || key.isBlank()) key = PUBLIC_KEY;
        return new String[]{id, key};
    }

    private static String randomPassword() {
        String chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        StringBuilder sb = new StringBuilder(12);
        ThreadLocalRandom r = ThreadLocalRandom.current();
        for (int i = 0; i < 12; i++) sb.append(chars.charAt(r.nextInt(chars.length())));
        return sb.toString();
    }

    private static String encToken(String mail, String pwd) {
        String raw = Json.serialize(Map.of("mail", mail, "pwd", pwd));
        return TOK_PREFIX + Base64.getEncoder().encodeToString(raw.getBytes(StandardCharsets.UTF_8));
    }

    private static String[] decToken(String tok) {
        if (tok == null || !tok.startsWith(TOK_PREFIX)) {
            throw new RuntimeException("apihz: 无效 token");
        }
        String raw = new String(Base64.getDecoder().decode(tok.substring(TOK_PREFIX.length())), StandardCharsets.UTF_8);
        JsonObject obj = Json.parseObject(raw);
        if (obj == null) throw new RuntimeException("apihz: 无效 token");
        String mail = Json.str(obj, "mail").trim();
        String pwd = Json.str(obj, "pwd").trim();
        if (mail.isEmpty() || pwd.isEmpty()) throw new RuntimeException("apihz: 无效 token");
        return new String[]{mail, pwd};
    }

    /**
     * 创建 apihz 临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        String[] cred = getCredentials();
        String domain = DOMAINS[ThreadLocalRandom.current().nextInt(DOMAINS.length)];
        String name = ProviderUtil.randomString(10);
        String pwd = randomPassword();

        String url = BASE_URL + "/api/mail/mailcache.php?id=" + cred[0]
                + "&key=" + cred[1] + "&domain=" + domain
                + "&name=" + name + "&pwd=" + ProviderUtil.urlEncode(pwd) + "&buytype=0";
        HttpResult resp = HttpClient.get(url, headers());
        resp.ensureSuccess();

        JsonObject data = Json.parseObject(resp.getBody());
        if (data == null || !data.has("code") || data.get("code").getAsInt() != 200) {
            throw new RuntimeException("apihz: 创建邮箱失败");
        }
        String mail = Json.str(data, "mail");
        String finalPwd = Json.str(data, "pwd");
        if (finalPwd.isEmpty()) finalPwd = pwd;

        return new EmailInfo("apihz", mail, encToken(mail, finalPwd), null, null);
    }

    /**
     * 获取 apihz 邮件列表。
     *
     * @param token 编码的 token（含 mail 和 pwd）
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token) {
        String[] mp = decToken(token);
        String mail = mp[0];
        String pwd = mp[1];
        String[] cred = getCredentials();

        String url = BASE_URL + "/api/mail/mailgetlist.php?id=" + cred[0]
                + "&key=" + cred[1] + "&mail=" + ProviderUtil.urlEncode(mail)
                + "&pwd=" + ProviderUtil.urlEncode(pwd) + "&page=1";
        HttpResult resp = HttpClient.get(url, headers());
        resp.ensureSuccess();

        JsonObject data = Json.parseObject(resp.getBody());
        if (data == null || !data.has("code") || data.get("code").getAsInt() != 200) {
            return new ArrayList<>();
        }
        var items = Json.arr(data, "data");
        if (items == null) return new ArrayList<>();

        List<Email> out = new ArrayList<>();
        for (JsonElement item : items) {
            if (!item.isJsonObject()) continue;
            JsonObject msg = item.getAsJsonObject();
            Map<String, Object> row = new LinkedHashMap<>();
            row.put("id", ProviderUtil.firstNonEmpty(Json.str(msg, "time1")));
            row.put("from", ProviderUtil.firstNonEmpty(Json.str(msg, "frommail"), Json.str(msg, "fromname")));
            row.put("to", mail);
            row.put("subject", Json.str(msg, "subject"));
            row.put("text", Json.str(msg, "content"));
            row.put("html", Json.str(msg, "content"));
            row.put("date", ProviderUtil.firstNonEmpty(Json.str(msg, "time2"), Json.str(msg, "time1")));
            out.add(Normalizer.normalizeEmail(row, mail));
        }
        return out;
    }
}
