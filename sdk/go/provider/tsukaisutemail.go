package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"net/url"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

const tsukaisuteMailBase = "https://tsukaisutemail.com/api/v1"

func tsukaisuteMailGetJSON(path string) (map[string]any, error) {
	req, err := http.NewRequest("GET", tsukaisuteMailBase+path, nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("Accept", "application/json")
	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("tsukaisutemail: HTTP %d", resp.StatusCode)
	}
	var raw map[string]any
	if err := json.Unmarshal(body, &raw); err != nil {
		return nil, err
	}
	return raw, nil
}

/* TsukaisuteMailGenerate 创建随机地址，地址即读取凭据 */
func TsukaisuteMailGenerate() (*CreatedMailbox, error) {
	req, err := http.NewRequest("POST", tsukaisuteMailBase+"/address", nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("Accept", "application/json")
	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("tsukaisutemail create: %d", resp.StatusCode)
	}
	var out struct {
		Address string `json:"address"`
	}
	if err := json.Unmarshal(body, &out); err != nil {
		return nil, err
	}
	if strings.TrimSpace(out.Address) == "" {
		return nil, fmt.Errorf("tsukaisutemail: empty address")
	}
	return &CreatedMailbox{Channel: "tsukaisutemail", Email: out.Address}, nil
}

func tsukaisuteMailFlatten(raw map[string]any, recipient string) map[string]any {
	return map[string]any{
		"id":      raw["id"],
		"from":    raw["from_address"],
		"to":      recipient,
		"subject": raw["subject"],
		"text":    raw["text_body"],
		"html":    raw["html_body"],
		"date":    raw["received_at"],
	}
}

/* TsukaisuteMailGetEmails 先取列表，再按消息 ID 读取完整正文 */
func TsukaisuteMailGetEmails(email string) ([]NormEmail, error) {
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("tsukaisutemail: empty email")
	}
	list, err := tsukaisuteMailGetJSON("/inbox/" + url.PathEscape(email))
	if err != nil {
		return nil, err
	}
	items, _ := list["messages"].([]any)
	out := make([]NormEmail, 0, len(items))
	for _, item := range items {
		summary, _ := item.(map[string]any)
		id := strings.TrimSpace(fmt.Sprint(summary["id"]))
		if id == "" {
			continue
		}
		detail, detailErr := tsukaisuteMailGetJSON("/message/" + url.PathEscape(id))
		if detailErr == nil {
			if message, ok := detail["message"].(map[string]any); ok {
				out = append(out, NormalizeMap(tsukaisuteMailFlatten(message, email), email))
				continue
			}
		}
		out = append(out, NormalizeMap(map[string]any{
			"id":      id,
			"from":    summary["from_address"],
			"to":      email,
			"subject": summary["subject"],
			"text":    summary["snippet"],
			"date":    summary["received_at"],
		}, email))
	}
	return out, nil
}
