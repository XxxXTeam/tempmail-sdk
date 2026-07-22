package com.xxxxteam.tempmail.providers;

import com.xxxxteam.tempmail.Email;
import com.xxxxteam.tempmail.EmailInfo;

import java.util.List;

/**
 * mjj-cm 渠道实现（mjj.cm，Socket.IO 临时邮箱）。
 *
 * <p>共用 {@link SocketIoMail} 协议。</p>
 */
public final class MjjCm {

    private static final String CHANNEL = "mjj-cm";
    private static final String HOST = "mjj.cm";

    private MjjCm() {
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        return SocketIoMail.generate(CHANNEL, HOST);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址
     * @param token 认证令牌，可为 null
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email, String token) {
        return SocketIoMail.getEmails(CHANNEL, email, token);
    }
}
