using System;
using System.Collections.Generic;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// 占位 provider：用于 HTML 抓取 / WebSocket / socket.io 等复杂渠道尚未落地真实现的情况。
/// 保证渠道可注册、可枚举、可编译；被调用时抛出 NotImplementedException。
/// 后续按 Python/npm 参考实现逐步替换为真逻辑。
/// </summary>
public static class Stub
{
    /// <summary>桩创建：抛出未实现异常（会被 fallback 逻辑捕获并尝试其他渠道）</summary>
    public static EmailInfo Generate(string channel)
        => throw new NotImplementedException($"channel '{channel}' generate 尚未实现（占位桩）");

    /// <summary>桩收信：抛出未实现异常</summary>
    public static List<Email> GetEmails(string channel)
        => throw new NotImplementedException($"channel '{channel}' getEmails 尚未实现（占位桩）");
}
