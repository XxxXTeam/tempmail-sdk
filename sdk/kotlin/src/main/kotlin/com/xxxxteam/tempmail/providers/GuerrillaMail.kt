package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * GuerrillaMail 及其全部镜像渠道实现。
 *
 * 所有镜像共用同一套 ajax.php API，仅 baseUrl 不同：
 * guerrillamail.com / sharklasers.com / grr.la / guerrillamail.info / spam4.me 等。
 *
 * @property channel 对外渠道标识
 * @property baseUrl API 基地址
 */
class GuerrillaMail(private val channel: String, private val baseUrl: String) : Provider {

    private val headers = mapOf(
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
    )

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val resp = ProviderUtil.httpGet("$baseUrl?f=get_email_address&lang=en", headers)
        resp.ensureSuccess()
        val data = ProviderUtil.parseObject(resp.body)
        val email = ProviderUtil.str(data, "email_addr")
        val token = ProviderUtil.str(data, "sid_token")
        if (email.isEmpty() || token.isEmpty()) {
            throw RuntimeException("$channel: 缺少 email_addr 或 sid_token")
        }
        val expiresAt = (data?.get("email_timestamp") as? JsonPrimitive)?.longOrNull
            ?.let { ((it + 3600) * 1000).toString() } ?: ""
        return EmailInfo(email = email, channel = channel, token = token, expiresAt = expiresAt)
    }

    /** 获取邮件列表，对每封调用 fetch_email 拉正文。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val tok = info.token
        val resp = ProviderUtil.httpGet(
            "$baseUrl?f=check_email&seq=0&sid_token=${ProviderUtil.urlEncode(tok)}", headers)
        resp.ensureSuccess()
        val list = ProviderUtil.arr(ProviderUtil.parseObject(resp.body), "list") ?: return emptyList()

        val result = mutableListOf<Email>()
        for (msg in list.filterIsInstance<JsonObject>()) {
            var body = ProviderUtil.str(msg, "mail_body")
            val mailId = ProviderUtil.str(msg, "mail_id")
            if (body.isEmpty() && mailId.isNotEmpty()) {
                try {
                    val dr = ProviderUtil.httpGet(
                        "$baseUrl?f=fetch_email&sid_token=${ProviderUtil.urlEncode(tok)}" +
                            "&email_id=${ProviderUtil.urlEncode(mailId)}",
                        headers,
                    )
                    if (dr.isOk) body = ProviderUtil.str(ProviderUtil.parseObject(dr.body), "mail_body")
                } catch (_: Exception) {
                }
            }
            val text = if (body.isEmpty()) ProviderUtil.str(msg, "mail_excerpt") else Normalize.htmlToText(body)
            val raw = mapOf(
                "id" to mailId,
                "from" to ProviderUtil.str(msg, "mail_from"),
                "to" to info.email,
                "subject" to ProviderUtil.str(msg, "mail_subject"),
                "text" to text,
                "html" to body,
                "date" to ProviderUtil.str(msg, "mail_date"),
            )
            result.add(Normalize.fromMap(raw, info.email))
        }
        return result
    }
}
