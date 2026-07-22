package com.xxxxteam.tempmail

import kotlinx.serialization.Serializable

/**
 * 邮箱信息（生成邮箱后返回）
 *
 * @property email 完整邮箱地址
 * @property channel 渠道标识
 * @property token 会话令牌（provider 内部使用，用户无需关心）
 * @property expiresAt 邮箱过期时间（可选）
 * @property createdAt 邮箱创建时间（可选）
 */
@Serializable
data class EmailInfo(
    val email: String,
    val channel: String,
    val token: String = "",
    val expiresAt: String = "",
    val createdAt: String = "",
)

/**
 * 邮件内容（标准化后的统一格式）
 *
 * @property id 邮件唯一标识
 * @property from 发件人地址
 * @property to 收件人地址
 * @property subject 邮件主题
 * @property body 邮件正文（纯文本或 HTML）
 * @property date 收信时间
 */
@Serializable
data class Email(
    val id: String = "",
    val from: String = "",
    val to: String = "",
    val subject: String = "",
    val body: String = "",
    val date: String = "",
)

/**
 * 渠道规格描述
 *
 * @property channel 渠道标识字符串（全局唯一）
 * @property name 渠道显示名称
 * @property website 渠道对应网站域名
 */
data class ChannelSpec(
    val channel: String,
    val name: String,
    val website: String,
)

/**
 * Provider 接口：每个渠道实现此接口
 */
interface Provider {
    /** 生成临时邮箱地址 */
    suspend fun generate(): EmailInfo

    /** 获取该邮箱的收件列表 */
    suspend fun getEmails(info: EmailInfo): List<Email>
}
