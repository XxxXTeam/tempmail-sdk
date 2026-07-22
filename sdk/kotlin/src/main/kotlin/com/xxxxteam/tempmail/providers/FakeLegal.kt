package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * Fake Legal 渠道（imgui.de）。
 *
 * 多个域名别名共享同一实现，参数化为 (channel, domain)。imgui.de / pulsewebmenu.de
 * 用 POST 创建，其余用 GET 创建。
 *
 * @property channel 渠道标识
 * @property domain 域名，空串则随机选取
 */
class FakeLegal(private val channel: String, private val domain: String) : Provider {

    private companion object {
        const val BASE = "https://imgui.de"
        const val USERNAME_CHARS = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
        val HEADERS = mapOf(
            "Accept" to "application/json, text/plain, */*",
            "Accept-Language" to "zh-CN,zh;q=0.9,en;q=0.8",
            "Cache-Control" to "no-cache",
            "DNT" to "1",
            "Pragma" to "no-cache",
            "Referer" to "https://imgui.de/",
            "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
                "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
        )
    }

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val domains = fetchDomains()
        if (domains.isEmpty()) throw RuntimeException("fake-legal: 无可用域名")
        val d = pickDomain(domains)
        val username = randomUsername(12)
        val resp = if (d == "imgui.de" || d == "pulsewebmenu.de") {
            val body = buildJsonObject {
                put("username", username)
                put("domain", d)
            }
            ProviderUtil.httpPost("$BASE/api/inbox/custom", body.toString(), "application/json", HEADERS)
        } else {
            ProviderUtil.httpGet("$BASE/api/inbox/new?domain=${ProviderUtil.urlEncode(d)}", HEADERS)
        }
        resp.ensureSuccess()
        val data = Http.json.parseToJsonElement(resp.body) as? JsonObject
        if (data == null || !jsonBool(data, "success")) {
            throw RuntimeException("fake-legal: 创建邮箱失败")
        }
        val addr = jsonStr(data, "address").trim()
        if (addr.isEmpty()) throw RuntimeException("fake-legal: 缺少邮箱地址")
        return EmailInfo(email = addr, channel = channel)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val e = info.email.trim()
        if (e.isBlank()) throw RuntimeException("fake-legal: 邮箱地址为空")
        val resp = ProviderUtil.httpGet("$BASE/api/inbox/${ProviderUtil.urlEncode(e)}", HEADERS)
        resp.ensureSuccess()
        val data = Http.json.parseToJsonElement(resp.body) as? JsonObject ?: return emptyList()
        if (!jsonBool(data, "success")) return emptyList()
        val rows = data["emails"] as? JsonArray ?: return emptyList()
        return rows.mapNotNull { it as? JsonObject }.map { row ->
            Normalize.fromJson(row, e)
        }
    }

    /** 拉取可用域名列表。 */
    private suspend fun fetchDomains(): List<String> {
        val resp = ProviderUtil.httpGet("$BASE/api/domains", HEADERS)
        resp.ensureSuccess()
        val data = Http.json.parseToJsonElement(resp.body) as? JsonObject ?: return emptyList()
        val raw = data["domains"] as? JsonArray ?: return emptyList()
        return raw.mapNotNull { (it as? JsonPrimitive)?.contentOrNull?.trim() }.filter { it.isNotEmpty() }
    }

    /** 选取域名。 */
    private fun pickDomain(domains: List<String>): String {
        val preferred = domain.trim()
        if (preferred.isNotEmpty()) {
            return domains.firstOrNull { it.equals(preferred, ignoreCase = true) }
                ?: throw RuntimeException("fake-legal: 域名不可用: $preferred")
        }
        return domains.random()
    }

    /** 生成随机用户名。 */
    private fun randomUsername(length: Int): String {
        val sb = StringBuilder(length)
        repeat(length) { sb.append(USERNAME_CHARS.random()) }
        return sb.toString()
    }
}

/** 从 JSON 对象读取字符串字段，缺失返回空串（文件私有）。 */
private fun jsonStr(obj: JsonObject, key: String): String =
    (obj[key] as? JsonPrimitive)?.contentOrNull ?: ""

/** 从 JSON 对象读取布尔字段（文件私有）。 */
private fun jsonBool(obj: JsonObject, key: String): Boolean =
    (obj[key] as? JsonPrimitive)?.booleanOrNull ?: false
