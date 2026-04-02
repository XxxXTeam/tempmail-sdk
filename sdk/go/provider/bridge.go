/*
 * 桥接：根包 tempemail 在 init 中注入 HTTP/归一化等实现，
 * 避免 provider import tempemail 造成循环依赖（Go 单模块内子包不可与根包互引）。
 */
package provider

import (
	"encoding/json"

	http "github.com/bogdanfinn/fhttp"
	tls_client "github.com/bogdanfinn/tls-client"
)

// ConfigSnapshot 与 tempemail.SDKConfig 字段对齐，供 Dropmail 等只读配置
type ConfigSnapshot struct {
	Proxy                    string
	Timeout                  int64 // 纳秒，0 表示未设置
	Insecure                 bool
	DropmailAuthToken        string
	DropmailDisableAutoToken bool
	DropmailRenewLifetime    string
}

// CreatedMailbox 创建邮箱的统一中间结果，由 tempemail 转为 EmailInfo
type CreatedMailbox struct {
	Channel   string
	Email     string
	Token     string
	ExpiresAt any
	CreatedAt string
}

// NormEmail / NormAttachment 与 tempemail.Email 结构一致，便于根包转换
type NormAttachment struct {
	Filename    string
	Size        int64
	ContentType string
	URL         string
}

type NormEmail struct {
	ID          string
	From        string
	To          string
	Subject     string
	Text        string
	HTML        string
	Date        string
	IsRead      bool
	Attachments []NormAttachment
}

var (
	// HTTPClient 由 tempemail.init 注入
	HTTPClient func() tls_client.HttpClient
	// HTTPClientNoRedirect 由 tempemail.init 注入（不跟随重定向）
	HTTPClientNoRedirect func() tls_client.HttpClient
	// CheckHTTPStatus 由 tempemail.init 注入（状态码 >=400 返回 error）
	CheckHTTPStatus func(*http.Response, string) error
	// GetCurrentUA 由 tempemail.init 注入
	GetCurrentUA func() string
	// GetConfigSnapshot 由 tempemail.init 注入
	GetConfigSnapshot func() ConfigSnapshot
	// NormalizeMap 将原始 map 转为 NormEmail
	NormalizeMap func(raw map[string]interface{}, recipientEmail string) NormEmail
	// NormalizeRawMessages 将 JSON 消息列表转为 NormEmail（由 tempemail.init 注入）
	NormalizeRawMessages func([]json.RawMessage, string) ([]NormEmail, error)
)

func normEmailsFromMaps(maps []map[string]interface{}, recipient string) []NormEmail {
	out := make([]NormEmail, 0, len(maps))
	for _, m := range maps {
		out = append(out, NormalizeMap(m, recipient))
	}
	return out
}
