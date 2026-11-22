package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"net/url"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

const tempmailifyBase = "https://tempmailify.com"

func tempmailifyRequest(method, path, bearer string, body io.Reader) (map[string]any, error) {
	req, err := http.NewRequest(method, tempmailifyBase+path, body)
	if err != nil {
		return nil, err
	}
	req.Header.Set("Accept", "application/json")
	if body != nil {
		req.Header.Set("Content-Type", "application/json")
	}
	if bearer != "" {
		req.Header.Set("Authorization", "Bearer "+bearer)
	}
	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("tempmailify: HTTP %d", resp.StatusCode)
	}
	var out map[string]any
	if err := json.Unmarshal(raw, &out); err != nil {
		return nil, err
	}
	return out, nil
}

/* TempmailifyGenerate 创建收件箱；每箱拥有独立读取令牌 */
func TempmailifyGenerate() (*CreatedMailbox, error) {
	raw, err := tempmailifyRequest("POST", "/api/inbox", "", strings.NewReader("{}"))
	if err != nil {
		return nil, err
	}
	address := strings.TrimSpace(fmt.Sprint(raw["address"]))
	token := strings.TrimSpace(fmt.Sprint(raw["token"]))
	if address == "" || token == "" {
		return nil, fmt.Errorf("tempmailify: incomplete creation response")
	}
	return &CreatedMailbox{
		Channel:   "tempmailify",
		Email:     address,
		Token:     token,
		ExpiresAt: raw["expiresAt"],
		CreatedAt: fmt.Sprint(raw["createdAt"]),
	}, nil
}

/* tempmailifyFrom 提取发件人地址；列表/详情可能返回对象或字符串 */
func tempmailifyFrom(value any) string {
	if from, ok := value.(map[string]any); ok {
		return strings.TrimSpace(fmt.Sprint(from["address"]))
	}
	return strings.TrimSpace(fmt.Sprint(value))
}

func tempmailifyFlatten(raw map[string]any, recipient string) map[string]any {
	return map[string]any{
		"id":      raw["id"],
		"from":    tempmailifyFrom(raw["from"]),
		"to":      recipient,
		"subject": raw["subject"],
		"text":    raw["text"],
		"html":    raw["html"],
		"date":    raw["receivedAt"],
		"code":    raw["code"],
	}
}

/* TempmailifyGetEmails 列表与详情均需收件箱令牌，详情提供完整 text/html */
func TempmailifyGetEmails(email, token string) ([]NormEmail, error) {
	email = strings.TrimSpace(email)
	token = strings.TrimSpace(token)
	if email == "" || token == "" {
		return nil, fmt.Errorf("tempmailify: empty email or token")
	}
	basePath := "/api/inbox/" + url.PathEscape(email)
	list, err := tempmailifyRequest("GET", basePath, token, nil)
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
		detail, detailErr := tempmailifyRequest("GET", basePath+"/messages/"+url.PathEscape(id), token, nil)
		if detailErr == nil {
			flat := tempmailifyFlatten(detail, email)
			if fmt.Sprint(flat["subject"]) == "" {
				flat["subject"] = summary["subject"]
			}
			out = append(out, NormalizeMap(flat, email))
			continue
		}
		out = append(out, NormalizeMap(map[string]any{
			"id":      id,
			"from":    tempmailifyFrom(summary["from"]),
			"to":      email,
			"subject": summary["subject"],
			"text":    summary["preview"],
			"date":    summary["receivedAt"],
			"seen":    summary["seen"],
		}, email))
	}
	return out, nil
}
