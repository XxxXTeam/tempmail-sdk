package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"net/url"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

const shittyEmailBase = "https://shitty.email/api"

type shittyEmailInboxResponse struct {
	Email     string                   `json:"email"`
	Token     string                   `json:"token"`
	ExpiresAt string                   `json:"expiresAt"`
	Emails    []map[string]interface{} `json:"emails"`
}

func shittyEmailJSON(method, u, token string, out any) error {
	req, err := http.NewRequest(method, u, nil)
	if err != nil {
		return err
	}
	req.Header.Set("Accept", "application/json")
	req.Header.Set("User-Agent", "Mozilla/5.0")
	if token != "" {
		req.Header.Set("X-Session-Token", token)
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
		return fmt.Errorf("shitty-email: http %d", resp.StatusCode)
	}
	if out == nil {
		return nil
	}
	return json.Unmarshal(raw, out)
}

func shittyEmailRaw(raw map[string]interface{}, recipient string) map[string]interface{} {
	to := raw["to"]
	if to == nil || fmt.Sprint(to) == "" {
		to = recipient
	}
	return map[string]interface{}{
		"id":      raw["id"],
		"from":    raw["from"],
		"to":      to,
		"subject": raw["subject"],
		"text":    raw["text"],
		"html":    raw["html"],
		"date":    raw["date"],
	}
}

func ShittyEmailGenerate() (*CreatedMailbox, error) {
	var data shittyEmailInboxResponse
	if err := shittyEmailJSON(http.MethodPost, shittyEmailBase+"/inbox", "", &data); err != nil {
		return nil, err
	}
	email := strings.TrimSpace(data.Email)
	token := strings.TrimSpace(data.Token)
	if email == "" || !strings.Contains(email, "@") || token == "" {
		return nil, fmt.Errorf("shitty-email: invalid inbox response")
	}
	return &CreatedMailbox{
		Channel:   "shitty-email",
		Email:     email,
		Token:     token,
		ExpiresAt: data.ExpiresAt,
	}, nil
}

func shittyEmailFetchDetail(token, id string) (map[string]interface{}, error) {
	u := fmt.Sprintf("%s/email/%s", shittyEmailBase, url.PathEscape(id))
	var detail map[string]interface{}
	if err := shittyEmailJSON(http.MethodGet, u, token, &detail); err != nil {
		return nil, err
	}
	return detail, nil
}

func ShittyEmailGetEmails(token, email string) ([]NormEmail, error) {
	sessionToken := strings.TrimSpace(token)
	address := strings.TrimSpace(email)
	if sessionToken == "" {
		return nil, fmt.Errorf("shitty-email: empty token")
	}
	if address == "" {
		return nil, fmt.Errorf("shitty-email: empty email")
	}

	var data shittyEmailInboxResponse
	if err := shittyEmailJSON(http.MethodGet, shittyEmailBase+"/inbox", sessionToken, &data); err != nil {
		return nil, err
	}
	out := make([]NormEmail, 0, len(data.Emails))
	for _, row := range data.Emails {
		raw := row
		id := strings.TrimSpace(fmt.Sprint(row["id"]))
		if id != "" {
			if detail, err := shittyEmailFetchDetail(sessionToken, id); err == nil {
				raw = detail
			}
		}
		out = append(out, NormalizeMap(shittyEmailRaw(raw, address), address))
	}
	return out, nil
}
