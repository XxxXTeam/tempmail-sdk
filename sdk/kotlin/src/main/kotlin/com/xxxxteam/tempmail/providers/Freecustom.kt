package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * Freecustom 渠道（freecustom.email）。
 *
 * 免注册临时邮箱，任意 local part @ 可用域名即时可收信。
 * 读信时动态获取匿名 JWT（有效期约 2 小时）。
 */
object Freecustom : Provider {

    private const val SITE_URL = "https://www.freecustom.email"
    private const val DOMAINS_URL = "https://api2.freecustom.email/domains"
    private const val REFERER = "https://www.freecustom.email/en"

    private fun baseHeaders(): MutableMap<String, String> = mutableMapOf(
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
        "Accept" to "application/json",
        "Referer" to REFERER,
    )

    /** 挑选一个当前可收信的域名。 */
    private suspend fun pickDomain(): String {
        val resp = ProviderUtil.httpGet(DOMAINS_URL, baseHeaders())
        resp.ensureSuccess()
        val obj = Http.json.parseToJsonElement(resp.body) as? JsonObject
            ?: throw RuntimeException("freecustom: 域名列表为空")
        val lst = obj["data"] as? JsonArray ?: throw RuntimeException("freecustom: 域名列表为空")
        val all = ArrayList<String>()
        val usable = ArrayList<String>()
        for (item in lst) {
            val io = item as? JsonObject ?: continue
            val domain = str(io, "domain").trim()
            if (domain.isEmpty()) continue
            all.add(domain)
            val tier = str(io, "tier").trim()
            val expiringSoon = (io["expiring_soon"] as? JsonPrimitive)?.booleanOrNull ?: false
            if (tier == "free" && !expiringSoon) usable.add(domain)
        }
        val pool = usable.ifEmpty { all }
        if (pool.isEmpty()) throw RuntimeException("freecustom: 无可用域名")
        return pool.random()
    }

    /** 获取匿名访问令牌（JWT）。 */
    private suspend fun fetchAuthToken(): String {
        val h = baseHeaders()
        h["Content-Type"] = "application/json"
        val resp = ProviderUtil.httpPost("$SITE_URL/api/auth", null, "application/json", h)
        resp.ensureSuccess()
        val obj = Http.json.parseToJsonElement(resp.body) as? JsonObject
            ?: throw RuntimeException("freecustom: 令牌响应无效")
        return str(obj, "token").trim().ifEmpty { throw RuntimeException("freecustom: 令牌响应无效") }
    }
    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val domain = pickDomain()
        val email = ProviderUtil.randomString(10) + "@" + domain
        return EmailInfo(email = email, channel = "freecustom", token = email)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val addr = info.token.ifBlank { info.email }.trim()
        if (addr.isEmpty()) throw RuntimeException("freecustom: 缺少邮箱地址")
        val jwt = fetchAuthToken()
        val authHeaders = baseHeaders()
        authHeaders["Authorization"] = "Bearer $jwt"
        authHeaders["x-fce-client"] = "web-client"

        val listResp = ProviderUtil.httpGet(
            "$SITE_URL/api/public-mailbox?fullMailboxId=" + ProviderUtil.urlEncode(addr), authHeaders)
        listResp.ensureSuccess()
        val listObj = Http.json.parseToJsonElement(listResp.body) as? JsonObject ?: return emptyList()
        if ((listObj["success"] as? JsonPrimitive)?.booleanOrNull != true) return emptyList()
        val items = listObj["data"] as? JsonArray ?: return emptyList()

        val result = ArrayList<Email>(items.size)
        for (itemEl in items) {
            val itemObj = itemEl as? JsonObject ?: continue
            val msgId = str(itemObj, "id").trim()
            if (msgId.isEmpty()) continue
            var full: JsonObject = itemObj
            // 尝试补全正文
            try {
                val msgResp = ProviderUtil.httpGet(
                    "$SITE_URL/api/public-mailbox?fullMailboxId=" + ProviderUtil.urlEncode(addr) +
                        "&messageId=" + ProviderUtil.urlEncode(msgId), authHeaders)
                if (msgResp.isOk) {
                    val msgObj = Http.json.parseToJsonElement(msgResp.body) as? JsonObject
                    if (msgObj != null && (msgObj["success"] as? JsonPrimitive)?.booleanOrNull == true) {
                        (msgObj["data"] as? JsonObject)?.let { full = it }
                    }
                }
            } catch (_: Exception) {
            }
            val row = mapOf(
                "id" to str(full, "id"),
                "from" to str(full, "from"),
                "to" to ProviderUtil.firstNonEmpty(str(full, "to"), addr),
                "subject" to str(full, "subject"),
                "text" to str(full, "text"),
                "html" to str(full, "html"),
                "date" to str(full, "date"),
            )
            result.add(Normalize.fromMap(row, addr))
        }
        return result
    }

    private fun str(obj: JsonObject, key: String): String =
        (obj[key] as? JsonPrimitive)?.content ?: ""
}
