package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"net/url"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

const openinboxBase = "https://api.openinbox.io/api"

type openinboxCreateResponse struct {
	ID        string `json:"id"`
	Email     string `json:"email"`
	ExpiresAt string `json:"expiresAt"`
	CreatedAt string `json:"createdAt"`
}

type openinboxListResponse struct {
	Emails []map[string]any `json:"emails"`
}

func openinboxJSON(method, u string, out any) error {
	req, err := http.NewRequest(method, u, nil)
	if err != nil {
		return err
	}
	req.Header.Set("Accept", "application/json, text/plain, */*")
	req.Header.Set("Origin", "https://openinbox.io")
	req.Header.Set("Referer", "https://openinbox.io/")
	req.Header.Set("User-Agent", "Mozilla/5.0")
	if method == http.MethodPost {
		req.Header.Set("Content-Type", "application/json")
	}
	resp, err := HTTPClient().Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	data, err := io.ReadAll(resp.Body)
	if err != nil {
		return err
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return fmt.Errorf("openinbox: http %d", resp.StatusCode)
	}
	return json.Unmarshal(data, out)
}

func openinboxFlatten(raw map[string]any, recipient string) map[string]any {
	return map[string]any{
		"id":          raw["id"],
		"from":        raw["from"],
		"to":          recipient,
		"subject":     raw["subject"],
		"text":        raw["textBody"],
		"html":        raw["htmlBody"],
		"receivedAt":  raw["receivedAt"],
		"isRead":      raw["isRead"],
		"attachments": []any{},
	}
}

func OpeninboxGenerate() (*CreatedMailbox, error) {
	var data openinboxCreateResponse
	if err := openinboxJSON(http.MethodPost, openinboxBase+"/inbox", &data); err != nil {
		return nil, err
	}
	id := strings.TrimSpace(data.ID)
	email := strings.TrimSpace(data.Email)
	if id == "" || email == "" || !strings.Contains(email, "@") {
		return nil, fmt.Errorf("openinbox: invalid mailbox response")
	}
	return &CreatedMailbox{
		Channel:   "openinbox",
		Email:     email,
		Token:     id,
		ExpiresAt: data.ExpiresAt,
		CreatedAt: data.CreatedAt,
	}, nil
}

func openinboxFetchDetail(messageID string) (map[string]any, error) {
	var data map[string]any
	u := openinboxBase + "/emails/" + url.PathEscape(messageID)
	if err := openinboxJSON(http.MethodGet, u, &data); err != nil {
		return nil, err
	}
	return data, nil
}

func OpeninboxGetEmails(inboxID, email string) ([]NormEmail, error) {
	box := strings.TrimSpace(inboxID)
	address := strings.TrimSpace(email)
	if box == "" {
		return nil, fmt.Errorf("openinbox: empty inbox id")
	}
	if address == "" {
		return nil, fmt.Errorf("openinbox: empty email")
	}
	u := fmt.Sprintf("%s/emails/inbox/%s?page=1&limit=50", openinboxBase, url.PathEscape(box))
	var data openinboxListResponse
	if err := openinboxJSON(http.MethodGet, u, &data); err != nil {
		return nil, err
	}
	out := make([]NormEmail, 0, len(data.Emails))
	for _, row := range data.Emails {
		raw := row
		if id := strings.TrimSpace(fmt.Sprintf("%v", row["id"])); id != "" {
			if detail, err := openinboxFetchDetail(id); err == nil {
				raw = detail
			}
		}
		out = append(out, NormalizeMap(openinboxFlatten(raw, address), address))
	}
	return out, nil
}
