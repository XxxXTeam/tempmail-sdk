package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"net/url"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

const devmailUKBase = "https://www.devmail.uk/api"

type devmailUKGenerateResponse struct {
	Mailbox string `json:"mailbox"`
	Email   string `json:"email"`
}

type devmailUKInboxResponse struct {
	Mailbox string           `json:"mailbox"`
	Detail  bool             `json:"detail"`
	Count   int              `json:"count"`
	Emails  []map[string]any `json:"emails"`
}

func devmailUKGetJSON(u string) ([]byte, error) {
	req, err := http.NewRequest("GET", u, nil)
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
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("devmail-uk: http %d", resp.StatusCode)
	}
	return body, nil
}

func devmailUKMailbox(email string) string {
	email = strings.TrimSpace(email)
	if email == "" {
		return ""
	}
	if local, _, ok := strings.Cut(email, "@"); ok && strings.TrimSpace(local) != "" {
		return strings.TrimSpace(local)
	}
	return email
}

func DevmailUkGenerate() (*CreatedMailbox, error) {
	body, err := devmailUKGetJSON(devmailUKBase + "/new")
	if err != nil {
		return nil, err
	}
	var data devmailUKGenerateResponse
	if err := json.Unmarshal(body, &data); err != nil {
		return nil, err
	}

	email := strings.TrimSpace(data.Email)
	if email == "" && strings.TrimSpace(data.Mailbox) != "" {
		email = strings.TrimSpace(data.Mailbox) + "@devmail.uk"
	}
	if email == "" || !strings.Contains(email, "@") {
		return nil, fmt.Errorf("devmail-uk: invalid new email response")
	}
	return &CreatedMailbox{Channel: "devmail-uk", Email: email}, nil
}

func DevmailUkGetEmails(email string) ([]NormEmail, error) {
	mailbox := devmailUKMailbox(email)
	if mailbox == "" {
		return nil, fmt.Errorf("devmail-uk: empty email")
	}

	u := fmt.Sprintf("%s/inbox/%s?detail=true", devmailUKBase, url.QueryEscape(mailbox))
	body, err := devmailUKGetJSON(u)
	if err != nil {
		return nil, err
	}

	var data devmailUKInboxResponse
	if err := json.Unmarshal(body, &data); err != nil {
		return nil, err
	}

	rows := data.Emails
	if rows == nil {
		rows = []map[string]any{}
	}
	out := make([]NormEmail, 0, len(rows))
	for _, item := range rows {
		out = append(out, NormalizeMap(item, email))
	}
	return out, nil
}
