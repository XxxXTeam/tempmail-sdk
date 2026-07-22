package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * 24mail.chacuo.net 渠道 — http://24mail.chacuo.net
 *
 * POST / form: data={email}&type=refresh&arg= 创建和刷新邮箱，token 即邮箱地址。
 * 使用默认域名 chacuo.net。
 */
object Chacuo24Mail : Provider {

    /** POST /  刷新（创建）邮箱并返回响应对象。 */
    private suspend fun refresh(email: String): JsonObject {
        val body = "data=" + ProviderUtil.urlEncode(email) + "&type=refresh&arg="
        val resp = ProviderUtil.httpPost(
            BASE_URL, body, "application/x-www-form-urlencoded; charset=UTF-8", HEADERS)
        resp.ensureSuccess()
        return Http.json.parseToJsonElement(resp.body) as? JsonObject
            ?: throw RuntimeException("24mail-chacuo: 响应格式异常")
    }

    /** 创建 24mail.chacuo.net 临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val email = "sdk" + ProviderUtil.randomString(12) + "@" + DEFAULT_DOMAIN
        val result = refresh(email)
        if (intOf(result, "status") != 1) throw RuntimeException("24mail-chacuo: 创建邮箱失败")
        return EmailInfo(email = email, channel = "24mail-chacuo", token = email)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val addr = info.email.ifBlank { info.token }.trim()
        if (addr.isEmpty()) throw RuntimeException("24mail-chacuo: 邮箱地址为空")
        val result = refresh(addr)
        val dataArr = result["data"] as? JsonArray ?: return emptyList()
        val first = dataArr.getOrNull(0) as? JsonObject ?: return emptyList()
        val rows = first["list"] as? JsonArray ?: return emptyList()
        val out = ArrayList<Email>(rows.size)
        for (item in rows) {
            val msg = item as? JsonObject ?: continue
            val row = mapOf(
                "id" to str(msg, "MID"),
                "from" to str(msg, "FROM"),
                "to" to addr,
                "subject" to str(msg, "SUBJECT"),
                "content" to str(msg, "CONTENT"),
                "date" to str(msg, "TIME"),
            )
            out.add(Normalize.fromMap(row, addr))
        }
        return out
    }

    private fun str(obj: JsonObject, key: String): String =
        (obj[key] as? JsonPrimitive)?.content ?: ""

    private fun intOf(obj: JsonObject, key: String): Int =
        (obj[key] as? JsonPrimitive)?.content?.toIntOrNull() ?: -1

    private const val BASE_URL = "http://24mail.chacuo.net/"
    private const val DEFAULT_DOMAIN = "chacuo.net"
    private val HEADERS = mapOf(
        "User-Agent" to "Mozilla/5.0",
        "Accept" to "application/json, text/javascript, */*; q=0.01",
        "X-Requested-With" to "XMLHttpRequest",
        "Content-Type" to "application/x-www-form-urlencoded; charset=UTF-8",
        "Origin" to "http://24mail.chacuo.net",
        "Referer" to "http://24mail.chacuo.net/",
    )
}
