package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*
import kotlin.random.Random

/**
 * restmail-net 渠道（restmail.net）。
 *
 * Mozilla 开源项目，无需创建邮箱，ad-hoc 模式：随机 username，
 * 邮箱即 username@restmail.net，token 留空。
 */
object RestmailNet : Provider {

    private const val CHANNEL = "restmail-net"
    private const val BASE_URL = "https://restmail.net"
    private val headers = mapOf(
        "Accept" to "application/json",
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
    )

    /** 本地生成邮箱，无需请求服务端。 */
    override suspend fun generate(): EmailInfo {
        val username = ProviderUtil.randomString(10 + Random.nextInt(3)) // 10-12 位
        return EmailInfo(email = "$username@restmail.net", channel = CHANNEL)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val address = info.email.trim()
        if (address.isEmpty()) throw RuntimeException("restmail-net: 邮箱地址为空")
        val username = address.substringBefore('@')
        if (username.isEmpty()) throw RuntimeException("restmail-net: 无法提取用户名")
        val resp = ProviderUtil.httpGet("$BASE_URL/mail/$username", headers)
        if (resp.statusCode == 404) return emptyList()
        resp.ensureSuccess()
        val arr = ProviderUtil.parse(resp.body) as? JsonArray ?: return emptyList()
        return arr.filterIsInstance<JsonObject>().map { msg ->
            // 发件人优先 from[0].address，其次 headers.from
            val fromArr = msg["from"] as? JsonArray
            val fromAddr = (fromArr?.firstOrNull() as? JsonObject)?.let { ProviderUtil.str(it, "address") }
                ?.takeIf { it.isNotEmpty() }
                ?: ProviderUtil.str(msg["headers"] as? JsonObject, "from")
            val row = mapOf(
                "id" to ProviderUtil.str(msg, "id"),
                "from" to fromAddr,
                "to" to address,
                "subject" to ProviderUtil.str(msg, "subject"),
                "text" to ProviderUtil.str(msg, "text"),
                "html" to ProviderUtil.str(msg, "html"),
                "date" to ProviderUtil.str(msg, "receivedAt"),
            )
            Normalize.fromMap(row, address)
        }
    }
}
