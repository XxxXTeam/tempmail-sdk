package tempemail

import (
	"encoding/json"
	"fmt"
	"io"
	"math/rand"
	"net/url"

	http "github.com/bogdanfinn/fhttp"
)

const boomlifyBaseURL = "https://v1.boomlify.com"

type boomlifyEmailRow struct {
	ID         string `json:"id"`
	FromEmail  string `json:"from_email"`
	FromName   string `json:"from_name"`
	Subject    string `json:"subject"`
	BodyHTML   string `json:"body_html"`
	BodyText   string `json:"body_text"`
	ReceivedAt string `json:"received_at"`
	TempEmail  string `json:"temp_email"`
}

func boomlifyDefaultHeaders(req *http.Request) {
	req.Header.Set("Accept", "application/json, text/plain, */*")
	req.Header.Set("Accept-Language", "zh")
	req.Header.Set("Origin", "https://boomlify.com")
	req.Header.Set("Referer", "https://boomlify.com/")
	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0")
	req.Header.Set("sec-ch-ua", `"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"`)
	req.Header.Set("sec-ch-ua-mobile", "?0")
	req.Header.Set("sec-ch-ua-platform", `"Windows"`)
	req.Header.Set("sec-fetch-dest", "empty")
	req.Header.Set("sec-fetch-mode", "cors")
	req.Header.Set("sec-fetch-site", "same-site")
	req.Header.Set("x-user-language", "zh")
}

func boomlifyRandomLocal(n int) string {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	b := make([]byte, n)
	for i := range b {
		b[i] = chars[rand.Intn(len(chars))]
	}
	return string(b)
}

func boomlifyGetDomains() ([]string, error) {
	req, err := http.NewRequest("GET", boomlifyBaseURL+"/domains/public", nil)
	if err != nil {
		return nil, err
	}
	boomlifyDefaultHeaders(req)

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
		return nil, fmt.Errorf("boomlify domains: %d", resp.StatusCode)
	}

	var items []map[string]interface{}
	if err := json.Unmarshal(body, &items); err != nil {
		return nil, err
	}
	var out []string
	for _, m := range items {
		dom, _ := m["domain"].(string)
		if dom == "" {
			continue
		}
		active := false
		switch v := m["is_active"].(type) {
		case float64:
			active = v == 1
		case bool:
			active = v
		}
		if !active {
			switch v := m["isActive"].(type) {
			case float64:
				active = v == 1
			case bool:
				active = v
			}
		}
		if active {
			out = append(out, dom)
		}
	}
	return out, nil
}

func boomlifyGenerate() (*EmailInfo, error) {
	domains, err := boomlifyGetDomains()
	if err != nil {
		return nil, err
	}
	if len(domains) == 0 {
		return nil, fmt.Errorf("boomlify: no domains")
	}
	domain := domains[rand.Intn(len(domains))]
	local := boomlifyRandomLocal(8)
	addr := fmt.Sprintf("%s@%s", local, domain)

	return &EmailInfo{
		Channel: ChannelBoomlify,
		Email:   addr,
	}, nil
}

func boomlifyGetEmails(email string) ([]Email, error) {
	u := fmt.Sprintf("%s/emails/public/%s", boomlifyBaseURL, url.PathEscape(email))
	req, err := http.NewRequest("GET", u, nil)
	if err != nil {
		return nil, err
	}
	boomlifyDefaultHeaders(req)

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
		return nil, fmt.Errorf("boomlify emails: %d", resp.StatusCode)
	}

	var rows []boomlifyEmailRow
	if err := json.Unmarshal(body, &rows); err != nil {
		return nil, err
	}

	emails := make([]Email, 0, len(rows))
	for _, raw := range rows {
		from := raw.FromEmail
		if from == "" {
			from = raw.FromName
		}
		flat := map[string]interface{}{
			"id":          raw.ID,
			"from":        from,
			"to":          email,
			"subject":     raw.Subject,
			"text":        raw.BodyText,
			"html":        raw.BodyHTML,
			"received_at": raw.ReceivedAt,
			"isRead":      false,
		}
		emails = append(emails, normalizeRawEmail(flat, email))
	}
	return emails, nil
}
