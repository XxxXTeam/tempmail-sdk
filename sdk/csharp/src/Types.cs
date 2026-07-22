using System.Collections.Generic;

namespace XxxXTeam.TempMail;

/// <summary>
/// 创建临时邮箱后返回的邮箱信息。
/// Token 等认证信息由 SDK 内部维护（internal），不对外暴露。
/// </summary>
public sealed class EmailInfo
{
    /// <summary>创建该邮箱所使用的渠道标识</summary>
    public string Channel { get; }

    /// <summary>临时邮箱地址</summary>
    public string Email { get; }

    /// <summary>认证令牌，SDK 内部维护，不对外暴露</summary>
    internal string? Token { get; }

    /// <summary>邮箱过期时间（毫秒时间戳），可空</summary>
    public long? ExpiresAt { get; }

    /// <summary>邮箱创建时间（原样透传各渠道返回值），可空</summary>
    public object? CreatedAt { get; }

    public EmailInfo(string channel, string email, string? token = null, long? expiresAt = null, object? createdAt = null)
    {
        Channel = channel;
        Email = email;
        Token = token;
        ExpiresAt = expiresAt;
        CreatedAt = createdAt;
    }

    public override string ToString() =>
        $"EmailInfo(channel={Channel}, email={Email}, expiresAt={ExpiresAt}, createdAt={CreatedAt})";
}

/// <summary>邮件附件信息</summary>
public sealed class EmailAttachment
{
    /// <summary>附件文件名</summary>
    public string Filename { get; set; } = "";

    /// <summary>附件大小（字节）</summary>
    public long? Size { get; set; }

    /// <summary>附件 MIME 类型</summary>
    public string? ContentType { get; set; }

    /// <summary>附件下载地址</summary>
    public string? Url { get; set; }
}

/// <summary>标准化后的邮件对象（所有渠道统一输出）</summary>
public sealed class Email
{
    /// <summary>邮件唯一标识</summary>
    public string Id { get; set; } = "";

    /// <summary>发件人地址</summary>
    public string From { get; set; } = "";

    /// <summary>收件人地址</summary>
    public string To { get; set; } = "";

    /// <summary>邮件主题</summary>
    public string Subject { get; set; } = "";

    /// <summary>纯文本内容</summary>
    public string Text { get; set; } = "";

    /// <summary>HTML 内容</summary>
    public string Html { get; set; } = "";

    /// <summary>接收日期（ISO 8601 格式）</summary>
    public string Date { get; set; } = "";

    /// <summary>是否已读</summary>
    public bool IsRead { get; set; }

    /// <summary>附件列表</summary>
    public List<EmailAttachment> Attachments { get; set; } = new();
}

/// <summary>获取邮件列表的结果</summary>
public sealed class GetEmailsResult
{
    /// <summary>渠道标识</summary>
    public string Channel { get; set; } = "";

    /// <summary>邮箱地址</summary>
    public string Email { get; set; } = "";

    /// <summary>邮件列表</summary>
    public List<Email> Emails { get; set; } = new();

    /// <summary>本次请求是否成功</summary>
    public bool Success { get; set; } = true;
}

/// <summary>重试配置</summary>
public sealed class RetryConfig
{
    /// <summary>最大重试次数（不含首次请求），默认 2</summary>
    public int MaxRetries { get; set; } = 2;

    /// <summary>初始重试延迟（秒），默认 1.0</summary>
    public double InitialDelay { get; set; } = 1.0;

    /// <summary>最大重试延迟（秒），默认 5.0</summary>
    public double MaxDelay { get; set; } = 5.0;

    /// <summary>请求超时（秒），默认 15.0</summary>
    public double Timeout { get; set; } = 15.0;
}

/// <summary>创建临时邮箱的选项</summary>
public sealed class GenerateEmailOptions
{
    /// <summary>指定渠道，不指定则随机选择</summary>
    public string? Channel { get; set; }

    /// <summary>tempmail 渠道的有效期（分钟）</summary>
    public int Duration { get; set; } = 30;

    /// <summary>指定域名（tempmail-cn / tempmail-lol / maildrop / fake-legal / mail-cx 等）</summary>
    public string? Domain { get; set; }

    /// <summary>重试配置</summary>
    public RetryConfig? Retry { get; set; }

    /// <summary>最大尝试渠道数</summary>
    public int MaxChannelsTried { get; set; } = 20;

    /// <summary>整体超时时间（秒）</summary>
    public double TotalTimeout { get; set; } = 60.0;

    /// <summary>邮箱后缀筛选（如 "@gmail.com" 或 "gmail.com"）</summary>
    public string? Suffix { get; set; }

    /// <summary>多个目标域名</summary>
    public List<string>? Domains { get; set; }
}

/// <summary>获取邮件的选项（Channel/Email/Token 由 SDK 从 EmailInfo 自动获取）</summary>
public sealed class GetEmailsOptions
{
    /// <summary>重试配置</summary>
    public RetryConfig? Retry { get; set; }
}

/// <summary>渠道信息</summary>
public sealed class ChannelInfo
{
    /// <summary>渠道标识</summary>
    public string Channel { get; set; } = "";

    /// <summary>渠道显示名称</summary>
    public string Name { get; set; } = "";

    /// <summary>对应的临时邮箱服务网站</summary>
    public string Website { get; set; } = "";
}
