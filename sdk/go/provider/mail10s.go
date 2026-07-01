package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"math/rand"
	"net/url"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

const mail10sBase = "https://mail10s.com"

func mail10sRandomLocal() string {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	b := make([]byte, 19)
	copy(b, "sdk")
	for i := 3; i < len(b); i++ {
		b[i] = chars[rand.Intn(len(chars))]
	}
	return string(b)
}

func Mail10sGenerate() (*CreatedMailbox, error) {
	email := mail10sRandomLocal() + "@mail10s.com"
	return &CreatedMailbox{
		Channel: "mail10s",
		Email:   email,
		Token:   email,
	}, nil
}

func mail10sFlatten(raw map[string]any, recipient string) map[string]any {
	return map[string]any{
		"id":          raw["id"],
		"from":        raw["sender"],
		"to":          recipient,
		"subject":     raw["subject"],
		"text":        raw["body_text"],
		"html":        raw["body_html"],
		"date":        raw["received_at"],
		"attachments": raw["attachments"],
	}
}

func Mail10sGetEmails(email string) ([]NormEmail, error) {
	address := strings.TrimSpace(email)
	if address == "" {
		return nil, fmt.Errorf("mail10s: empty email")
	}
	u := fmt.Sprintf("%s/api/emails/%s/inbox", mail10sBase, url.PathEscape(address))
	req, err := http.NewRequest(http.MethodGet, u, nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("Accept", "application/json")
	req.Header.Set("User-Agent", "Mozilla/5.0")
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
		return nil, fmt.Errorf("mail10s: http %d", resp.StatusCode)
	}
	var data struct {
		Data struct {
			Messages []map[string]any `json:"messages"`
		} `json:"data"`
	}
	if err := json.Unmarshal(body, &data); err != nil {
		return nil, err
	}
	out := make([]NormEmail, 0, len(data.Data.Messages))
	for _, row := range data.Data.Messages {
		out = append(out, NormalizeMap(mail10sFlatten(row, address), address))
	}
	return out, nil
}
