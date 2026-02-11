package tempemail

import (
	"fmt"
	"math"
	"net/http"
	"strings"
	"time"
)

/*
 * RetryOptions 重试配置选项
 * SDK 内部对网络错误、超时、服务端 5xx 错误自动重试
 * 4xx 客户端错误（如参数错误）不会重试
 *
 * 示例:
 *   opts := &RetryOptions{MaxRetries: 3, InitialDelay: 2 * time.Second}
 *   info, err := GenerateEmail(&GenerateEmailOptions{Channel: ChannelMailTm, Retry: opts})
 */
type RetryOptions struct {
	/* 最大重试次数（不含首次请求），默认 2 */
	MaxRetries int
	/* 初始重试延迟，采用指数退避策略，默认 1s */
	InitialDelay time.Duration
	/* 最大重试延迟上限，默认 5s */
	MaxDelay time.Duration
	/* 单次请求超时时间，默认 15s */
	Timeout time.Duration
}

/* 默认重试配置 */
var defaultRetryOptions = RetryOptions{
	MaxRetries:   2,
	InitialDelay: 1 * time.Second,
	MaxDelay:     5 * time.Second,
	Timeout:      15 * time.Second,
}

/*
 * mergeRetryOptions 合并用户配置和默认配置
 * 用户未设置的字段（零值）会回退到默认值
 */
func mergeRetryOptions(opts *RetryOptions) RetryOptions {
	merged := defaultRetryOptions
	if opts == nil {
		return merged
	}
	if opts.MaxRetries > 0 {
		merged.MaxRetries = opts.MaxRetries
	}
	if opts.InitialDelay > 0 {
		merged.InitialDelay = opts.InitialDelay
	}
	if opts.MaxDelay > 0 {
		merged.MaxDelay = opts.MaxDelay
	}
	if opts.Timeout > 0 {
		merged.Timeout = opts.Timeout
	}
	return merged
}

/*
 * shouldRetry 判断错误是否应该重试
 *
 * 可重试的错误类型:
 * - 网络连接错误（connection refused, connection reset, broken pipe）
 * - 超时错误（timeout, timed out, i/o timeout）
 * - DNS 解析失败（no such host, dns）
 * - HTTP 4xx/5xx 错误
 *
 * 不可重试的错误:
 * - 响应解析错误、参数校验错误等 SDK 内部错误
 */
func shouldRetry(err error) bool {
	if err == nil {
		return false
	}

	msg := strings.ToLower(err.Error())

	/* 网络级别错误 → 重试 */
	networkKeywords := []string{
		"connection refused", "connection reset",
		"timeout", "timed out",
		"no such host", "dns",
		"eof", "broken pipe",
		"network is unreachable",
		"i/o timeout",
	}
	for _, kw := range networkKeywords {
		if strings.Contains(msg, kw) {
			return true
		}
	}

	/* HTTP 4xx/5xx 错误 → 重试 */
	if strings.Contains(msg, ": 4") || strings.Contains(msg, ": 5") {
		return true
	}

	return false
}

/*
 * WithRetry 带重试的泛型操作执行器
 *
 * 功能:
 * - 自动重试可恢复的错误（网络错误、超时、服务端 5xx）
 * - 指数退避策略避免短时间内过度请求
 * - 不可恢复的错误（4xx 等）直接返回，不浪费重试次数
 *
 * 参数:
 * - fn:   要执行的操作函数
 * - opts: 重试配置，nil 则使用默认值
 */
func WithRetry[T any](fn func() (T, error), opts *RetryOptions) (T, error) {
	merged := mergeRetryOptions(opts)
	var lastErr error
	var zero T

	for attempt := 0; attempt <= merged.MaxRetries; attempt++ {
		result, err := fn()
		if err == nil {
			if attempt > 0 {
				sdkLogger.Info("重试成功", "attempt", attempt+1)
			}
			return result, nil
		}

		lastErr = err
		errorMsg := err.Error()

		/* 最后一次尝试失败或不可重试的错误 → 直接返回 */
		if attempt >= merged.MaxRetries || !shouldRetry(err) {
			if attempt >= merged.MaxRetries && merged.MaxRetries > 0 {
				sdkLogger.Error("重试耗尽后仍失败", "retries", merged.MaxRetries, "error", errorMsg)
			} else if !shouldRetry(err) {
				sdkLogger.Debug("不可重试的错误", "error", errorMsg)
			}
			return zero, err
		}

		/* 指数退避等待 */
		delay := time.Duration(float64(merged.InitialDelay) * math.Pow(2, float64(attempt)))
		if delay > merged.MaxDelay {
			delay = merged.MaxDelay
		}
		sdkLogger.Warn("请求失败，即将重试", "error", errorMsg, "delay", delay.String(), "attempt", attempt+2)
		time.Sleep(delay)
	}

	return zero, lastErr
}

/*
 * httpClientWithTimeout 创建带超时控制的 HTTP 客户端
 * 超时时间从 RetryOptions 中读取，默认 15 秒
 */
func httpClientWithTimeout(opts *RetryOptions) *http.Client {
	merged := mergeRetryOptions(opts)
	return &http.Client{
		Timeout: merged.Timeout,
	}
}

/*
 * checkHTTPStatus 检查 HTTP 响应状态码
 * 返回包含状态码的格式化错误，便于 shouldRetry 识别 5xx 进行重试
 * 状态码 < 400 时返回 nil
 */
func checkHTTPStatus(resp *http.Response, action string) error {
	if resp.StatusCode >= 400 {
		return fmt.Errorf("%s: %d", action, resp.StatusCode)
	}
	return nil
}
