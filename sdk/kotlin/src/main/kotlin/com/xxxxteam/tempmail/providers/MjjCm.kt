package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Provider

/**
 * mjj-cm 渠道（mjj.cm，Socket.IO 临时邮箱）。
 *
 * 共用 [SocketIoMail] 协议。
 */
object MjjCm : Provider {

    private const val CHANNEL = "mjj-cm"
    private const val HOST = "mjj.cm"

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo = SocketIoMail.generate(CHANNEL, HOST)

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> = SocketIoMail.getEmails(CHANNEL, info.email)
}
