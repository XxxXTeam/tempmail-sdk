package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"net/url"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

const mailboxTempBase = "https://mailboxtemp.com"

func mailboxTempGetJSON(path string) (map[string]any, error) {
	req, err := http.NewRequest("GET", mailboxTempBase+path, nil)
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
	var raw map[string]any
	if err := json.Unmarshal(body, &raw); err != nil {
		return nil, err
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 || raw["ok"] == false {
		return nil, fmt.Errorf("mailboxtemp: HTTP %d", resp.StatusCode)
	}
	return raw, nil
}

/* MailboxTempGenerate 创建免费邮箱，地址即读取凭据 */
func MailboxTempGenerate() (*CreatedMailbox, error) {
	req, err := http.NewRequest("POST", mailboxTempBase+"/api/inbox/generate", nil)
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
	var out struct {
		OK      bool   `json:"ok"`
		Address string `json:"address"`
		Expires string `json:"expires"`
	}
	if err := json.Unmarshal(body, &out); err != nil {
		return nil, err
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 || !out.OK || out.Address == "" {
		return nil, fmt.Errorf("mailboxtemp create: %d", resp.StatusCode)
	}
	return &CreatedMailbox{Channel: "mailboxtemp", Email: out.Address, ExpiresAt: out.Expires}, nil
}

func mailboxTempFlatten(raw map[string]any, recipient string) map[string]any {
	return map[string]any{
		"id":      raw["id"],
		"from":    raw["from_address"],
		"to":      recipient,
		"subject": raw["subject"],
		"text":    raw["body_text"],
		"html":    raw["body_html"],
		"date":    raw["received_at"],
		"read":    raw["read"],
	}
}

/* MailboxTempGetEmails 先取列表，再逐条读取正文与附件 */
func MailboxTempGetEmails(email string) ([]NormEmail, error) {
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("mailboxtemp: empty email")
	}
	address := url.PathEscape(email)
	list, err := mailboxTempGetJSON("/api/inbox/" + address + "/emails")
	if err != nil {
		return nil, err
	}
	items, _ := list["emails"].([]any)
	out := make([]NormEmail, 0, len(items))
	for _, item := range items {
		summary, _ := item.(map[string]any)
		id := strings.TrimSpace(fmt.Sprint(summary["id"]))
		if id == "" {
			continue
		}
		detail, detailErr := mailboxTempGetJSON("/api/email/" + address + "/" + url.PathEscape(id))
		if detailErr == nil {
			if message, ok := detail["email"].(map[string]any); ok {
				flat := mailboxTempFlatten(message, email)
				if attachments, ok := detail["attachments"]; ok {
					flat["attachments"] = attachments
				}
				out = append(out, NormalizeMap(flat, email))
				continue
			}
		}
		out = append(out, NormalizeMap(map[string]any{
			"id":      id,
			"from":    summary["from_address"],
			"to":      email,
			"subject": summary["subject"],
			"date":    summary["received_at"],
			"read":    summary["read"],
		}, email))
	}
	return out, nil
}
