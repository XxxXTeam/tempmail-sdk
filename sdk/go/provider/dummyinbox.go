package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"net/url"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

const dummyInboxBase = "https://dummyinbox.com/api/v1"

/* dummyInboxPathAddress 按官方文档保留地址中的 @ 原样拼入路径 */
func dummyInboxPathAddress(email string) string {
	return strings.ReplaceAll(url.PathEscape(strings.TrimSpace(email)), "%40", "@")
}

func dummyInboxGetJSON(path string) (map[string]any, error) {
	req, err := http.NewRequest("GET", dummyInboxBase+path, nil)
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
		return nil, fmt.Errorf("dummyinbox: HTTP %d", resp.StatusCode)
	}
	var raw map[string]any
	if err := json.Unmarshal(body, &raw); err != nil {
		return nil, err
	}
	return raw, nil
}

/* DummyInboxGenerate 创建随机地址；地址本身即读取凭据 */
func DummyInboxGenerate() (*CreatedMailbox, error) {
	req, err := http.NewRequest("POST", dummyInboxBase+"/inboxes", nil)
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
		return nil, fmt.Errorf("dummyinbox create: %d", resp.StatusCode)
	}
	var out struct {
		Address string `json:"address"`
	}
	if err := json.Unmarshal(body, &out); err != nil {
		return nil, err
	}
	if strings.TrimSpace(out.Address) == "" {
		return nil, fmt.Errorf("dummyinbox: empty address")
	}
	return &CreatedMailbox{Channel: "dummyinbox", Email: out.Address}, nil
}

func dummyInboxFlatten(raw map[string]any, recipient string) map[string]any {
	flat := map[string]any{
		"id":      raw["id"],
		"from":    raw["from"],
		"to":      raw["to"],
		"subject": raw["subject"],
		"text":    raw["text"],
		"html":    raw["html"],
		"date":    raw["receivedAt"],
	}
	if flat["to"] == nil || fmt.Sprint(flat["to"]) == "" {
		flat["to"] = recipient
	}
	return flat
}

/* DummyInboxGetEmails 先取摘要，再逐条读取完整正文与附件 */
func DummyInboxGetEmails(email string) ([]NormEmail, error) {
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("dummyinbox: empty email")
	}
	pathAddr := dummyInboxPathAddress(email)
	list, err := dummyInboxGetJSON(fmt.Sprintf("/inboxes/%s/messages?limit=50", pathAddr))
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
		detail, err := dummyInboxGetJSON(fmt.Sprintf("/inboxes/%s/messages/%s", pathAddr, url.PathEscape(id)))
		if err == nil {
			if message, ok := detail["message"].(map[string]any); ok {
				out = append(out, NormalizeMap(dummyInboxFlatten(message, email), email))
				continue
			}
		}
		/* 详情失败时至少返回摘要中的主题与片段 */
		out = append(out, NormalizeMap(map[string]any{
			"id":      id,
			"from":    summary["from"],
			"to":      email,
			"subject": summary["subject"],
			"text":    summary["snippet"],
			"date":    summary["receivedAt"],
		}, email))
	}
	return out, nil
}
