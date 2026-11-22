package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"net/url"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

const plainlyKitBase = "https://plainlykit.com/tools/temporary-email/api"

/* plainlyKitTokenSep 在内部令牌中分隔邮箱 ID 与读取令牌 */
const plainlyKitTokenSep = "|"

func plainlyKitRequest(method, path, token string, body io.Reader) (map[string]any, error) {
	req, err := http.NewRequest(method, plainlyKitBase+path, body)
	if err != nil {
		return nil, err
	}
	req.Header.Set("Accept", "application/json")
	if body != nil {
		req.Header.Set("Content-Type", "application/json")
	}
	if token != "" {
		req.Header.Set("Authorization", "Bearer "+token)
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
		return nil, fmt.Errorf("plainlykit: HTTP %d", resp.StatusCode)
	}
	var out map[string]any
	if err := json.Unmarshal(raw, &out); err != nil {
		return nil, err
	}
	return out, nil
}

func plainlyKitSplitToken(token string) (id, bearer string, err error) {
	parts := strings.SplitN(strings.TrimSpace(token), plainlyKitTokenSep, 2)
	if len(parts) != 2 || parts[0] == "" || parts[1] == "" {
		return "", "", fmt.Errorf("plainlykit: malformed internal token")
	}
	return parts[0], parts[1], nil
}

/* PlainlyKitGenerate 创建邮箱；内部令牌打包邮箱 ID 与读取令牌，令牌仅返回一次 */
func PlainlyKitGenerate() (*CreatedMailbox, error) {
	raw, err := plainlyKitRequest("POST", "/mailboxes", "", strings.NewReader("{}"))
	if err != nil {
		return nil, err
	}
	address := strings.TrimSpace(fmt.Sprint(raw["address"]))
	id := strings.TrimSpace(fmt.Sprint(raw["id"]))
	token := strings.TrimSpace(fmt.Sprint(raw["token"]))
	if address == "" || id == "" || token == "" {
		return nil, fmt.Errorf("plainlykit: incomplete creation response")
	}
	return &CreatedMailbox{
		Channel:   "plainlykit",
		Email:     address,
		Token:     id + plainlyKitTokenSep + token,
		ExpiresAt: raw["expiresAt"],
		CreatedAt: fmt.Sprint(raw["createdAt"]),
	}, nil
}

/* plainlyKitFlatten 将详情响应映射到统一候选字段 */
func plainlyKitFlatten(raw map[string]any, recipient string) map[string]any {
	return map[string]any{
		"id":      raw["id"],
		"from":    raw["sender"],
		"to":      recipient,
		"subject": raw["subject"],
		"text":    raw["text"],
		"html":    raw["html"],
		"date":    raw["receivedAt"],
	}
}

/* PlainlyKitGetEmails 列表需 Bearer 令牌，详情补全纯文本正文 */
func PlainlyKitGetEmails(email, token string) ([]NormEmail, error) {
	id, bearer, err := plainlyKitSplitToken(token)
	if err != nil {
		return nil, err
	}
	basePath := "/mailboxes/" + url.PathEscape(id) + "/messages"
	list, err := plainlyKitRequest("GET", basePath, bearer, nil)
	if err != nil {
		return nil, err
	}
	items, _ := list["messages"].([]any)
	out := make([]NormEmail, 0, len(items))
	for _, item := range items {
		summary, _ := item.(map[string]any)
		messageID := strings.TrimSpace(fmt.Sprint(summary["id"]))
		if messageID == "" {
			continue
		}
		detail, detailErr := plainlyKitRequest("GET", basePath+"/"+url.PathEscape(messageID), bearer, nil)
		if detailErr == nil {
			flat := plainlyKitFlatten(detail, email)
			if fmt.Sprint(flat["subject"]) == "" {
				flat["subject"] = summary["subject"]
			}
			if fmt.Sprint(flat["from"]) == "" {
				flat["from"] = summary["sender"]
			}
			out = append(out, NormalizeMap(flat, email))
			continue
		}
		/* 详情失败时退回摘要字段 */
		out = append(out, NormalizeMap(map[string]any{
			"id":      messageID,
			"from":    summary["sender"],
			"to":      email,
			"subject": summary["subject"],
			"text":    summary["preview"],
			"date":    summary["receivedAt"],
		}, email))
	}
	return out, nil
}
