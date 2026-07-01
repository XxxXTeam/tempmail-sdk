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

const tempmailproBase = "https://tempmailpro.us/api/v1"

type tempmailproMailboxResponse struct {
	Data struct {
		Token     string `json:"token"`
		Address   string `json:"address"`
		ExpiresAt any    `json:"expires_at"`
		CreatedAt any    `json:"created_at"`
	} `json:"data"`
}

type tempmailproListResponse struct {
	Data []map[string]any `json:"data"`
}

type tempmailproDetailResponse struct {
	Data map[string]any `json:"data"`
}

func tempmailproJSON(method, u string, body []byte, out any) error {
	var reader io.Reader
	if body != nil {
		reader = bytes.NewReader(body)
	}
	req, err := http.NewRequest(method, u, reader)
	if err != nil {
		return err
	}
	req.Header.Set("Accept", "application/json")
	req.Header.Set("User-Agent", "Mozilla/5.0")
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
		return fmt.Errorf("tempmailpro: http %d", resp.StatusCode)
	}
	if out == nil {
		return nil
	}
	return json.Unmarshal(raw, out)
}

func tempmailproString(v any) string {
	if v == nil {
		return ""
	}
	return strings.TrimSpace(fmt.Sprint(v))
}

func tempmailproFlatten(raw map[string]any, recipient string) map[string]any {
	id := raw["id"]
	if id == nil || tempmailproString(id) == "" {
		id = raw["message_id"]
	}
	from := raw["from_address"]
	if from == nil || tempmailproString(from) == "" {
		from = raw["from_name"]
	}
	return map[string]any{
		"id":          id,
		"from":        from,
		"to":          recipient,
		"subject":     raw["subject"],
		"text":        raw["body_text"],
		"html":        raw["body_html"],
		"date":        raw["received_at"],
		"attachments": raw["attachments"],
	}
}

func TempmailproGenerate() (*CreatedMailbox, error) {
	var data tempmailproMailboxResponse
	if err := tempmailproJSON(http.MethodPost, tempmailproBase+"/mailbox/create", []byte("{}"), &data); err != nil {
		return nil, err
	}
	email := strings.TrimSpace(data.Data.Address)
	token := strings.TrimSpace(data.Data.Token)
	if email == "" || !strings.Contains(email, "@") || token == "" {
		return nil, fmt.Errorf("tempmailpro: invalid mailbox response")
	}
	return &CreatedMailbox{
		Channel:   "tempmailpro",
		Email:     email,
		Token:     token,
		ExpiresAt: data.Data.ExpiresAt,
		CreatedAt: tempmailproString(data.Data.CreatedAt),
	}, nil
}

func tempmailproFetchDetail(token, messageID string) (map[string]any, error) {
	u := fmt.Sprintf("%s/mailbox/%s/emails/%s", tempmailproBase, url.PathEscape(token), url.PathEscape(messageID))
	var data tempmailproDetailResponse
	if err := tempmailproJSON(http.MethodGet, u, nil, &data); err != nil {
		return nil, err
	}
	if data.Data == nil {
		return nil, fmt.Errorf("tempmailpro: empty message detail")
	}
	return data.Data, nil
}

func TempmailproGetEmails(token, email string) ([]NormEmail, error) {
	mailboxToken := strings.TrimSpace(token)
	address := strings.TrimSpace(email)
	if mailboxToken == "" {
		return nil, fmt.Errorf("tempmailpro: empty token")
	}
	if address == "" {
		return nil, fmt.Errorf("tempmailpro: empty email")
	}

	u := fmt.Sprintf("%s/mailbox/%s/emails", tempmailproBase, url.PathEscape(mailboxToken))
	var data tempmailproListResponse
	if err := tempmailproJSON(http.MethodGet, u, nil, &data); err != nil {
		return nil, err
	}
	out := make([]NormEmail, 0, len(data.Data))
	for _, row := range data.Data {
		raw := row
		if id := tempmailproString(row["id"]); id != "" {
			if detail, err := tempmailproFetchDetail(mailboxToken, id); err == nil {
				raw = detail
			}
		}
		out = append(out, NormalizeMap(tempmailproFlatten(raw, address), address))
	}
	return out, nil
}
