package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"net/url"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

const tempfastmailBase = "https://tempfastmail.com"

type tempfastmailBox struct {
	Email string `json:"email"`
	UUID  string `json:"uuid"`
}

func TempfastmailGenerate() (*CreatedMailbox, error) {
	req, err := http.NewRequest(http.MethodPost, tempfastmailBase+"/api/email-box", nil)
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
		return nil, fmt.Errorf("tempfastmail create: http %d", resp.StatusCode)
	}
	var data tempfastmailBox
	if err := json.Unmarshal(body, &data); err != nil {
		return nil, err
	}
	if !strings.Contains(data.Email, "@") || data.UUID == "" {
		return nil, fmt.Errorf("tempfastmail: invalid create response")
	}
	return &CreatedMailbox{
		Channel: "tempfastmail",
		Email:   strings.TrimSpace(data.Email),
		Token:   strings.TrimSpace(data.UUID),
	}, nil
}

func tempfastmailFlatten(raw map[string]any, recipient string) map[string]any {
	to := raw["real_to"]
	if s, ok := to.(string); !ok || strings.TrimSpace(s) == "" {
		to = recipient
	}
	return map[string]any{
		"id":          raw["uuid"],
		"from":        raw["from"],
		"to":          to,
		"subject":     raw["subject"],
		"html":        raw["html"],
		"date":        raw["received_at"],
		"attachments": raw["attachments"],
	}
}

func tempfastmailJSON(path string, out any) error {
	req, err := http.NewRequest(http.MethodGet, tempfastmailBase+path, nil)
	if err != nil {
		return err
	}
	req.Header.Set("Accept", "application/json")
	req.Header.Set("User-Agent", "Mozilla/5.0")
	resp, err := HTTPClient().Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return err
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return fmt.Errorf("tempfastmail: http %d", resp.StatusCode)
	}
	return json.Unmarshal(body, out)
}

func TempfastmailGetEmails(token string, email string) ([]NormEmail, error) {
	uuid := strings.TrimSpace(token)
	address := strings.TrimSpace(email)
	if uuid == "" {
		return nil, fmt.Errorf("tempfastmail: empty token")
	}
	if address == "" {
		return nil, fmt.Errorf("tempfastmail: empty email")
	}
	var rows []map[string]any
	if err := tempfastmailJSON(fmt.Sprintf("/api/email-box/%s/emails", url.PathEscape(uuid)), &rows); err != nil {
		return nil, err
	}
	out := make([]NormEmail, 0, len(rows))
	for _, row := range rows {
		raw := row
		if messageID, _ := row["uuid"].(string); strings.TrimSpace(messageID) != "" {
			var detail map[string]any
			if err := tempfastmailJSON(fmt.Sprintf("/api/email-box/%s/email/%s", url.PathEscape(uuid), url.PathEscape(messageID)), &detail); err == nil {
				raw = detail
			}
		}
		out = append(out, NormalizeMap(tempfastmailFlatten(raw, address), address))
	}
	return out, nil
}
