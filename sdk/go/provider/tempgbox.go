package provider

import (
	"bytes"
	cryptorand "crypto/rand"
	"encoding/base64"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"io"
	"math/rand"
	"regexp"

	http "github.com/bogdanfinn/fhttp"
)

const tempgboxAPIURL = "https://tempgbox.net/api/proxy"

var tempgboxDataXRe = regexp.MustCompile(`data-x=["']([^"']+)["']`)

type tempgboxAlias struct {
	ID        any    `json:"id"`
	Alias     string `json:"alias"`
	Email     string `json:"email"`
	BaseEmail string `json:"base_email"`
	CreatedAt string `json:"created_at"`
	ExpiresAt string `json:"expires_at"`
}

type tempgboxPayload struct {
	Alias    *tempgboxAlias   `json:"alias"`
	Messages []map[string]any `json:"messages"`
	Detail   string           `json:"detail"`
	Error    string           `json:"error"`
	Message  string           `json:"message"`
}

func tempgboxRandomDeviceID() string {
	// 上游按 X-Device-ID 限制连续生成；每个新邮箱使用新的设备 ID，后续收件复用该邮箱 token。
	buf := make([]byte, 32)
	if _, err := cryptorand.Read(buf); err == nil {
		return hex.EncodeToString(buf)
	}
	return randomStr(64)
}

func tempgboxRandomIP() string {
	return fmt.Sprintf("%d.%d.%d.%d", rand.Intn(254)+1, rand.Intn(254)+1, rand.Intn(254)+1, rand.Intn(254)+1)
}

func tempgboxHeaders(deviceID string) []string {
	ip := tempgboxRandomIP()
	return []string{
		"Accept: text/html,application/json",
		"Content-Type: application/json",
		"Origin: https://tempgbox.net",
		"Referer: https://tempgbox.net/",
		"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
		"X-Device-ID: " + deviceID,
		"X-Forwarded-For: " + ip,
		"X-Real-IP: " + ip,
		"X-Originating-IP: " + ip,
	}
}

func tempgboxDecodePayload(body []byte) (*tempgboxPayload, error) {
	match := tempgboxDataXRe.FindSubmatch(body)
	if len(match) < 2 {
		return nil, fmt.Errorf("tempgbox: missing encoded response payload")
	}
	raw, err := base64.StdEncoding.DecodeString(string(match[1]))
	if err != nil {
		return nil, fmt.Errorf("tempgbox: decode payload failed: %w", err)
	}
	var payload tempgboxPayload
	if err := json.Unmarshal(raw, &payload); err != nil {
		return nil, fmt.Errorf("tempgbox: parse payload failed: %w", err)
	}
	return &payload, nil
}

func tempgboxPost(route string, deviceID string, body any) (*tempgboxPayload, int, error) {
	reqBody, err := json.Marshal(body)
	if err != nil {
		return nil, 0, err
	}
	req, err := http.NewRequest("POST", tempgboxAPIURL+"?route="+route, bytes.NewReader(reqBody))
	if err != nil {
		return nil, 0, err
	}
	for _, h := range tempgboxHeaders(deviceID) {
		if k, v, ok := stringsCut(h, ": "); ok {
			req.Header.Set(k, v)
		}
	}

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, 0, err
	}
	defer resp.Body.Close()

	respBody, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, resp.StatusCode, err
	}
	payload, err := tempgboxDecodePayload(respBody)
	if err != nil {
		return nil, resp.StatusCode, err
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		reason := payload.Detail
		if reason == "" {
			reason = payload.Error
		}
		if reason == "" {
			reason = payload.Message
		}
		return payload, resp.StatusCode, fmt.Errorf("tempgbox %s failed: %d %s", route, resp.StatusCode, reason)
	}
	return payload, resp.StatusCode, nil
}

func stringsCut(s, sep string) (string, string, bool) {
	for i := 0; i+len(sep) <= len(s); i++ {
		if s[i:i+len(sep)] == sep {
			return s[:i], s[i+len(sep):], true
		}
	}
	return s, "", false
}

func TempgboxGenerate() (*CreatedMailbox, error) {
	deviceID := tempgboxRandomDeviceID()
	payload, _, err := tempgboxPost("generate", deviceID, map[string]string{"variant": "googlemail"})
	if err != nil {
		return nil, err
	}
	if payload.Alias == nil {
		return nil, fmt.Errorf("tempgbox: missing alias")
	}
	email := payload.Alias.Email
	if email == "" {
		email = payload.Alias.Alias
	}
	if email == "" {
		return nil, fmt.Errorf("tempgbox: missing email")
	}
	return &CreatedMailbox{
		Channel:   "tempgbox",
		Email:     email,
		Token:     deviceID,
		CreatedAt: payload.Alias.CreatedAt,
		ExpiresAt: payload.Alias.ExpiresAt,
	}, nil
}

func TempgboxGetEmails(deviceID string, email string) ([]NormEmail, error) {
	if deviceID == "" {
		return nil, fmt.Errorf("tempgbox: missing device id")
	}
	payload, _, err := tempgboxPost("inbox", deviceID, map[string]string{"email": email})
	if err != nil {
		return nil, err
	}
	out := make([]NormEmail, 0, len(payload.Messages))
	for _, raw := range payload.Messages {
		if raw["from"] == nil {
			raw["from"] = raw["sender"]
		}
		if raw["text"] == nil {
			raw["text"] = raw["body_text"]
		}
		if raw["html"] == nil {
			raw["html"] = raw["body_html"]
		}
		if raw["date"] == nil {
			raw["date"] = raw["received_at"]
		}
		if raw["messageId"] == nil {
			raw["messageId"] = raw["message_id"]
		}
		if raw["attachments"] == nil {
			raw["attachments"] = raw["attachments_info"]
		}
		out = append(out, NormalizeMap(raw, email))
	}
	return out, nil
}
