package provider

import (
	"encoding/base64"
	"encoding/json"
	"fmt"
	"io"
	"net/url"
	"strings"
	"time"

	http "github.com/bogdanfinn/fhttp"
)

const webmailtempBase = "https://webmailtemp.com"

type webmailtempToken struct {
	Username string `json:"u"`
	Cookie   string `json:"c"`
}

type webmailtempCreateResponse struct {
	Success   bool    `json:"success"`
	Email     string  `json:"email"`
	Username  string  `json:"username"`
	CreatedAt float64 `json:"created_at"`
	TTL       int64   `json:"ttl"`
}

func webmailtempEncodeToken(data webmailtempToken) (string, error) {
	raw, err := json.Marshal(data)
	if err != nil {
		return "", err
	}
	return "wmt1:" + base64.StdEncoding.EncodeToString(raw), nil
}

func webmailtempDecodeToken(token string) (webmailtempToken, error) {
	if !strings.HasPrefix(token, "wmt1:") {
		return webmailtempToken{}, fmt.Errorf("webmailtemp: invalid token")
	}
	raw, err := base64.StdEncoding.DecodeString(strings.TrimPrefix(token, "wmt1:"))
	if err != nil {
		return webmailtempToken{}, err
	}
	var data webmailtempToken
	if err := json.Unmarshal(raw, &data); err != nil {
		return webmailtempToken{}, err
	}
	if data.Username == "" || data.Cookie == "" {
		return webmailtempToken{}, fmt.Errorf("webmailtemp: invalid token data")
	}
	return data, nil
}

func webmailtempSessionCookie(resp *http.Response) string {
	for _, value := range resp.Header.Values("Set-Cookie") {
		if p := strings.TrimSpace(strings.Split(value, ";")[0]); p != "" {
			return p
		}
	}
	return ""
}

func WebmailtempGenerate() (*CreatedMailbox, error) {
	req, err := http.NewRequest(http.MethodGet, webmailtempBase+"/api/create", nil)
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
	cookie := webmailtempSessionCookie(resp)
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("webmailtemp create: http %d", resp.StatusCode)
	}
	var data webmailtempCreateResponse
	if err := json.Unmarshal(body, &data); err != nil {
		return nil, err
	}
	if !data.Success || !strings.Contains(data.Email, "@") || data.Username == "" || cookie == "" {
		return nil, fmt.Errorf("webmailtemp: invalid create response")
	}
	token, err := webmailtempEncodeToken(webmailtempToken{Username: data.Username, Cookie: cookie})
	if err != nil {
		return nil, err
	}
	var expiresAt any
	if data.TTL > 0 {
		expiresAt = time.Now().Add(time.Duration(data.TTL) * time.Second).UnixMilli()
	}
	return &CreatedMailbox{
		Channel:   "webmailtemp",
		Email:     strings.TrimSpace(data.Email),
		Token:     token,
		ExpiresAt: expiresAt,
	}, nil
}

func webmailtempFlatten(raw map[string]any, recipient string) map[string]any {
	return map[string]any{
		"id":          raw["id"],
		"from":        raw["from"],
		"to":          recipient,
		"subject":     raw["subject"],
		"text":        raw["body"],
		"html":        raw["html"],
		"date":        raw["received_at"],
		"attachments": raw["attachments"],
	}
}

func WebmailtempGetEmails(token string, email string) ([]NormEmail, error) {
	address := strings.TrimSpace(email)
	if address == "" {
		return nil, fmt.Errorf("webmailtemp: empty email")
	}
	session, err := webmailtempDecodeToken(token)
	if err != nil {
		return nil, err
	}
	req, err := http.NewRequest(http.MethodGet, fmt.Sprintf("%s/api/check/%s", webmailtempBase, url.PathEscape(session.Username)), nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("Accept", "application/json")
	req.Header.Set("Cookie", session.Cookie)
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
		return nil, fmt.Errorf("webmailtemp check: http %d", resp.StatusCode)
	}
	var data struct {
		Emails []map[string]any `json:"emails"`
	}
	if err := json.Unmarshal(body, &data); err != nil {
		return nil, err
	}
	out := make([]NormEmail, 0, len(data.Emails))
	for _, row := range data.Emails {
		out = append(out, NormalizeMap(webmailtempFlatten(row, address), address))
	}
	return out, nil
}
