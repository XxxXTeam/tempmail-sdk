package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Config
import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*
import java.util.Base64

/**
 * apihz（接口盒子）渠道 — https://apihz.cn
 *
 * 需 id/key 凭据，内置官方公共账号 88888888 作为默认。
 * token 编码: "apihz1:" + base64(JSON{mail,pwd})。
 */
object Apihz : Provider {

    private const val BASE_URL = "https://cn.apihz.cn"
    private const val TOK_PREFIX = "apihz1:"
    private const val PUBLIC_ID = "88888888"
    private const val PUBLIC_KEY = "88888888"
    private val DOMAINS = arrayOf("apimail.email", "apimail.vip")

    private val HEADERS = mapOf("User-Agent" to "Mozilla/5.0", "Accept" to "application/json")

    /** 读取凭据，优先配置/环境变量，回退公共账号。 */
    private fun credentials(): Pair<String, String> {
        val cfg = Config.current()
        val id = cfg.apihzId?.takeIf { it.isNotBlank() } ?: PUBLIC_ID
        val key = cfg.apihzKey?.takeIf { it.isNotBlank() } ?: PUBLIC_KEY
        return id to key
    }

    private fun randomPassword(): String {
        val chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
        return (1..12).map { chars.random() }.joinToString("")
    }

    private fun encToken(mail: String, pwd: String): String {
        val raw = buildJsonObject { put("mail", mail); put("pwd", pwd) }.toString()
        return TOK_PREFIX + Base64.getEncoder().encodeToString(raw.toByteArray(Charsets.UTF_8))
    }

    private fun decToken(tok: String): Pair<String, String> {
        if (!tok.startsWith(TOK_PREFIX)) throw RuntimeException("apihz: 无效 token")
        val raw = String(Base64.getDecoder().decode(tok.substring(TOK_PREFIX.length)), Charsets.UTF_8)
        val obj = Http.json.parseToJsonElement(raw) as? JsonObject ?: throw RuntimeException("apihz: 无效 token")
        val mail = str(obj, "mail").trim()
        val pwd = str(obj, "pwd").trim()
        if (mail.isEmpty() || pwd.isEmpty()) throw RuntimeException("apihz: 无效 token")
        return mail to pwd
    }
    /** 创建 apihz 临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val (id, key) = credentials()
        val domain = DOMAINS.random()
        val name = ProviderUtil.randomString(10)
        val pwd = randomPassword()
        val url = "$BASE_URL/api/mail/mailcache.php?id=$id&key=$key&domain=$domain" +
            "&name=$name&pwd=" + ProviderUtil.urlEncode(pwd) + "&buytype=0"
        val resp = ProviderUtil.httpGet(url, HEADERS)
        resp.ensureSuccess()
        val data = Http.json.parseToJsonElement(resp.body) as? JsonObject
        if (data == null || intOf(data, "code") != 200) throw RuntimeException("apihz: 创建邮箱失败")
        val mail = str(data, "mail")
        val finalPwd = str(data, "pwd").ifEmpty { pwd }
        return EmailInfo(email = mail, channel = "apihz", token = encToken(mail, finalPwd))
    }

    /** 获取 apihz 邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val (mail, pwd) = decToken(info.token)
        val (id, key) = credentials()
        val url = "$BASE_URL/api/mail/mailgetlist.php?id=$id&key=$key" +
            "&mail=" + ProviderUtil.urlEncode(mail) + "&pwd=" + ProviderUtil.urlEncode(pwd) + "&page=1"
        val resp = ProviderUtil.httpGet(url, HEADERS)
        resp.ensureSuccess()
        val data = Http.json.parseToJsonElement(resp.body) as? JsonObject
        if (data == null || intOf(data, "code") != 200) return emptyList()
        val items = data["data"] as? JsonArray ?: return emptyList()
        val out = ArrayList<Email>(items.size)
        for (item in items) {
            val msg = item as? JsonObject ?: continue
            val content = str(msg, "content")
            val row = mapOf(
                "id" to str(msg, "time1"),
                "from" to ProviderUtil.firstNonEmpty(str(msg, "frommail"), str(msg, "fromname")),
                "to" to mail,
                "subject" to str(msg, "subject"),
                "text" to content,
                "html" to content,
                "date" to ProviderUtil.firstNonEmpty(str(msg, "time2"), str(msg, "time1")),
            )
            out.add(Normalize.fromMap(row, mail))
        }
        return out
    }

    private fun str(obj: JsonObject, key: String): String =
        (obj[key] as? JsonPrimitive)?.content ?: ""

    private fun intOf(obj: JsonObject, key: String): Int =
        (obj[key] as? JsonPrimitive)?.content?.toIntOrNull() ?: -1
}
