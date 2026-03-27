package tempemail

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"math/rand"
	"net/url"
	"regexp"
	"strings"

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

type boomlifyDomainEntry struct {
	ID     string
	Domain string
}

var boomlifyInboxUUID = regexp.MustCompile(`(?i)^[0-9a-f]{8}-[0-9a-f]{4}-[1-5][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$`)

func boomlifyGetDomains() ([]boomlifyDomainEntry, error) {
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
	var out []boomlifyDomainEntry
	for _, m := range items {
		dom, _ := m["domain"].(string)
		id, _ := m["id"].(string)
		if dom == "" || id == "" {
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
			out = append(out, boomlifyDomainEntry{ID: id, Domain: dom})
		}
	}
	return out, nil
}

func boomlifyCreatePublicInbox(domainID string) (string, error) {
	payload := map[string]string{"domainId": domainID}
	raw, err := json.Marshal(payload)
	if err != nil {
		return "", err
	}
	req, err := http.NewRequest("POST", boomlifyBaseURL+"/emails/public/create", bytes.NewReader(raw))
	if err != nil {
		return "", err
	}
	boomlifyDefaultHeaders(req)
	req.Header.Set("Content-Type", "application/json")

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", err
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return "", fmt.Errorf("boomlify public create: %d", resp.StatusCode)
	}
	var cr struct {
		ID              string `json:"id"`
		Error           string `json:"error"`
		Message         string `json:"message"`
		CaptchaRequired bool   `json:"captchaRequired"`
	}
	if err := json.Unmarshal(body, &cr); err != nil {
		return "", err
	}
	if cr.Error != "" {
		msg := cr.Error
		if cr.Message != "" {
			msg = msg + " — " + cr.Message
		}
		if cr.CaptchaRequired {
			msg = msg + "（服务端限流/需验证码，请稍后重试）"
		}
		return "", fmt.Errorf("boomlify: %s", msg)
	}
	if cr.ID == "" {
		return "", fmt.Errorf("boomlify: public create returned no inbox id")
	}
	return cr.ID, nil
}

func boomlifyGenerate() (*EmailInfo, error) {
	domains, err := boomlifyGetDomains()
	if err != nil {
		return nil, err
	}
	if len(domains) == 0 {
		return nil, fmt.Errorf("boomlify: no domains")
	}
	pick := domains[rand.Intn(len(domains))]
	boxID, err := boomlifyCreatePublicInbox(pick.ID)
	if err != nil {
		return nil, err
	}
	addr := fmt.Sprintf("%s@%s", boxID, pick.Domain)

	return &EmailInfo{
		Channel: ChannelBoomlify,
		Email:   addr,
		token:   boxID,
	}, nil
}

func boomlifyInboxPathSegment(email string) string {
	at := strings.LastIndex(email, "@")
	local := email
	if at > 0 {
		local = email[:at]
	}
	if boomlifyInboxUUID.MatchString(local) {
		return local
	}
	return email
}

func boomlifyGetEmails(email string) ([]Email, error) {
	seg := boomlifyInboxPathSegment(email)
	u := fmt.Sprintf("%s/emails/public/%s", boomlifyBaseURL, url.PathEscape(seg))
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
