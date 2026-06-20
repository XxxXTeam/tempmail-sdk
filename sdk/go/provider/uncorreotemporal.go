package provider

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net"
	stdhttp "net/http"
	"net/url"
	"strings"
	"time"
)

const uncorreotemporalBase = "https://uncorreotemporal.com/api/v1"

type uncorreotemporalCreateResponse struct {
	Address      string `json:"address"`
	ExpiresAt    string `json:"expires_at"`
	SessionToken string `json:"session_token"`
}

var uncorreotemporalHTTPClient = &stdhttp.Client{
	Timeout: 15 * time.Second,
	Transport: &stdhttp.Transport{
		Proxy: stdhttp.ProxyFromEnvironment,
		DialContext: func(ctx context.Context, network, address string) (net.Conn, error) {
			dialer := &net.Dialer{Timeout: 15 * time.Second}
			return dialer.DialContext(ctx, "tcp4", address)
		},
	},
}

func uncorreotemporalJSON(method, u, token string, out any) error {
	var body io.Reader
	if method == stdhttp.MethodPost {
		body = strings.NewReader("")
	}
	req, err := stdhttp.NewRequest(method, u, body)
	if err != nil {
		return err
	}
	req.Header.Set("Accept", "application/json")
	req.Header.Set("Origin", "https://uncorreotemporal.com")
	req.Header.Set("Referer", "https://uncorreotemporal.com/")
	req.Header.Set("User-Agent", "Mozilla/5.0")
	if method == stdhttp.MethodPost {
		req.Header.Set("Content-Type", "application/json")
	}
	if token != "" {
		req.Header.Set("X-Session-Token", token)
	}
	resp, err := uncorreotemporalHTTPClient.Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	data, err := io.ReadAll(resp.Body)
	if err != nil {
		return err
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return fmt.Errorf("uncorreotemporal http %d", resp.StatusCode)
	}
	return json.Unmarshal(data, out)
}

func uncorreotemporalFlatten(raw map[string]any, recipient string) map[string]any {
	return map[string]any{
		"id":          raw["id"],
		"from":        raw["from_address"],
		"to":          uncorreotemporalFirstNonEmpty(raw["to_address"], recipient),
		"subject":     raw["subject"],
		"text":        raw["body_text"],
		"html":        raw["body_html"],
		"date":        raw["received_at"],
		"isRead":      raw["is_read"],
		"attachments": raw["attachments"],
	}
}

func uncorreotemporalFirstNonEmpty(value any, fallback string) any {
	if s, ok := value.(string); ok && strings.TrimSpace(s) != "" {
		return s
	}
	return fallback
}

func UncorreotemporalGenerate() (*CreatedMailbox, error) {
	var data uncorreotemporalCreateResponse
	if err := uncorreotemporalJSON(stdhttp.MethodPost, uncorreotemporalBase+"/mailboxes", "", &data); err != nil {
		return nil, err
	}
	email := strings.TrimSpace(data.Address)
	token := strings.TrimSpace(data.SessionToken)
	if email == "" || !strings.Contains(email, "@") || token == "" {
		return nil, fmt.Errorf("uncorreotemporal: invalid mailbox response")
	}
	return &CreatedMailbox{
		Channel:   "uncorreotemporal",
		Email:     email,
		Token:     token,
		ExpiresAt: data.ExpiresAt,
	}, nil
}

func uncorreotemporalFetchDetail(email, token, messageID string) (map[string]any, error) {
	var raw map[string]any
	u := fmt.Sprintf("%s/mailboxes/%s/messages/%s", uncorreotemporalBase, url.PathEscape(email), url.PathEscape(messageID))
	if err := uncorreotemporalJSON(stdhttp.MethodGet, u, token, &raw); err != nil {
		return nil, err
	}
	return raw, nil
}

func UncorreotemporalGetEmails(token, email string) ([]NormEmail, error) {
	sessionToken := strings.TrimSpace(token)
	address := strings.TrimSpace(email)
	if sessionToken == "" {
		return nil, fmt.Errorf("uncorreotemporal: empty session token")
	}
	if address == "" {
		return nil, fmt.Errorf("uncorreotemporal: empty email")
	}
	u := fmt.Sprintf("%s/mailboxes/%s/messages?limit=50", uncorreotemporalBase, url.PathEscape(address))
	var rows []map[string]any
	if err := uncorreotemporalJSON(stdhttp.MethodGet, u, sessionToken, &rows); err != nil {
		return nil, err
	}
	out := make([]NormEmail, 0, len(rows))
	for _, row := range rows {
		raw := row
		if id := strings.TrimSpace(fmt.Sprint(row["id"])); id != "" {
			if detail, err := uncorreotemporalFetchDetail(address, sessionToken, id); err == nil {
				raw = detail
			}
		}
		out = append(out, NormalizeMap(uncorreotemporalFlatten(raw, address), address))
	}
	return out, nil
}
