package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*
import java.security.MessageDigest

/**
 * Mail.TD 渠道 — https://mail.td
 *
 * 使用 SHA-256 Proof-of-Work 创建账户。
 * GET /domains → POST /accounts → GET /accounts/{id}/messages。
 * token 存复合 JSON {jwt, id}。
 */
object MailTd : Provider {

    private const val BASE_URL = "https://api.mail.td/api"
    private const val INITIAL_DIFFICULTY = 15
    private const val MAX_DIFFICULTY = 24
    private const val MAX_RETRIES = 5

    private fun headers() = mutableMapOf(
        "Content-Type" to "application/json",
        "Accept" to "application/json",
        "User-Agent" to "Mozilla/5.0",
    )

    /** 计算摘要前导零 bit 数。 */
    private fun leadingZeroBits(digest: ByteArray): Int {
        var bits = 0
        for (b in digest) {
            if (b.toInt() == 0) { bits += 8; continue }
            val unsigned = b.toInt() and 0xFF
            for (i in 7 downTo 0) {
                if ((unsigned shr i) and 1 == 1) return bits
                bits++
            }
            return bits
        }
        return bits
    }

    /** 求解 PoW nonce。 */
    private fun solvePow(address: String, timestamp: Long, difficulty: Int): String {
        val prefix = address.lowercase().trim() + timestamp
        var nonce = 0L
        val md = MessageDigest.getInstance("SHA-256")
        while (true) {
            val digest = md.digest((prefix + nonce).toByteArray(Charsets.UTF_8))
            md.reset()
            if (leadingZeroBits(digest) >= difficulty) return nonce.toString()
            nonce++
        }
    }

    private fun sha256Hex(s: String): String {
        val hash = MessageDigest.getInstance("SHA-256").digest(s.toByteArray(Charsets.UTF_8))
        return hash.joinToString("") { "%02x".format(it.toInt() and 0xFF) }
    }
    /** 创建 mail.td 临时邮箱（含 PoW 求解）。 */
    override suspend fun generate(): EmailInfo {
        val domResp = ProviderUtil.httpGet("$BASE_URL/domains", headers())
        domResp.ensureSuccess()
        val domBody = ProviderUtil.parseObject(domResp.body) ?: throw RuntimeException("mail-td: 域名响应格式无效")
        val domArr = ProviderUtil.arr(domBody, "domains") ?: throw RuntimeException("mail-td: 未获取到域名列表")
        val freeDomains = ArrayList<String>()
        for (el in domArr) {
            val d = el as? JsonObject ?: continue
            if (ProviderUtil.bool(d, "pro_only")) continue
            val dn = ProviderUtil.str(d, "domain")
            if (dn.isNotEmpty()) freeDomains.add(dn)
        }
        if (freeDomains.isEmpty()) throw RuntimeException("mail-td: 无可用域名")
        val address = ProviderUtil.randomString(12) + "@" + freeDomains.random()
        val password = ProviderUtil.randomString(16)
        val authKey = sha256Hex(password)

        var difficulty = INITIAL_DIFFICULTY
        for (i in 0..MAX_RETRIES) {
            if (difficulty > MAX_DIFFICULTY) throw RuntimeException("mail-td: PoW 难度超出上限")
            val timestamp = System.currentTimeMillis() / 1000
            val nonce = solvePow(address, timestamp, difficulty)
            val reqBody = buildJsonObject {
                put("address", address)
                put("auth_key", authKey)
                putJsonObject("pow") {
                    put("t", timestamp)
                    put("n", nonce)
                    put("d", difficulty)
                }
            }.toString()
            val resp = ProviderUtil.httpPost("$BASE_URL/accounts", reqBody, "application/json", headers())
            val data = ProviderUtil.parseObject(resp.body) ?: throw RuntimeException("mail-td: 创建账户响应格式无效")
            if (ProviderUtil.str(data, "status") == "retry") {
                val req = (data["required_difficulty"] as? JsonPrimitive)?.content?.toIntOrNull()
                difficulty = if (req != null) maxOf(difficulty + 1, req) else difficulty + 1
                continue
            }
            resp.ensureSuccess()
            val accountId = ProviderUtil.str(data, "id")
            val jwt = ProviderUtil.str(data, "token")
            val addr = ProviderUtil.str(data, "address").ifEmpty { address }
            if (accountId.isEmpty() || jwt.isEmpty()) throw RuntimeException("mail-td: 创建账户失败")
            val token = buildJsonObject { put("jwt", jwt); put("id", accountId) }.toString()
            return EmailInfo(email = addr, channel = "mail-td", token = token)
        }
        throw RuntimeException("mail-td: PoW 重试次数已用尽")
    }

    /** 获取邮件列表，token 为复合 JSON {jwt, id}。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val token = info.token
        if (token.isBlank()) throw RuntimeException("mail-td: token 为空")
        val tok = ProviderUtil.parseObject(token) ?: throw RuntimeException("mail-td: token 格式无效")
        val jwt = ProviderUtil.str(tok, "jwt")
        val accountId = ProviderUtil.str(tok, "id")
        if (jwt.isEmpty() || accountId.isEmpty()) throw RuntimeException("mail-td: token 缺少 jwt 或 id")
        val email = info.email
        val h = headers()
        h["Authorization"] = "Bearer $jwt"
        val resp = ProviderUtil.httpGet("$BASE_URL/accounts/$accountId/messages?page=1", h)
        resp.ensureSuccess()
        val body = ProviderUtil.parseObject(resp.body) ?: return emptyList()
        val messages = ProviderUtil.arr(body, "messages") ?: return emptyList()
        val out = ArrayList<Email>(messages.size)
        for (item in messages) {
            val msg = item as? JsonObject ?: continue
            val fromObj = msg["from"] as? JsonObject
            val from = if (fromObj != null) ProviderUtil.str(fromObj, "address") else ProviderUtil.str(msg, "from")
            val row = mapOf(
                "id" to ProviderUtil.str(msg, "id"),
                "from" to from,
                "to" to email,
                "subject" to ProviderUtil.str(msg, "subject"),
                "text" to ProviderUtil.str(msg, "text"),
                "html" to ProviderUtil.str(msg, "html"),
                "created_at" to ProviderUtil.str(msg, "created_at"),
            )
            out.add(Normalize.fromMap(row, email))
        }
        return out
    }
}
