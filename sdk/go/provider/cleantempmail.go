package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"net/url"
	"os"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

const cleanTempMailBase = "https://cleantempmail.com/api"
const cleanTempMailDefaultAPIKey = "ct-test"

type cleanTempMailGenerateResponse struct {
	Success bool `json:"success"`
	Data    struct {
		Email   string `json:"email"`
		Mailbox string `json:"mailbox"`
	} `json:"data"`
}

type cleanTempMailInboxResponse struct {
	Success bool `json:"success"`
	Data    struct {
		Emails []map[string]any `json:"emails"`
	} `json:"data"`
}

func cleanTempMailAPIKey() string {
	if s := strings.TrimSpace(os.Getenv("CLEANTEMPMAIL_API_KEY")); s != "" {
		return s
	}
	return cleanTempMailDefaultAPIKey
}

func cleanTempMailHeaders(req *http.Request) {
	req.Header.Set("Accept", "application/json")
	req.Header.Set("User-Agent", "Mozilla/5.0")
	req.Header.Set("X-API-Key", cleanTempMailAPIKey())
}

func cleanTempMailGetJSON(u string) ([]byte, error) {
	req, err := http.NewRequest("GET", u, nil)
	if err != nil {
		return nil, err
	}
	cleanTempMailHeaders(req)
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
		return nil, fmt.Errorf("cleantempmail: http %d", resp.StatusCode)
	}
	return body, nil
}

func CleanTempMailGenerate() (*CreatedMailbox, error) {
	body, err := cleanTempMailGetJSON(cleanTempMailBase + "/generate-email")
	if err != nil {
		return nil, err
	}
	var data cleanTempMailGenerateResponse
	if err := json.Unmarshal(body, &data); err != nil {
		return nil, err
	}

	email := strings.TrimSpace(data.Data.Email)
	if email == "" {
		email = strings.TrimSpace(data.Data.Mailbox)
	}
	if email == "" || !strings.Contains(email, "@") {
		return nil, fmt.Errorf("cleantempmail: invalid generate-email response")
	}
	return &CreatedMailbox{Channel: "cleantempmail", Email: email}, nil
}

func CleanTempMailGetEmails(email string) ([]NormEmail, error) {
	address := strings.TrimSpace(email)
	if address == "" {
		return nil, fmt.Errorf("cleantempmail: empty email")
	}

	u := fmt.Sprintf("%s/emails?email=%s", cleanTempMailBase, url.QueryEscape(address))
	body, err := cleanTempMailGetJSON(u)
	if err != nil {
		return nil, err
	}

	var data cleanTempMailInboxResponse
	if err := json.Unmarshal(body, &data); err != nil {
		return nil, err
	}

	rows := data.Data.Emails
	if rows == nil {
		rows = []map[string]any{}
	}
	out := make([]NormEmail, 0, len(rows))
	for _, item := range rows {
		out = append(out, NormalizeMap(item, address))
	}
	return out, nil
}
