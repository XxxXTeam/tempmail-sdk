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

const inboxesBase = "https://inboxes.com/api/v2"
const inboxesDefaultDomain = "blondmail.com"

func inboxesHeaders(req *http.Request) {
	req.Header.Set("Accept", "application/json")
	req.Header.Set("Origin", "https://inboxes.com")
	req.Header.Set("Referer", "https://inboxes.com/")
	req.Header.Set("User-Agent", "Mozilla/5.0")
}

func inboxesJSON(u string, out any) error {
	req, err := http.NewRequest(http.MethodGet, u, nil)
	if err != nil {
		return err
	}
	inboxesHeaders(req)
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
		return fmt.Errorf("inboxes http %d", resp.StatusCode)
	}
	return json.Unmarshal(body, out)
}

func inboxesRandomLocal() string {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	b := make([]byte, 19)
	copy(b, "sdk")
	for i := 3; i < len(b); i++ {
		b[i] = chars[rand.Intn(len(chars))]
	}
	return string(b)
}

func inboxesGetDomains() ([]string, error) {
	var data struct {
		Domains []struct {
			QDN string `json:"qdn"`
		} `json:"domains"`
	}
	if err := inboxesJSON(inboxesBase+"/domain", &data); err != nil {
		return nil, err
	}
	domains := make([]string, 0, len(data.Domains))
	for _, item := range data.Domains {
		if d := strings.TrimSpace(item.QDN); d != "" {
			domains = append(domains, d)
		}
	}
	if len(domains) == 0 {
		return nil, fmt.Errorf("inboxes: no domains")
	}
	return domains, nil
}

func inboxesPickDomain(domains []string, preferred *string) string {
	if preferred != nil {
		want := strings.TrimPrefix(strings.ToLower(strings.TrimSpace(*preferred)), "@")
		if want != "" {
			for _, d := range domains {
				if strings.ToLower(d) == want {
					return d
				}
			}
		}
	}
	for _, d := range domains {
		if d == inboxesDefaultDomain {
			return d
		}
	}
	return domains[0]
}

func InboxesGenerate(domain *string) (*CreatedMailbox, error) {
	domains, err := inboxesGetDomains()
	if err != nil {
		return nil, err
	}
	email := fmt.Sprintf("%s@%s", inboxesRandomLocal(), inboxesPickDomain(domains, domain))
	return &CreatedMailbox{Channel: "inboxes", Email: email}, nil
}

func inboxesFetchDetail(uid string) (map[string]any, error) {
	var raw map[string]any
	u := inboxesBase + "/message/" + url.PathEscape(uid)
	if err := inboxesJSON(u, &raw); err != nil {
		return nil, err
	}
	return raw, nil
}

func inboxesFlatten(raw map[string]any, recipient string) map[string]any {
	from := raw["f"]
	if sf, ok := raw["sf"].(string); ok && strings.TrimSpace(sf) != "" {
		from = sf
	}
	text := raw["text"]
	html := raw["html"]
	return map[string]any{
		"id":           raw["uid"],
		"from":         from,
		"to":           inboxesFirstNonEmpty(raw["ib"], recipient),
		"subject":      raw["s"],
		"text":         text,
		"html":         html,
		"preview_text": raw["ph"],
		"date":         raw["cr"],
		"isRead":       raw["ru"],
		"attachments":  raw["at"],
	}
}

func inboxesFirstNonEmpty(value any, fallback string) any {
	if s, ok := value.(string); ok && strings.TrimSpace(s) != "" {
		return s
	}
	return fallback
}

func InboxesGetEmails(email string) ([]NormEmail, error) {
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("inboxes: empty email")
	}
	u := fmt.Sprintf("%s/inbox/%s", inboxesBase, url.PathEscape(email))
	var data struct {
		Messages []map[string]any `json:"msgs"`
	}
	if err := inboxesJSON(u, &data); err != nil {
		return nil, err
	}
	out := make([]NormEmail, 0, len(data.Messages))
	for _, row := range data.Messages {
		raw := row
		if uid := strings.TrimSpace(fmt.Sprint(row["uid"])); uid != "" {
			if detail, err := inboxesFetchDetail(uid); err == nil {
				raw = detail
			}
		}
		out = append(out, NormalizeMap(inboxesFlatten(raw, email), email))
	}
	return out, nil
}
