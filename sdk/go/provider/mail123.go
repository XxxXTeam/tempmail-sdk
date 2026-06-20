package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"net/url"
	"strings"
	"time"

	http "github.com/bogdanfinn/fhttp"
)

const mail123Base = "https://mail123.fr/api/v1"

type mail123MailboxResponse struct {
	Address      string `json:"address"`
	ExpiresInDay int64  `json:"expires_in_days"`
}

type mail123ListResponse struct {
	Messages []map[string]any `json:"messages"`
}

type mail123DetailResponse struct {
	Message map[string]any `json:"message"`
}

func mail123JSON(u string, out any) error {
	req, err := http.NewRequest(http.MethodGet, u, nil)
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
		return fmt.Errorf("mail123: http %d", resp.StatusCode)
	}
	return json.Unmarshal(body, out)
}

func mail123Flatten(raw map[string]any, recipient string) map[string]any {
	out := make(map[string]any, len(raw)+4)
	for k, v := range raw {
		out[k] = v
	}
	out["id"] = raw["id"]
	if v, ok := raw["to"]; ok && strings.TrimSpace(fmt.Sprintf("%v", v)) != "" {
		out["to"] = v
	} else {
		out["to"] = recipient
	}
	if _, ok := out["text"]; !ok {
		out["text"] = raw["preview"]
	}
	if v, ok := raw["is_unread"].(bool); ok {
		out["isRead"] = !v
	}
	return out
}

func Mail123Generate() (*CreatedMailbox, error) {
	var data mail123MailboxResponse
	if err := mail123JSON(mail123Base+"/mailbox/new", &data); err != nil {
		return nil, err
	}
	email := strings.TrimSpace(data.Address)
	if email == "" || !strings.Contains(email, "@") {
		return nil, fmt.Errorf("mail123: invalid mailbox response")
	}
	var expiresAt any
	if data.ExpiresInDay > 0 {
		expiresAt = time.Now().Add(time.Duration(data.ExpiresInDay) * 24 * time.Hour).UnixMilli()
	}
	return &CreatedMailbox{
		Channel:   "mail123",
		Email:     email,
		Token:     email,
		ExpiresAt: expiresAt,
	}, nil
}

func mail123FetchDetail(address, messageID string) (map[string]any, error) {
	u := fmt.Sprintf("%s/mailbox/%s/messages/%s", mail123Base, url.PathEscape(address), url.PathEscape(messageID))
	var data mail123DetailResponse
	if err := mail123JSON(u, &data); err != nil {
		return nil, err
	}
	if data.Message == nil {
		return nil, fmt.Errorf("mail123: empty message detail")
	}
	return data.Message, nil
}

func Mail123GetEmails(email string) ([]NormEmail, error) {
	address := strings.TrimSpace(email)
	if address == "" {
		return nil, fmt.Errorf("mail123: empty email")
	}
	u := fmt.Sprintf("%s/mailbox/%s/messages?limit=50", mail123Base, url.PathEscape(address))
	var data mail123ListResponse
	if err := mail123JSON(u, &data); err != nil {
		return nil, err
	}
	out := make([]NormEmail, 0, len(data.Messages))
	for _, row := range data.Messages {
		raw := row
		if id := strings.TrimSpace(fmt.Sprintf("%v", row["id"])); id != "" {
			if detail, err := mail123FetchDetail(address, id); err == nil {
				raw = detail
			}
		}
		out = append(out, NormalizeMap(mail123Flatten(raw, address), address))
	}
	return out, nil
}
