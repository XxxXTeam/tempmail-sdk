package provider

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

const mffacAPIBase = "https://www.mffac.com/api"

func mffacDefaultHeaders(req *http.Request) {
	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0")
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Accept", "*/*")
	req.Header.Set("Origin", "https://www.mffac.com")
	req.Header.Set("Referer", "https://www.mffac.com/")
	req.Header.Set("sec-ch-ua", `"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"`)
	req.Header.Set("sec-ch-ua-mobile", "?0")
	req.Header.Set("sec-ch-ua-platform", `"Windows"`)
	req.Header.Set("sec-fetch-dest", "empty")
	req.Header.Set("sec-fetch-mode", "cors")
	req.Header.Set("sec-fetch-site", "same-origin")
}

func MffacGenerate() (*CreatedMailbox, error) {
	req, err := http.NewRequest("POST", mffacAPIBase+"/mailboxes", bytes.NewReader([]byte(`{"expiresInHours":24}`)))
	if err != nil {
		return nil, err
	}
	mffacDefaultHeaders(req)

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, fmt.Errorf("mffac generate: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("mffac generate: %d", resp.StatusCode)
	}

	var parsed struct {
		Success bool `json:"success"`
		Mailbox *struct {
			Address   string `json:"address"`
			ID        string `json:"id"`
			ExpiresAt any    `json:"expiresAt"`
			CreatedAt string `json:"createdAt"`
		} `json:"mailbox"`
	}
	if err := json.Unmarshal(body, &parsed); err != nil {
		return nil, err
	}
	if !parsed.Success || parsed.Mailbox == nil || parsed.Mailbox.Address == "" || parsed.Mailbox.ID == "" {
		return nil, fmt.Errorf("mffac: invalid response")
	}

	email := parsed.Mailbox.Address + "@mffac.com"
	info := &CreatedMailbox{Channel: "mffac", Email: email, Token: parsed.Mailbox.ID}
	info.ExpiresAt = parsed.Mailbox.ExpiresAt
	info.CreatedAt = parsed.Mailbox.CreatedAt
	return info, nil
}

func MffacGetEmails(email string, _token string) ([]NormEmail, error) {
	local := email
	if i := strings.LastIndex(email, "@"); i > 0 {
		local = email[:i]
	}
	u := fmt.Sprintf("%s/mailboxes/%s/emails", mffacAPIBase, local)
	req, err := http.NewRequest("GET", u, nil)
	if err != nil {
		return nil, err
	}
	mffacDefaultHeaders(req)
	req.Header.Del("Content-Type")

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, fmt.Errorf("mffac emails: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("mffac emails: %d", resp.StatusCode)
	}

	var parsed struct {
		Success bool            `json:"success"`
		Emails  json.RawMessage `json:"emails"`
	}
	if err := json.Unmarshal(body, &parsed); err != nil {
		return nil, err
	}
	if !parsed.Success {
		return nil, fmt.Errorf("mffac: success false")
	}

	var rawList []json.RawMessage
	if len(parsed.Emails) > 0 && string(parsed.Emails) != "null" {
		if err := json.Unmarshal(parsed.Emails, &rawList); err != nil {
			return nil, err
		}
	}
	return NormalizeRawMessages(rawList, email)
}
