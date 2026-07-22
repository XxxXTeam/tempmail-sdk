package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Config
import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * Dropmail 渠道（dropmail.me GraphQL API）。
 *
 * 需先获取 af_ 令牌，再通过 GraphQL mutation 创建 session。token 为 session id。
 */
object Dropmail : Provider {

    private const val CHANNEL = "dropmail"
    private const val TOKEN_GENERATE_URL = "https://dropmail.me/api/token/generate"
    private const val CREATE_SESSION_QUERY =
        "mutation {introduceSession {id, expiresAt, addresses{id, address}}}"
    private const val GET_MAILS_QUERY =
        "query (\$id: ID!) { session(id:\$id) { mails { id, rawSize, fromAddr, toAddr, receivedAt, " +
            "text, headerFrom, headerSubject, html } } }"

    private val lock = Any()
    private var cachedToken: String? = null
    private var cachedExpiry: Long = 0L // System.nanoTime() 基准

    private fun tokenHeaders(): Map<String, String> = mapOf(
        "Accept" to "application/json",
        "Content-Type" to "application/json",
        "Origin" to "https://dropmail.me",
        "Referer" to "https://dropmail.me/api/",
    )

    /** 获取或刷新 af_ 令牌。 */
    private suspend fun resolveAfToken(): String {
        Config.current().dropmailAuthToken?.takeIf { it.isNotBlank() }?.let { return it.trim() }
        System.getenv("DROPMAIL_API_TOKEN")?.takeIf { it.isNotBlank() }?.let { return it.trim() }

        synchronized(lock) {
            val now = System.nanoTime()
            cachedToken?.let { if (now < cachedExpiry) return it }
        }
        val payload = buildJsonObject {
            put("type", "af")
            put("lifetime", "1h")
        }.toString()
        val resp = ProviderUtil.httpPost(TOKEN_GENERATE_URL, payload, "application/json", tokenHeaders())
        resp.ensureSuccess()
        val parsed = Http.json.parseToJsonElement(resp.body) as? JsonObject
        val tok = (parsed?.let { jsonStr(it, "token") } ?: "").trim()
        if (tok.isEmpty() || !tok.startsWith("af_")) {
            val err = parsed?.let { jsonStr(it, "error") } ?: ""
            throw RuntimeException("dropmail: token/generate 未返回有效 af_ 令牌: $err")
        }
        synchronized(lock) {
            cachedToken = tok
            cachedExpiry = System.nanoTime() + 50L * 60 * 1_000_000_000L // 50 分钟
        }
        return tok
    }

    /** 发起 GraphQL 请求（form-urlencoded）。 */
    private suspend fun graphqlRequest(query: String, variables: Map<String, String>?): JsonObject {
        val sb = StringBuilder()
        sb.append("query=").append(ProviderUtil.urlEncode(query))
        if (!variables.isNullOrEmpty()) {
            val varsJson = buildJsonObject { variables.forEach { (k, v) -> put(k, v) } }.toString()
            sb.append("&variables=").append(ProviderUtil.urlEncode(varsJson))
        }
        val url = "https://dropmail.me/api/graphql/${ProviderUtil.urlEncode(resolveAfToken())}"
        val resp = ProviderUtil.httpPost(
            url, sb.toString(), "application/x-www-form-urlencoded",
            mapOf("Content-Type" to "application/x-www-form-urlencoded"),
        )
        resp.ensureSuccess()
        val obj = Http.json.parseToJsonElement(resp.body) as? JsonObject
            ?: throw RuntimeException("dropmail: GraphQL 响应格式无效")
        (obj["errors"] as? JsonArray)?.takeIf { it.isNotEmpty() }?.let {
            val msg = (it[0] as? JsonObject)?.let { e -> jsonStr(e, "message") } ?: ""
            throw RuntimeException("dropmail: GraphQL error: $msg")
        }
        return obj["data"] as? JsonObject ?: JsonObject(emptyMap())
    }

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val data = graphqlRequest(CREATE_SESSION_QUERY, null)
        val session = data["introduceSession"] as? JsonObject
            ?: throw RuntimeException("dropmail: 创建 session 失败")
        val sessionId = jsonStr(session, "id")
        val addrs = session["addresses"] as? JsonArray
        if (sessionId.isEmpty() || addrs.isNullOrEmpty()) {
            throw RuntimeException("dropmail: 响应中缺少 session ID 或地址")
        }
        val address = (addrs[0] as? JsonObject)?.let { jsonStr(it, "address") } ?: ""
        if (address.isEmpty()) throw RuntimeException("dropmail: 地址为空")
        return EmailInfo(
            email = address, channel = CHANNEL, token = sessionId,
            expiresAt = jsonStr(session, "expiresAt"),
        )
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val token = info.token
        if (token.isBlank()) throw RuntimeException("dropmail: token 不能为空")
        val data = graphqlRequest(GET_MAILS_QUERY, mapOf("id" to token))
        val session = data["session"] as? JsonObject ?: return emptyList()
        val mails = session["mails"] as? JsonArray ?: return emptyList()
        return mails.mapNotNull { it as? JsonObject }.map { mail ->
            val toAddr = jsonStr(mail, "toAddr")
            val flat = mapOf(
                "id" to jsonStr(mail, "id"),
                "from" to jsonStr(mail, "fromAddr"),
                "to" to toAddr.ifEmpty { info.email },
                "subject" to jsonStr(mail, "headerSubject"),
                "body" to ProviderUtil.firstNonEmpty(jsonStr(mail, "text"), jsonStr(mail, "html")),
                "date" to jsonStr(mail, "receivedAt"),
            )
            Normalize.fromMap(flat, info.email)
        }
    }
}

/** 从 JSON 对象读取字符串字段，缺失返回空串（文件私有）。 */
private fun jsonStr(obj: JsonObject, key: String): String =
    (obj[key] as? JsonPrimitive)?.contentOrNull ?: ""
