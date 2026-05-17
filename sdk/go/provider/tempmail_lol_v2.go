package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"net/url"
)

/**
 * tempmail.lol V2 渠道实现
 * API: https://api.tempmail.lol
 */

const tempmailLolV2BaseURL = "https://api.tempmail.lol"

/*
 * TempmailLolV2Generate 创建临时邮箱
 * API: GET /generate → {"address":"...","token":"..."}
 */
func TempmailLolV2Generate() (*CreatedMailbox, error) {
	client := HTTPClient()
	resp, err := client.Get(tempmailLolV2BaseURL + "/generate")
	if err != nil {
		return nil, fmt.Errorf("tempmail-lol-v2 generate request failed: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != 200 {
		return nil, fmt.Errorf("tempmail-lol-v2 generate failed: %d", resp.StatusCode)
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("tempmail-lol-v2 read body failed: %w", err)
	}

	var result struct {
		Address string `json:"address"`
		Token   string `json:"token"`
	}

	if err := json.Unmarshal(body, &result); err != nil {
		return nil, fmt.Errorf("tempmail-lol-v2 parse response failed: %w", err)
	}

	if result.Address == "" || result.Token == "" {
		return nil, fmt.Errorf("tempmail-lol-v2: missing address or token")
	}

	return &CreatedMailbox{
		Channel: "tempmail-lol-v2",
		Email:   result.Address,
		Token:   result.Token,
	}, nil
}

/*
 * TempmailLolV2GetEmails 获取邮件列表
 * API: GET /auth/<token> → {"email":[...]}
 */
func TempmailLolV2GetEmails(token string, email string) ([]NormEmail, error) {
	u := tempmailLolV2BaseURL + "/auth/" + url.PathEscape(token)
	client := HTTPClient()
	resp, err := client.Get(u)
	if err != nil {
		return nil, fmt.Errorf("tempmail-lol-v2 get emails request failed: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != 200 {
		return nil, fmt.Errorf("tempmail-lol-v2 get emails failed: %d", resp.StatusCode)
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("tempmail-lol-v2 read body failed: %w", err)
	}

	var result struct {
		Email []json.RawMessage `json:"email"`
	}

	if err := json.Unmarshal(body, &result); err != nil {
		return nil, fmt.Errorf("tempmail-lol-v2 parse response failed: %w", err)
	}

	if len(result.Email) == 0 {
		return []NormEmail{}, nil
	}

	out := make([]NormEmail, 0, len(result.Email))
	for _, raw := range result.Email {
		var item map[string]interface{}
		if err := json.Unmarshal(raw, &item); err != nil {
			continue
		}

		flat := map[string]interface{}{
			"id":      item["id"],
			"from":    item["from"],
			"to":      email,
			"subject": item["subject"],
			"text":    firstNonEmpty(item, "body", "text"),
			"html":    firstNonEmpty(item, "html", "body"),
			"date":    firstNonEmpty(item, "date", "receivedAt"),
			"isRead":  false,
		}
		if flat["from"] == nil || flat["from"] == "" {
			flat["from"] = item["sender"]
		}

		out = append(out, NormalizeMap(flat, email))
	}
	return out, nil
}

/* firstNonEmpty 从 map 中按候选 key 取第一个非空字符串 */
func firstNonEmpty(m map[string]interface{}, keys ...string) interface{} {
	for _, k := range keys {
		if v, ok := m[k]; ok {
			if s, ok := v.(string); ok && s != "" {
				return s
			}
		}
	}
	return ""
}
