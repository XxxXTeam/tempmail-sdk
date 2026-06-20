package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"math/rand"
	"net/url"
	"regexp"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

const catchmailBase = "https://api.catchmail.io/api/v1"
const catchmailDefaultDomain = "catchmail.io"

var catchmailDomains = []string{"catchmail.io", "mailistry.com", "zeppost.com"}

func catchmailHeaders(req *http.Request) {
	req.Header.Set("Accept", "application/json")
	req.Header.Set("Referer", "https://catchmail.io/")
	req.Header.Set("Origin", "https://catchmail.io")
	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0")
}

func catchmailRandomLocal() string {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	b := make([]byte, 19)
	copy(b, "sdk")
	for i := 3; i < len(b); i++ {
		b[i] = chars[rand.Intn(len(chars))]
	}
	return string(b)
}

func catchmailPickDomain(preferred *string) string {
	if preferred != nil {
		p := strings.TrimPrefix(strings.ToLower(strings.TrimSpace(*preferred)), "@")
		for _, d := range catchmailDomains {
			if strings.ToLower(d) == p {
				return d
			}
		}
	}
	return catchmailDefaultDomain
}

func catchmailCleanAddress(v any) string {
	switch x := v.(type) {
	case []string:
		if len(x) == 0 {
			return ""
		}
		return catchmailCleanAddress(x[0])
	case []any:
		if len(x) == 0 {
			return ""
		}
		return catchmailCleanAddress(x[0])
	case string:
		s := strings.TrimSpace(x)
		re := regexp.MustCompile(`<([^>]+)>`)
		if m := re.FindStringSubmatch(s); len(m) == 2 {
			return strings.TrimSpace(m[1])
		}
		return s
	default:
		if v == nil {
			return ""
		}
		return strings.TrimSpace(fmt.Sprint(v))
	}
}

func CatchmailGenerate(domain *string, channel ...string) (*CreatedMailbox, error) {
	ch := "catchmail"
	if len(channel) > 0 && channel[0] != "" {
		ch = channel[0]
	}
	email := fmt.Sprintf("%s@%s", catchmailRandomLocal(), catchmailPickDomain(domain))
	u := fmt.Sprintf("%s/mailbox?address=%s", catchmailBase, url.QueryEscape(email))
	req, err := http.NewRequest("GET", u, nil)
	if err != nil {
		return nil, err
	}
	catchmailHeaders(req)
	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	io.ReadAll(resp.Body)
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("catchmail create mailbox: %d", resp.StatusCode)
	}
	return &CreatedMailbox{Channel: ch, Email: email}, nil
}

func catchmailFetchMessage(id, email string) (map[string]any, error) {
	u := fmt.Sprintf("%s/message/%s?mailbox=%s", catchmailBase, url.PathEscape(id), url.QueryEscape(email))
	req, err := http.NewRequest("GET", u, nil)
	if err != nil {
		return nil, err
	}
	catchmailHeaders(req)
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
		return nil, fmt.Errorf("catchmail message: %d", resp.StatusCode)
	}
	var raw map[string]any
	if err := json.Unmarshal(body, &raw); err != nil {
		return nil, err
	}
	return raw, nil
}

func catchmailFlattenDetail(raw map[string]any, recipient string) map[string]any {
	body, _ := raw["body"].(map[string]any)
	flat := map[string]any{
		"id":          raw["id"],
		"from":        catchmailCleanAddress(raw["from"]),
		"to":          catchmailCleanAddress(raw["to"]),
		"subject":     raw["subject"],
		"text":        "",
		"html":        "",
		"date":        raw["date"],
		"attachments": raw["attachments"],
	}
	if flat["to"] == "" {
		flat["to"] = recipient
	}
	if body != nil {
		flat["text"] = body["text"]
		flat["html"] = body["html"]
	}
	return flat
}

func CatchmailGetEmails(email string) ([]NormEmail, error) {
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("catchmail: empty email")
	}
	u := fmt.Sprintf("%s/mailbox?address=%s", catchmailBase, url.QueryEscape(email))
	req, err := http.NewRequest("GET", u, nil)
	if err != nil {
		return nil, err
	}
	catchmailHeaders(req)
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
		return nil, fmt.Errorf("catchmail mailbox: %d", resp.StatusCode)
	}
	var data struct {
		Messages []map[string]any `json:"messages"`
	}
	if err := json.Unmarshal(body, &data); err != nil {
		return nil, err
	}
	out := make([]NormEmail, 0, len(data.Messages))
	for _, item := range data.Messages {
		id := strings.TrimSpace(fmt.Sprint(item["id"]))
		if id == "" {
			continue
		}
		if detail, err := catchmailFetchMessage(id, email); err == nil {
			out = append(out, NormalizeMap(catchmailFlattenDetail(detail, email), email))
			continue
		}
		flat := map[string]any{
			"id":      id,
			"from":    catchmailCleanAddress(item["from"]),
			"to":      item["mailbox"],
			"subject": item["subject"],
			"date":    item["date"],
		}
		out = append(out, NormalizeMap(flat, email))
	}
	return out, nil
}
