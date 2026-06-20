package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"math/rand"
	"net/url"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

const mailforspamBase = "https://api.mailforspam.com/api"
const mailforspamDefaultDomain = "mailforspam.com"

var mailforspamDomains = []string{"mailforspam.com", "tempmail.io", "disposable.email"}

func mailforspamHeaders(req *http.Request) {
	req.Header.Set("Accept", "application/json")
	req.Header.Set("Referer", "https://mailforspam.com/")
	req.Header.Set("Origin", "https://mailforspam.com")
	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0")
}

func mailforspamRandomLocal() string {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	b := make([]byte, 19)
	copy(b, "sdk")
	for i := 3; i < len(b); i++ {
		b[i] = chars[rand.Intn(len(chars))]
	}
	return string(b)
}

func mailforspamPickDomain(preferred *string) string {
	if preferred != nil {
		p := strings.TrimPrefix(strings.ToLower(strings.TrimSpace(*preferred)), "@")
		for _, d := range mailforspamDomains {
			if strings.ToLower(d) == p {
				return d
			}
		}
	}
	return mailforspamDefaultDomain
}

func mailforspamString(v any) string {
	if v == nil {
		return ""
	}
	switch x := v.(type) {
	case string:
		return strings.TrimSpace(x)
	default:
		return strings.TrimSpace(fmt.Sprint(x))
	}
}

func mailforspamEmailsURL(email string, pageSize int) string {
	return fmt.Sprintf(
		"%s/mailboxes/%s/emails?page=1&page_size=%d&sort_by=date&sort_order=desc",
		mailforspamBase,
		url.PathEscape(email),
		pageSize,
	)
}

type mailforspamListResponse struct {
	Emails []map[string]any `json:"emails"`
}

func mailforspamGetJSON(u string) ([]byte, error) {
	req, err := http.NewRequest("GET", u, nil)
	if err != nil {
		return nil, err
	}
	mailforspamHeaders(req)
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
		return nil, fmt.Errorf("mailforspam: http %d", resp.StatusCode)
	}
	return body, nil
}

func MailforspamGenerate(domain *string, channel ...string) (*CreatedMailbox, error) {
	ch := "mailforspam"
	if len(channel) > 0 && channel[0] != "" {
		ch = channel[0]
	}
	email := fmt.Sprintf("%s@%s", mailforspamRandomLocal(), mailforspamPickDomain(domain))
	if _, err := mailforspamGetJSON(mailforspamEmailsURL(email, 1)); err != nil {
		return nil, fmt.Errorf("mailforspam validate mailbox: %w", err)
	}
	return &CreatedMailbox{Channel: ch, Email: email}, nil
}

func mailforspamFetchMessage(id, email string) (map[string]any, error) {
	u := fmt.Sprintf(
		"%s/mailboxes/%s/emails/%s",
		mailforspamBase,
		url.PathEscape(email),
		url.PathEscape(id),
	)
	body, err := mailforspamGetJSON(u)
	if err != nil {
		return nil, err
	}
	var raw map[string]any
	if err := json.Unmarshal(body, &raw); err != nil {
		return nil, err
	}
	return raw, nil
}

func mailforspamFlatten(raw map[string]any, recipient string) map[string]any {
	to := mailforspamString(raw["receiver"])
	if to == "" {
		to = recipient
	}
	return map[string]any{
		"id":          raw["id"],
		"from":        mailforspamString(raw["sender"]),
		"to":          to,
		"subject":     raw["subject"],
		"text":        raw["body_text"],
		"html":        raw["body_html"],
		"date":        raw["date"],
		"isRead":      raw["readAt"] != nil,
		"attachments": raw["attachments"],
	}
}

func MailforspamGetEmails(email string) ([]NormEmail, error) {
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("mailforspam: empty email")
	}
	body, err := mailforspamGetJSON(mailforspamEmailsURL(email, 100))
	if err != nil {
		return nil, err
	}
	var data mailforspamListResponse
	if err := json.Unmarshal(body, &data); err != nil {
		return nil, err
	}
	out := make([]NormEmail, 0, len(data.Emails))
	for _, item := range data.Emails {
		id := mailforspamString(item["id"])
		if id == "" {
			continue
		}
		detail, err := mailforspamFetchMessage(id, email)
		if err != nil {
			out = append(out, NormalizeMap(mailforspamFlatten(item, email), email))
			continue
		}
		out = append(out, NormalizeMap(mailforspamFlatten(detail, email), email))
	}
	return out, nil
}
