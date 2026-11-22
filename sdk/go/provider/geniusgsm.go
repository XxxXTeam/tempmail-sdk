package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"net/url"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

const geniusGSMBase = "https://tempmail.geniusgsm.com/api"

func geniusGSMRequest(method, path string, body io.Reader) (any, error) {
	req, err := http.NewRequest(method, geniusGSMBase+path, body)
	if err != nil {
		return nil, err
	}
	req.Header.Set("Accept", "application/json")
	if body != nil {
		req.Header.Set("Content-Type", "application/json")
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
		return nil, fmt.Errorf("geniusgsm: HTTP %d", resp.StatusCode)
	}
	var out any
	if err := json.Unmarshal(raw, &out); err != nil {
		return nil, err
	}
	return out, nil
}

/* geniusGSMRows 兼容数组与分页对象两种列表响应 */
func geniusGSMRows(raw any, key string) []any {
	if obj, ok := raw.(map[string]any); ok {
		if rows, ok := obj[key].([]any); ok {
			return rows
		}
		if rows, ok := obj["results"].([]any); ok {
			return rows
		}
		return nil
	}
	if rows, ok := raw.([]any); ok {
		return rows
	}
	return nil
}

/* GeniusGSMGenerate 选择激活域名并创建收件箱；内部令牌即收件箱 ID */
func GeniusGSMGenerate() (*CreatedMailbox, error) {
	domains, err := geniusGSMRequest("GET", "/domains/", nil)
	if err != nil {
		return nil, err
	}
	domainID := 0
	for _, item := range geniusGSMRows(domains, "results") {
		info, _ := item.(map[string]any)
		active, _ := info["is_active"].(bool)
		if active {
			domainID = int(info["id"].(float64))
			break
		}
	}
	if domainID == 0 {
		return nil, fmt.Errorf("geniusgsm: no active domain")
	}

	body := strings.NewReader(fmt.Sprintf(`{"domain_id":%d,"ttl":3600}`, domainID))
	raw, err := geniusGSMRequest("POST", "/inboxes/create_inbox/", body)
	if err != nil {
		return nil, err
	}
	mailbox, _ := raw.(map[string]any)
	id := strings.TrimSpace(fmt.Sprint(mailbox["id"]))
	email := strings.TrimSpace(fmt.Sprint(mailbox["email"]))
	if id == "" || email == "" {
		return nil, fmt.Errorf("geniusgsm: incomplete creation response")
	}
	return &CreatedMailbox{
		Channel:   "geniusgsm",
		Email:     email,
		Token:     id,
		ExpiresAt: mailbox["expires_at"],
		CreatedAt: fmt.Sprint(mailbox["created_at"]),
	}, nil
}

func geniusGSMFlatten(raw map[string]any, recipient string) map[string]any {
	from := strings.TrimSpace(fmt.Sprint(raw["from_email"]))
	if name := strings.TrimSpace(fmt.Sprint(raw["from_name"])); name != "" && from != "" {
		from = fmt.Sprintf("%s <%s>", name, from)
	}
	to := raw["inbox_email"]
	if fmt.Sprint(to) == "" || to == nil {
		to = recipient
	}
	return map[string]any{
		"id":          raw["id"],
		"from":        from,
		"to":          to,
		"subject":     raw["subject"],
		"text":        raw["body_text"],
		"html":        raw["body_html"],
		"date":        raw["received_at"],
		"read":        raw["is_read"],
		"attachments": raw["attachments"],
	}
}

/* GeniusGSMGetEmails 列表响应已包含完整正文，无需再次请求详情 */
func GeniusGSMGetEmails(email, token string) ([]NormEmail, error) {
	token = strings.TrimSpace(token)
	if token == "" {
		return nil, fmt.Errorf("geniusgsm: empty inbox id")
	}
	raw, err := geniusGSMRequest("GET", "/inboxes/"+url.PathEscape(token)+"/emails/", nil)
	if err != nil {
		return nil, err
	}
	rows := geniusGSMRows(raw, "results")
	out := make([]NormEmail, 0, len(rows))
	for _, item := range rows {
		message, _ := item.(map[string]any)
		out = append(out, NormalizeMap(geniusGSMFlatten(message, email), email))
	}
	return out, nil
}
