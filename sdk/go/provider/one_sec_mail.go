package provider

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

const oneSecMailBase = "https://1sec-mail.com/"

type oneSecMailSession struct {
	CSRF   string `json:"csrf"`
	Cookie string `json:"cookie"`
}

type oneSecMailMessagesResponse struct {
	Status     bool             `json:"status"`
	Mailbox    string           `json:"mailbox"`
	EmailToken string           `json:"email_token"`
	Messages   []map[string]any `json:"messages"`
}

func oneSecMailCookieLines(headers http.Header) []string {
	return headers.Values("Set-Cookie")
}

func oneSecMailMergeCookie(prev string, headers http.Header) string {
	jar := map[string]string{}
	for _, part := range strings.Split(prev, ";") {
		p := strings.TrimSpace(part)
		if i := strings.Index(p, "="); i > 0 {
			jar[p[:i]] = p[i+1:]
		}
	}
	for _, line := range oneSecMailCookieLines(headers) {
		first := strings.TrimSpace(strings.Split(line, ";")[0])
		if i := strings.Index(first, "="); i > 0 {
			jar[first[:i]] = first[i+1:]
		}
	}
	parts := make([]string, 0, len(jar))
	for k, v := range jar {
		parts = append(parts, k+"="+v)
	}
	return strings.Join(parts, "; ")
}

func oneSecMailCSRF(html string) string {
	const needle = `<meta name="csrf-token" content="`
	i := strings.Index(html, needle)
	if i < 0 {
		return ""
	}
	start := i + len(needle)
	end := strings.Index(html[start:], `"`)
	if end < 0 {
		return ""
	}
	return html[start : start+end]
}

func oneSecMailEncodeSession(session oneSecMailSession) string {
	b, _ := json.Marshal(session)
	return string(b)
}

func oneSecMailDecodeSession(token string) (oneSecMailSession, error) {
	var session oneSecMailSession
	if err := json.Unmarshal([]byte(token), &session); err != nil {
		return session, err
	}
	session.CSRF = strings.TrimSpace(session.CSRF)
	session.Cookie = strings.TrimSpace(session.Cookie)
	if session.CSRF == "" || session.Cookie == "" {
		return session, fmt.Errorf("1sec-mail: invalid session token")
	}
	return session, nil
}

func oneSecMailOpenSession() (oneSecMailSession, error) {
	req, err := http.NewRequest(http.MethodGet, oneSecMailBase, nil)
	if err != nil {
		return oneSecMailSession{}, err
	}
	req.Header.Set("Accept", "text/html,application/xhtml+xml")
	req.Header.Set("User-Agent", "Mozilla/5.0")
	resp, err := HTTPClient().Do(req)
	if err != nil {
		return oneSecMailSession{}, err
	}
	defer resp.Body.Close()
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return oneSecMailSession{}, err
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return oneSecMailSession{}, fmt.Errorf("1sec-mail home: %d", resp.StatusCode)
	}
	csrf := oneSecMailCSRF(string(body))
	if csrf == "" {
		return oneSecMailSession{}, fmt.Errorf("1sec-mail: csrf token not found")
	}
	return oneSecMailSession{
		CSRF:   csrf,
		Cookie: oneSecMailMergeCookie("", resp.Header),
	}, nil
}

func oneSecMailFetchMessages(session oneSecMailSession) (oneSecMailMessagesResponse, string, error) {
	payload, _ := json.Marshal(map[string]string{
		"_token":  session.CSRF,
		"captcha": "",
	})
	req, err := http.NewRequest(http.MethodPost, oneSecMailBase+"get_messages", bytes.NewReader(payload))
	if err != nil {
		return oneSecMailMessagesResponse{}, "", err
	}
	req.Header.Set("Accept", "application/json, text/plain, */*")
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-Requested-With", "XMLHttpRequest")
	req.Header.Set("X-CSRF-TOKEN", session.CSRF)
	req.Header.Set("Referer", oneSecMailBase)
	req.Header.Set("Cookie", session.Cookie)
	req.Header.Set("User-Agent", "Mozilla/5.0")
	resp, err := HTTPClient().Do(req)
	if err != nil {
		return oneSecMailMessagesResponse{}, "", err
	}
	defer resp.Body.Close()
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return oneSecMailMessagesResponse{}, "", err
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return oneSecMailMessagesResponse{}, "", fmt.Errorf("1sec-mail messages: %d", resp.StatusCode)
	}
	var data oneSecMailMessagesResponse
	if err := json.Unmarshal(body, &data); err != nil {
		return oneSecMailMessagesResponse{}, "", err
	}
	return data, oneSecMailMergeCookie(session.Cookie, resp.Header), nil
}

func oneSecMailFlatten(raw map[string]any, recipient string) map[string]any {
	out := make(map[string]any, len(raw)+8)
	for k, v := range raw {
		out[k] = v
	}
	content, _ := raw["content"].(string)
	html := ""
	if b, ok := raw["html"].(bool); ok && b {
		html = content
	} else if s, ok := raw["html"].(string); ok {
		html = s
	}
	out["id"] = raw["id"]
	if raw["from_email"] != nil {
		out["from"] = raw["from_email"]
	}
	if raw["to"] != nil {
		out["to"] = raw["to"]
	} else {
		out["to"] = recipient
	}
	out["subject"] = raw["subject"]
	if html != "" && raw["html"] == true {
		out["text"] = ""
	} else {
		out["text"] = content
	}
	out["html"] = html
	out["date"] = raw["receivedAt"]
	out["isRead"] = raw["is_seen"]
	out["attachments"] = raw["attachments"]
	return out
}

func OneSecMailGenerate() (*CreatedMailbox, error) {
	session, err := oneSecMailOpenSession()
	if err != nil {
		return nil, err
	}
	data, cookie, err := oneSecMailFetchMessages(session)
	if err != nil {
		return nil, err
	}
	email := strings.TrimSpace(data.Mailbox)
	if email == "" || !strings.Contains(email, "@") {
		return nil, fmt.Errorf("1sec-mail: invalid mailbox response")
	}
	session.Cookie = cookie
	return &CreatedMailbox{
		Channel: "1sec-mail",
		Email:   email,
		Token:   oneSecMailEncodeSession(session),
	}, nil
}

func OneSecMailGetEmails(token, email string) ([]NormEmail, error) {
	session, err := oneSecMailDecodeSession(token)
	if err != nil {
		return nil, err
	}
	address := strings.TrimSpace(email)
	if address == "" {
		return nil, fmt.Errorf("1sec-mail: empty email")
	}
	data, _, err := oneSecMailFetchMessages(session)
	if err != nil {
		return nil, err
	}
	out := make([]NormEmail, 0, len(data.Messages))
	for _, row := range data.Messages {
		out = append(out, NormalizeMap(oneSecMailFlatten(row, address), address))
	}
	return out, nil
}
