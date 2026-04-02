package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

const temporaryEmailOrgMessagesURL = "https://www.temporary-email.org/zh/messages"
const temporaryEmailOrgReferer = "https://www.temporary-email.org/zh"

var temporaryEmailOrgHeaders = map[string]string{
	"Accept":             "text/plain, */*; q=0.01",
	"accept-language":    "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
	"cache-control":      "no-cache",
	"dnt":                "1",
	"pragma":             "no-cache",
	"priority":           "u=1, i",
	"referer":            temporaryEmailOrgReferer,
	"sec-ch-ua":          `"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"`,
	"sec-ch-ua-mobile":   "?0",
	"sec-ch-ua-platform": `"Windows"`,
	"sec-fetch-dest":     "empty",
	"sec-fetch-mode":     "cors",
	"sec-fetch-site":     "same-origin",
}

func setTemporaryEmailOrgHeaders(req *http.Request) {
	for k, v := range temporaryEmailOrgHeaders {
		req.Header.Set(k, v)
	}
	req.Header.Set("User-Agent", GetCurrentUA())
}

func joinCookieHeader(cookies []*http.Cookie) string {
	var b strings.Builder
	for i, c := range cookies {
		if i > 0 {
			b.WriteString("; ")
		}
		b.WriteString(c.Name)
		b.WriteByte('=')
		b.WriteString(c.Value)
	}
	return b.String()
}

type temporaryEmailOrgMessagesResp struct {
	Mailbox  string            `json:"mailbox"`
	Messages []json.RawMessage `json:"messages"`
}

func TemporaryEmailOrgGenerate() (*CreatedMailbox, error) {
	req, err := http.NewRequest("GET", temporaryEmailOrgMessagesURL, nil)
	if err != nil {
		return nil, err
	}
	setTemporaryEmailOrgHeaders(req)

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	if err := CheckHTTPStatus(resp, "temporary-email-org generate"); err != nil {
		return nil, err
	}

	cookieStr := joinCookieHeader(resp.Cookies())
	if cookieStr == "" || !strings.Contains(cookieStr, "temporaryemail_session=") || !strings.Contains(cookieStr, "email=") {
		return nil, fmt.Errorf("temporary-email-org: missing session cookies")
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	var parsed temporaryEmailOrgMessagesResp
	if err := json.Unmarshal(body, &parsed); err != nil {
		return nil, err
	}
	mailbox := strings.TrimSpace(parsed.Mailbox)
	if mailbox == "" || !strings.Contains(mailbox, "@") {
		return nil, fmt.Errorf("temporary-email-org: invalid mailbox")
	}

	return &CreatedMailbox{Channel: "temporary-email-org", Email: mailbox, Token: cookieStr}, nil
}

func TemporaryEmailOrgGetEmails(cookieHeader string, email string) ([]NormEmail, error) {
	req, err := http.NewRequest("GET", temporaryEmailOrgMessagesURL, nil)
	if err != nil {
		return nil, err
	}
	setTemporaryEmailOrgHeaders(req)
	req.Header.Set("Cookie", cookieHeader)
	req.Header.Set("X-Requested-With", "XMLHttpRequest")

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	if err := CheckHTTPStatus(resp, "temporary-email-org messages"); err != nil {
		return nil, err
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	var parsed temporaryEmailOrgMessagesResp
	if err := json.Unmarshal(body, &parsed); err != nil {
		return nil, err
	}

	return NormalizeRawMessages(parsed.Messages, email)
}
