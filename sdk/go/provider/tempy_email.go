package provider

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/url"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

const tempyEmailBase = "https://tempy.email/api/v1"

type tempyEmailCreateResponse struct {
	Email        string  `json:"email"`
	ExpiresAt    string  `json:"expires_at"`
	WebURL       string  `json:"web_url"`
	WebhookURL   *string `json:"webhook_url"`
	WebhookFormat *string `json:"webhook_format"`
}

type tempyEmailListResponse struct {
	Messages []map[string]any `json:"messages"`
}

func tempyEmailJSON(method, u string, body []byte, out any) error {
	var reader io.Reader
	if body != nil {
		reader = bytes.NewReader(body)
	}
	req, err := http.NewRequest(method, u, reader)
	if err != nil {
		return err
	}
	req.Header.Set("Accept", "application/json")
	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36")
	if body != nil {
		req.Header.Set("Content-Type", "application/json")
	}

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()

	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return err
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return fmt.Errorf("tempy-email: http %d", resp.StatusCode)
	}
	if out == nil {
		return nil
	}
	return json.Unmarshal(raw, out)
}

func tempyEmailFlatten(raw map[string]any, recipient string) map[string]any {
	id := raw["id"]
	if m2uAnyString(id) == "" {
		id = raw["messageId"]
	}
	if m2uAnyString(id) == "" {
		id = raw["message_id"]
	}
	if m2uAnyString(id) == "" {
		id = raw["mail_id"]
	}
	from := raw["from"]
	if m2uAnyString(from) == "" {
		from = raw["sender"]
	}
	if m2uAnyString(from) == "" {
		from = raw["from_addr"]
	}
	if m2uAnyString(from) == "" {
		from = raw["from_address"]
	}
	text := raw["text"]
	if m2uAnyString(text) == "" {
		text = raw["body_text"]
	}
	if m2uAnyString(text) == "" {
		text = raw["text_body"]
	}
	if m2uAnyString(text) == "" {
		text = raw["body"]
	}
	html := raw["html"]
	if m2uAnyString(html) == "" {
		html = raw["body_html"]
	}
	if m2uAnyString(html) == "" {
		html = raw["html_body"]
	}
	date := raw["date"]
	if m2uAnyString(date) == "" {
		date = raw["received_at"]
	}
	if m2uAnyString(date) == "" {
		date = raw["created_at"]
	}
	return map[string]any{
		"id":          id,
		"from":        from,
		"to":          recipient,
		"subject":     raw["subject"],
		"text":        text,
		"html":        html,
		"date":        date,
		"is_read":     raw["is_read"],
		"attachments": raw["attachments"],
	}
}

func TempyEmailGenerate(domain *string) (*CreatedMailbox, error) {
	body := map[string]any{}
	if domain != nil && strings.TrimSpace(*domain) != "" {
		body["domain"] = strings.TrimSpace(*domain)
	}
	reqBody, err := json.Marshal(body)
	if err != nil {
		return nil, err
	}
	var data tempyEmailCreateResponse
	if err := tempyEmailJSON(http.MethodPost, tempyEmailBase+"/mailbox", reqBody, &data); err != nil {
		return nil, err
	}
	email := strings.TrimSpace(data.Email)
	if email == "" {
		return nil, fmt.Errorf("tempy-email: invalid create response")
	}
	return &CreatedMailbox{
		Channel:   "tempy-email",
		Email:     email,
		ExpiresAt: data.ExpiresAt,
	}, nil
}

func TempyEmailGetEmails(email string) ([]NormEmail, error) {
	address := strings.TrimSpace(email)
	if address == "" {
		return nil, fmt.Errorf("tempy-email: empty email")
	}

	u := fmt.Sprintf("%s/mailbox/%s/messages", tempyEmailBase, url.PathEscape(address))
	var data tempyEmailListResponse
	if err := tempyEmailJSON(http.MethodGet, u, nil, &data); err != nil {
		return nil, err
	}
	out := make([]NormEmail, 0, len(data.Messages))
	for _, row := range data.Messages {
		out = append(out, NormalizeMap(tempyEmailFlatten(row, address), address))
	}
	return out, nil
}
