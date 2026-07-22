using System;
using System.Threading;

namespace XxxXTeam.TempMail;

/// <summary>
/// 通用重试工具：网络错误 / 超时 / 429 / HTTP 4xx-5xx 自动重试，指数退避。
/// 参数校验类错误（ArgumentException 等）不重试，直接抛出。
/// </summary>
public static class Retry
{
    /// <summary>重试结果：ok 时 Value 有效，否则 Error 有效</summary>
    public sealed class Result<T>
    {
        public bool Ok { get; init; }
        public int Attempts { get; init; }
        public T? Value { get; init; }
        public Exception? Error { get; init; }
    }

    // 网络类错误关键词：static readonly 避免每次判定都分配数组
    private static readonly string[] NetworkKeywords =
    {
        "connection", "timeout", "timed out", "dns", "eof",
        "broken pipe", "network is unreachable", "refused", "reset",
    };

    /// <summary>判断错误是否应重试</summary>
    private static bool ShouldRetry(Exception error)
    {
        if (error is ArgumentException) return false;

        var msg = (error.Message ?? "").ToLowerInvariant();
        foreach (var kw in NetworkKeywords)
            if (msg.Contains(kw)) return true;

        if (msg.Contains("429") || msg.Contains("too many requests") || msg.Contains("rate limit"))
            return true;
        if (msg.Contains(": 4") || msg.Contains(": 5"))
            return true;
        if (error is System.Net.Http.HttpRequestException || error is TimeoutException)
            return true;

        return false;
    }

    /// <summary>带尝试次数的重试执行器，失败时不抛异常（见 Error 字段）</summary>
    public static Result<T> WithAttempts<T>(Func<T> fn, RetryConfig? config = null)
    {
        var cfg = config ?? new RetryConfig();
        Exception? lastError = null;

        for (var attempt = 0; attempt <= cfg.MaxRetries; attempt++)
        {
            var attempts = attempt + 1;
            try
            {
                var value = fn();
                return new Result<T> { Ok = true, Attempts = attempts, Value = value };
            }
            catch (Exception e)
            {
                lastError = e;
                if (attempt >= cfg.MaxRetries || !ShouldRetry(e))
                    return new Result<T> { Ok = false, Attempts = attempts, Error = e };

                var delay = Math.Min(cfg.InitialDelay * Math.Pow(2, attempt), cfg.MaxDelay);
                Thread.Sleep(TimeSpan.FromSeconds(delay));
            }
        }

        return new Result<T> { Ok = false, Attempts = cfg.MaxRetries + 1, Error = lastError };
    }

    /// <summary>带重试的操作执行器，重试耗尽后抛出最后一次异常</summary>
    public static T With<T>(Func<T> fn, RetryConfig? config = null)
    {
        var r = WithAttempts(fn, config);
        if (r.Ok) return r.Value!;
        throw r.Error!;
    }
}
