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

const m2uBase = "https://api.m2u.io"

type m2uMailboxResponse struct {
	Mailbox struct {
		Token     string `json:"token"`
		ViewToken string `json:"view_token"`
		LocalPart string `json:"local_part"`
		Domain    string `json:"domain"`
		ExpiresAt any    `json:"expires_at"`
		CreatedAt any    `json:"created_at"`
	} `json:"mailbox"`
}

type m2uListResponse struct {
	Messages []map[string]any `json:"messages"`
}

type m2uDetailResponse struct {
	Message map[string]any `json:"message"`
}

func m2uJSON(method, u string, body []byte, out any) error {
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
		return fmt.Errorf("m2u: http %d", resp.StatusCode)
	}
	if out == nil {
		return nil
	}
	return json.Unmarshal(raw, out)
}

func m2uAnyString(v any) string {
	if v == nil {
		return ""
	}
	switch x := v.(type) {
	case string:
		return strings.TrimSpace(x)
	default:
		return strings.TrimSpace(fmt.Sprint(v))
	}
}

func m2uPackToken(token, viewToken string) string {
	raw, _ := json.Marshal(map[string]string{
		"token":     token,
		"viewToken": viewToken,
	})
	return string(raw)
}

func m2uUnpackToken(packed string) (string, string) {
	value := strings.TrimSpace(packed)
	if value == "" {
		return "", ""
	}
	if strings.HasPrefix(value, "{") {
		var payload map[string]any
		if err := json.Unmarshal([]byte(value), &payload); err == nil {
			return m2uAnyString(payload["token"]), m2uAnyString(payload["viewToken"])
		}
		return "", ""
	}
	return value, ""
}

func m2uFlattenMessage(raw map[string]any, recipient string) map[string]any {
	id := raw["id"]
	if m2uAnyString(id) == "" {
		id = raw["message_id"]
	}
	from := raw["from_addr"]
	if m2uAnyString(from) == "" {
		from = raw["from_address"]
	}
	if m2uAnyString(from) == "" {
		from = raw["from"]
	}
	text := raw["text_body"]
	if m2uAnyString(text) == "" {
		text = raw["body_text"]
	}
	if m2uAnyString(text) == "" {
		text = raw["text"]
	}
	html := raw["html_body"]
	if m2uAnyString(html) == "" {
		html = raw["body_html"]
	}
	if m2uAnyString(html) == "" {
		html = raw["html"]
	}
	date := raw["received_at"]
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

func m2uFetchDetail(token, viewToken, messageID string) map[string]any {
	u := fmt.Sprintf("%s/v1/mailboxes/%s/messages/%s?view=%s", m2uBase, url.PathEscape(token), url.PathEscape(messageID), url.QueryEscape(viewToken))
	var data m2uDetailResponse
	if err := m2uJSON(http.MethodGet, u, nil, &data); err != nil {
		return nil
	}
	if data.Message == nil {
		return nil
	}
	return data.Message
}

func M2uGenerate() (*CreatedMailbox, error) {
	var data m2uMailboxResponse
	if err := m2uJSON(http.MethodPost, m2uBase+"/v1/mailboxes/auto", []byte("{}"), &data); err != nil {
		return nil, err
	}
	localPart := strings.TrimSpace(data.Mailbox.LocalPart)
	domain := strings.TrimSpace(data.Mailbox.Domain)
	token := strings.TrimSpace(data.Mailbox.Token)
	viewToken := strings.TrimSpace(data.Mailbox.ViewToken)
	email := ""
	if localPart != "" && domain != "" {
		email = localPart + "@" + domain
	}
	if email == "" || token == "" || viewToken == "" {
		return nil, fmt.Errorf("m2u: invalid mailbox response")
	}
	return &CreatedMailbox{
		Channel:   "m2u",
		Email:     email,
		Token:     m2uPackToken(token, viewToken),
		ExpiresAt: data.Mailbox.ExpiresAt,
		CreatedAt: m2uAnyString(data.Mailbox.CreatedAt),
	}, nil
}

func M2uGetEmails(token, email string) ([]NormEmail, error) {
	mailboxToken, viewToken := m2uUnpackToken(token)
	address := strings.TrimSpace(email)
	if mailboxToken == "" {
		return nil, fmt.Errorf("m2u: missing token")
	}
	if viewToken == "" {
		return nil, fmt.Errorf("m2u: missing view token")
	}
	if address == "" {
		return nil, fmt.Errorf("m2u: empty email")
	}

	u := fmt.Sprintf("%s/v1/mailboxes/%s/messages?view=%s", m2uBase, url.PathEscape(mailboxToken), url.QueryEscape(viewToken))
	var data m2uListResponse
	if err := m2uJSON(http.MethodGet, u, nil, &data); err != nil {
		return nil, err
	}
	out := make([]NormEmail, 0, len(data.Messages))
	for _, row := range data.Messages {
		raw := row
		if id := m2uAnyString(row["id"]); id != "" {
			if detail := m2uFetchDetail(mailboxToken, viewToken, id); detail != nil {
				raw = detail
			}
		}
		out = append(out, NormalizeMap(m2uFlattenMessage(raw, address), address))
	}
	return out, nil
}
