package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.JsonObject

/**
 * temp.tf 渠道（temp.tf）。
 *
 * 创建返回一次性地址；收件列表响应按 bodyContentType 归位正文。
 *
 * @property channel 对外渠道标识
 */
class TempTF(private val channel: String) : Provider {

    private val headers = mapOf(
        "Accept" to "application/json",
        "Content-Type" to "application/json",
    )

    /** 获取一个一次性地址。 */
    override suspend fun generate(): EmailInfo {
        val resp = ProviderUtil.httpGet("$API_BASE/account?plus=1", headers)
        resp.ensureSuccess()
        val address = ProviderUtil.str(ProviderUtil.parseObject(resp.body), "email").trim()
        if (address.isEmpty()) throw RuntimeException("temp-tf: empty email")
        return EmailInfo(email = address, channel = channel)
    }

    /** 获取 temp.tf 邮件，body 按 bodyContentType 归位。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val address = info.email.trim()
        if (address.isEmpty()) throw RuntimeException("temp-tf: empty email")
        val resp = ProviderUtil.httpPost(
            "$API_BASE/check",
            body = """{"email":"$address"}""",
            contentType = "application/json",
            headers = mapOf("Accept" to "application/json"),
        )
        resp.ensureSuccess()
        val rows = ProviderUtil.arr(ProviderUtil.parseObject(resp.body), "data")
            ?: return emptyList()
        return rows.filterIsInstance<JsonObject>().map { item ->
            val isText = ProviderUtil.str(item, "bodyContentType").equals("text", ignoreCase = true)
            val body = ProviderUtil.str(item, "body")
            Normalize.fromMap(
                mapOf(
                    "id" to ProviderUtil.str(item, "id"),
                    "from" to ProviderUtil.str(item, "from"),
                    "to" to address,
                    "subject" to ProviderUtil.str(item, "subject"),
                    "text" to if (isText) body else "",
                    "html" to if (isText) "" else body,
                    "date" to ProviderUtil.str(item, "date"),
                ),
                address,
            )
        }
    }

    private companion object {
        const val API_BASE = "https://temp.tf/api"
    }
}
