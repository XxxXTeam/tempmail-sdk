package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"net/url"
	"regexp"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

const xkxMeBase = "https://xkx.me"

var xkxMeCsrfRe = regexp.MustCompile(`csrf-token" content="([^"]+)"`)

type xkxMeSession struct {
	csrf    string
	cookies string
}

func xkxMeGetSession() (*xkxMeSession, error) {
	client := HTTPClient()
	req, err := http.NewRequest(http.MethodGet, xkxMeBase, nil)
	if err != nil {
		return nil, err
	}
	ua := getCurrentUA()
	if ua != "" {
		req.Header.Set("User-Agent", ua)
	}
	req.Header.Set("Accept", "text/html")

	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	m := xkxMeCsrfRe.FindStringSubmatch(string(body))
	if len(m) < 2 {
		return nil, fmt.Errorf("xkx-me: 无法获取 CSRF token")
	}

	var cookieParts []string
	for _, c := range resp.Cookies() {
		cookieParts = append(cookieParts, c.Name+"="+c.Value)
	}

	return &xkxMeSession{
		csrf:    m[1],
		cookies: strings.Join(cookieParts, "; "),
	}, nil
}

var xkxMeEmailRe = regexp.MustCompile(`mailbox/([^\s/"'<>]+@xkx\.me)`)

func XkxMeGenerate() (*CreatedMailbox, error) {
	session, err := xkxMeGetSession()
	if err != nil {
		return nil, err
	}

	client := HTTPClient()
	formData := "_token=" + url.QueryEscape(session.csrf)
	req, err := http.NewRequest(http.MethodPost, xkxMeBase+"/mailbox/create/random", strings.NewReader(formData))
	if err != nil {
		return nil, err
	}

	ua := getCurrentUA()
	if ua != "" {
		req.Header.Set("User-Agent", ua)
	}
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	req.Header.Set("Cookie", session.cookies)

	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	location := resp.Header.Get("Location")
	if location == "" {
		b, _ := io.ReadAll(resp.Body)
		location = string(b)
	}

	em := xkxMeEmailRe.FindStringSubmatch(location)
	if len(em) < 2 {
		return nil, fmt.Errorf("xkx-me: 无法从响应中提取邮箱地址")
	}

	return &CreatedMailbox{
		Channel: "xkx-me",
		Email:   em[1],
		Token:   session.cookies,
	}, nil
}

func XkxMeGetEmails(token, email string) ([]NormEmail, error) {
	address := strings.TrimSpace(email)
	if address == "" {
		return nil, fmt.Errorf("xkx-me: 缺少邮箱地址")
	}

	client := HTTPClient()
	u := fmt.Sprintf("%s/mailbox/%s/messages", xkxMeBase, url.PathEscape(address))
	req, err := http.NewRequest(http.MethodGet, u, nil)
	if err != nil {
		return nil, err
	}

	ua := getCurrentUA()
	if ua != "" {
		req.Header.Set("User-Agent", ua)
	}
	req.Header.Set("Accept", "application/json")
	req.Header.Set("X-Requested-With", "XMLHttpRequest")
	if token != "" {
		req.Header.Set("Cookie", token)
	}

	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	if resp.StatusCode == 404 {
		return []NormEmail{}, nil
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("xkx-me: 获取邮件失败 HTTP %d", resp.StatusCode)
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	var messages []map[string]any
	if err := json.Unmarshal(body, &messages); err != nil {
		var wrapper struct {
			Messages []map[string]any `json:"messages"`
		}
		if err2 := json.Unmarshal(body, &wrapper); err2 != nil {
			return []NormEmail{}, nil
		}
		messages = wrapper.Messages
	}

	if len(messages) == 0 {
		return []NormEmail{}, nil
	}

	out := make([]NormEmail, 0, len(messages))
	for _, msg := range messages {
		htmlContent := xkxMeToString(msg["html"])
		if htmlContent == "" {
			htmlContent = xkxMeToString(msg["body"])
		}
		raw := map[string]any{
			"id":      msg["id"],
			"from":    msg["from"],
			"to":      address,
			"subject": msg["subject"],
			"date":    msg["date"],
			"html":    htmlContent,
			"text":    msg["text"],
		}
		out = append(out, NormalizeMap(raw, address))
	}
	return out, nil
}

func xkxMeToString(v any) string {
	if v == nil {
		return ""
	}
	if s, ok := v.(string); ok {
		return s
	}
	return fmt.Sprintf("%v", v)
}
