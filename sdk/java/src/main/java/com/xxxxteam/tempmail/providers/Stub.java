package com.xxxxteam.tempmail.providers;

import com.xxxxteam.tempmail.Email;
import com.xxxxteam.tempmail.EmailInfo;

import java.util.List;

/**
 * 占位 provider：用于 HTML 抓取 / WebSocket / socket.io 等复杂渠道尚未落地真实现的情况。
 *
 * <p>保证渠道可注册、可枚举、可编译；被调用时抛出 {@link UnsupportedOperationException}
 * （会被 fallback 逻辑捕获并尝试其他渠道）。后续按 Python/C# 参考实现逐步替换为真逻辑。</p>
 */
public final class Stub {

    private Stub() {
    }

    /**
     * 桩创建：抛出未实现异常。
     *
     * @param channel 渠道标识
     * @return 不返回，恒抛异常
     */
    public static EmailInfo generate(String channel) {
        throw new UnsupportedOperationException("TODO: 待实现 —— channel '" + channel + "' generate（占位桩）");
    }

    /**
     * 桩收信：抛出未实现异常。
     *
     * @param channel 渠道标识
     * @return 不返回，恒抛异常
     */
    public static List<Email> getEmails(String channel) {
        throw new UnsupportedOperationException("TODO: 待实现 —— channel '" + channel + "' getEmails（占位桩）");
    }
}
