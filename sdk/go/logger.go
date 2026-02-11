package tempemail

import (
	"log/slog"
	"os"
)

/*
 * SDK 日志模块
 * 基于 Go 标准库 slog 实现分级日志
 * 默认静默不输出，用户可通过 SetLogLevel / SetLogger 启用
 *
 * 示例:
 *   tempemail.SetLogLevel(tempemail.LogLevelDebug) // 开启所有日志
 *   tempemail.SetLogLevel(tempemail.LogLevelInfo)  // 只输出 INFO 及以上
 */

/*
 * LogLevelSilent 等日志级别常量
 * 对应 slog 的级别体系，Silent 使用极高值屏蔽所有输出
 */
const (
	LogLevelDebug  = slog.LevelDebug
	LogLevelInfo   = slog.LevelInfo
	LogLevelWarn   = slog.LevelWarn
	LogLevelError  = slog.LevelError
	LogLevelSilent = slog.Level(100)
)

/* sdkLogger SDK 内部使用的 logger 实例 */
var sdkLogger *slog.Logger

func init() {
	/* 默认静默：日志级别设为极高值，不输出任何日志 */
	sdkLogger = slog.New(slog.NewTextHandler(os.Stderr, &slog.HandlerOptions{
		Level: LogLevelSilent,
	})).With("module", "tempmail-sdk")
}

/*
 * SetLogLevel 设置日志级别
 * 默认 LogLevelSilent（不输出任何日志）
 *
 * 示例:
 *   tempemail.SetLogLevel(tempemail.LogLevelDebug) // 开启所有日志
 *   tempemail.SetLogLevel(tempemail.LogLevelInfo)  // 只输出 INFO 及以上
 */
func SetLogLevel(level slog.Level) {
	sdkLogger = slog.New(slog.NewTextHandler(os.Stderr, &slog.HandlerOptions{
		Level: level,
	})).With("module", "tempmail-sdk")
}

/*
 * SetLogger 设置自定义 slog.Logger 实例
 * 替换默认的 stderr 文本输出，可对接任意日志框架
 *
 * 示例:
 *   handler := slog.NewJSONHandler(os.Stdout, &slog.HandlerOptions{Level: slog.LevelDebug})
 *   tempemail.SetLogger(slog.New(handler))
 */
func SetLogger(l *slog.Logger) {
	sdkLogger = l
}

/* GetLogger 获取当前 SDK 使用的 logger 实例 */
func GetLogger() *slog.Logger {
	return sdkLogger
}
