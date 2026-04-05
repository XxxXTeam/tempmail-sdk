package tempemail

import (
 	"net/http"
	"regexp"
	"strings"
	"time"
)

/*
 * 匿名用量上报
 *
 * 环境变量（进程启动时读入全局默认，可被 SetConfig 覆盖）：
 *   TEMPMAIL_TELEMETRY_ENABLED - 未设置则默认开启；false/0/no 关闭，true/1/yes 显式开启
 *   TEMPMAIL_TELEMETRY_URL      - 覆盖上报端点 URL
 *
 * 代码：SDKConfig.TelemetryEnabled 为 nil 表示沿用默认（开启）；指向 false 则关闭
 */

const defaultTelemetryURL = "https://sdk-1.openel.top/v1/event"

var telemetryEmailRedact = regexp.MustCompile(`[^\s@]{1,64}@[^\s@]{1,255}`)

func telemetryURLResolved(cfg SDKConfig) string {
	if u := strings.TrimSpace(cfg.TelemetryEndpoint); u != "" {
		return u
	}
	return defaultTelemetryURL
}

func telemetryOn(cfg SDKConfig) bool {
	if cfg.TelemetryEnabled == nil {
		return true
	}
	return *cfg.TelemetryEnabled
}

func sanitizeTelemetryError(s string) string {
	if s == "" {
		return ""
	}
	s = telemetryEmailRedact.ReplaceAllString(s, "[redacted]")
	if len(s) > 400 {
		return s[:400]
	}
	return s
}

var telemetryHTTP = &http.Client{Timeout: 8 * time.Second}

/*
 * reportTelemetry 将事件入队，与同一进程内其它事件合并后统一 POST（见 telemetry_batch.go）
 */
func reportTelemetry(operation, channel string, success bool, attemptCount, channelsTried int, errMsg string) {
	enqueueTelemetryEvent(operation, channel, success, attemptCount, channelsTried, errMsg)
}
