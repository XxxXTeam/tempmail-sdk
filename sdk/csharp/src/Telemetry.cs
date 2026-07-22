using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;

namespace XxxXTeam.TempMail;

/// <summary>
/// 匿名用量遥测。默认开启，可通过配置或环境变量关闭。
/// 上报内容仅含操作类型、渠道、成功与否、尝试次数等，邮箱地址会被脱敏。
/// </summary>
public static class Telemetry
{
    private const string DefaultUrl = "https://sdk-1.openel.top/v1/event";
    private const int MaxBatch = 32;
    private const double FlushSec = 2.0;

    private static readonly Regex EmailRe = new(@"[^\s@]{1,64}@[^\s@]{1,255}", RegexOptions.Compiled);
    private static readonly ConcurrentQueue<Dictionary<string, object?>> Queue = new();
    private static readonly object TimerLock = new();
    private static Timer? _flushTimer;
    private static int _periodicStarted;

    private static bool IsOn()
    {
        var v = Config.Get().TelemetryEnabled;
        return v ?? true;
    }

    private static string ResolveUrl()
    {
        var u = (Config.Get().TelemetryUrl ?? "").Trim();
        return string.IsNullOrEmpty(u) ? DefaultUrl : u;
    }

    private static string Sanitize(string? msg)
    {
        if (string.IsNullOrEmpty(msg)) return "";
        var redacted = EmailRe.Replace(msg, "[redacted]");
        return redacted.Length > 400 ? redacted[..400] : redacted;
    }

    private static string SdkVersion() => "0.1.0";

    /// <summary>上报一条遥测事件（非阻塞，失败静默）</summary>
    public static void Report(string operation, string channel, bool success,
        int attemptCount, int channelsTried, string error)
    {
        if (!IsOn()) return;
        EnsurePeriodic();

        var ev = new Dictionary<string, object?>
        {
            ["operation"] = operation,
            ["channel"] = channel,
            ["success"] = success,
            ["attempt_count"] = attemptCount,
            ["ts_ms"] = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds(),
        };
        if (channelsTried > 0) ev["channels_tried"] = channelsTried;
        var err = Sanitize(error);
        if (!string.IsNullOrEmpty(err)) ev["error"] = err;

        Queue.Enqueue(ev);
        if (Queue.Count >= MaxBatch) FlushBatch();
        else ArmFlushTimer();
    }

    private static void ArmFlushTimer()
    {
        lock (TimerLock)
        {
            // 复用单个 Timer，仅重置到期时间，避免每次入队都 Dispose+new Timer 分配
            if (_flushTimer is null)
                _flushTimer = new Timer(_ => FlushBatch(), null,
                    TimeSpan.FromSeconds(FlushSec), Timeout.InfiniteTimeSpan);
            else
                _flushTimer.Change(TimeSpan.FromSeconds(FlushSec), Timeout.InfiniteTimeSpan);
        }
    }

    private static void EnsurePeriodic()
    {
        if (Interlocked.Exchange(ref _periodicStarted, 1) == 1) return;
        _ = Task.Run(async () =>
        {
            while (true)
            {
                await Task.Delay(TimeSpan.FromSeconds(FlushSec)).ConfigureAwait(false);
                FlushBatch();
            }
        });
    }

    private static void FlushBatch()
    {
        if (!IsOn())
        {
            while (Queue.TryDequeue(out _)) { }
            return;
        }

        var events = new List<Dictionary<string, object?>>();
        while (Queue.TryDequeue(out var ev)) events.Add(ev);
        if (events.Count == 0) return;

        var batch = new Dictionary<string, object?>
        {
            ["schema_version"] = 2,
            ["sdk_language"] = "csharp",
            ["sdk_version"] = SdkVersion(),
            ["os"] = OsName(),
            ["arch"] = System.Runtime.InteropServices.RuntimeInformation.OSArchitecture.ToString().ToLowerInvariant(),
            ["events"] = events,
        };

        var body = Json.Serialize(batch);
        var url = ResolveUrl();
        var ver = SdkVersion();
        _ = Task.Run(() =>
        {
            try
            {
                Http.Post(url, body, "application/json; charset=utf-8",
                    new Dictionary<string, string> { ["User-Agent"] = $"tempmail-sdk-csharp/{ver}" }, 8);
            }
            catch { /* 遥测失败静默 */ }
        });
    }

    private static string OsName()
    {
        var r = System.Runtime.InteropServices.RuntimeInformation.OSDescription.ToLowerInvariant();
        if (r.Contains("windows")) return "windows";
        if (r.Contains("linux")) return "linux";
        if (r.Contains("darwin") || r.Contains("mac")) return "darwin";
        return "unknown";
    }
}
