package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*
import java.time.LocalDate
import java.time.ZoneOffset
import java.time.format.DateTimeFormatter
import java.util.Base64

/**
 * dropmail-me 渠道（dropmail.me）。
 *
 * GraphQL 临时邮箱服务，需自行生成认证 token（页面提取 data-k → 反转 base64
 * 解出 secret → FNV-1a 变体哈希）。token 为 JSON {"session_id","auth_token"}。
 */
object DropmailMe : Provider {

    private const val CHANNEL = "dropmail-me"
    private const val BASE_URL = "https://dropmail.me"
    private val DATA_K_RE = Regex("<meta\\s+name=\"app-config\"\\s+data-k=\"([^\"]+)\"")
    private const val UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
        "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36"
    private val apiHeaders = mapOf("User-Agent" to UA, "Accept" to "application/json", "Content-Type" to "application/json")

    /** FNV-1a 变体哈希，返回十六进制字符串。 */
    private fun fnvHash(s: String): String {
        var h = 2166136261L
        for (c in s) {
            h = h xor c.code.toLong()
            h += (h shl 1) + (h shl 4) + (h shl 7) + (h shl 8) + (h shl 24)
            h = h and 0xFFFFFFFFL
        }
        return h.toString(16)
    }

    /** 生成 dropmail.me API 认证 token。 */
    private suspend fun generateAuthToken(): String {
        val resp = ProviderUtil.httpGet("$BASE_URL/en/", mapOf("User-Agent" to UA, "Accept" to "text/html"))
        resp.ensureSuccess()
        val dataK = DATA_K_RE.find(resp.body)?.groupValues?.get(1)
            ?: throw RuntimeException("dropmail-me: 无法从页面提取 data-k")
        // 反转 + base64 解码得到 secret
        val secret = String(Base64.getDecoder().decode(dataK.reversed()), Charsets.UTF_8)
        val dateStr = LocalDate.now(ZoneOffset.UTC).format(DateTimeFormatter.ofPattern("yyyyMMdd"))
        val randomPart = dateStr + ProviderUtil.randomString(16)
        return "website_${randomPart}_${fnvHash(randomPart + secret)}"
    }

    /** 创建 dropmail.me 临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val authToken = generateAuthToken()
        val query = "mutation { introduceSession { id expiresAt addresses { address } } }"
        val body = buildJsonObject { put("query", query) }.toString()
        val resp = ProviderUtil.httpPost(
            "$BASE_URL/api/graphql/" + ProviderUtil.urlEncode(authToken), body, "application/json", apiHeaders)
        resp.ensureSuccess()
        val session = (ProviderUtil.parseObject(resp.body)?.get("data") as? JsonObject)
            ?.get("introduceSession") as? JsonObject
            ?: throw RuntimeException("dropmail-me: 创建 session 失败")
        val sessionId = ProviderUtil.str(session, "id")
        val addresses = session["addresses"] as? JsonArray
        val address = (addresses?.firstOrNull() as? JsonObject)?.let { ProviderUtil.str(it, "address") } ?: ""
        if (sessionId.isEmpty() || address.isEmpty()) throw RuntimeException("dropmail-me: 响应中缺少 session ID 或地址")
        val tokenJson = buildJsonObject {
            put("session_id", sessionId)
            put("auth_token", authToken)
        }.toString()
        return EmailInfo(email = address, channel = CHANNEL, token = tokenJson)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        if (info.token.isBlank()) throw IllegalArgumentException("dropmail-me: token 不能为空")
        val addr = info.email.trim()
        if (addr.isEmpty()) throw IllegalArgumentException("dropmail-me: 邮箱地址不能为空")
        val tokenObj = ProviderUtil.parseObject(info.token)
            ?: throw IllegalArgumentException("dropmail-me: token 格式无效")
        val sessionId = ProviderUtil.str(tokenObj, "session_id")
        val authToken = ProviderUtil.str(tokenObj, "auth_token")
        if (sessionId.isEmpty() || authToken.isEmpty()) {
            throw IllegalArgumentException("dropmail-me: token 中缺少 session_id 或 auth_token")
        }
        val query = "{ session(id:\"$sessionId\") " +
            "{ mails { id headerFrom headerSubject text html receivedAt } } }"
        val body = buildJsonObject { put("query", query) }.toString()
        val resp = ProviderUtil.httpPost(
            "$BASE_URL/api/graphql/" + ProviderUtil.urlEncode(authToken), body, "application/json", apiHeaders)
        resp.ensureSuccess()
        val mails = ((ProviderUtil.parseObject(resp.body)?.get("data") as? JsonObject)
            ?.get("session") as? JsonObject)?.get("mails") as? JsonArray ?: return emptyList()
        return mails.filterIsInstance<JsonObject>().map { msg ->
            val row = mapOf(
                "id" to ProviderUtil.str(msg, "id"),
                "from" to ProviderUtil.str(msg, "headerFrom"),
                "to" to addr,
                "subject" to ProviderUtil.str(msg, "headerSubject"),
                "text" to ProviderUtil.str(msg, "text"),
                "html" to ProviderUtil.str(msg, "html"),
                "date" to ProviderUtil.str(msg, "receivedAt"),
            )
            Normalize.fromMap(row, addr)
        }
    }
}
