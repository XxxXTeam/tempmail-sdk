package provider

import (
	"crypto/rand"
	"encoding/json"
	"fmt"
	"io"
	"math/big"
	"net/url"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

const neighboursBaseURL = "https://neighbours.sh/api/v1"

var neighboursHeaders = map[string]string{
	"Accept":     "application/json",
	"User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
}

type neighboursDomainListResponse struct {
	Data struct {
		Domains []string `json:"domains"`
	} `json:"data"`
}

type neighboursInboxResponse struct {
	Data []map[string]any `json:"data"`
}

func neighboursRequest(path string, allowNotFound bool) ([]byte, error) {
	req, err := http.NewRequest(http.MethodGet, neighboursBaseURL+path, nil)
	if err != nil {
		return nil, err
	}
	for k, v := range neighboursHeaders {
		req.Header.Set(k, v)
	}

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	if allowNotFound && resp.StatusCode == http.StatusNotFound {
		return nil, nil
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("neighbours http %d", resp.StatusCode)
	}
	return raw, nil
}

func neighboursJSON(path string, out any, allowNotFound bool) error {
	raw, err := neighboursRequest(path, allowNotFound)
	if err != nil {
		return err
	}
	if raw == nil {
		return nil
	}
	return json.Unmarshal(raw, out)
}

func neighboursRandomInt(n int) int {
	if n <= 1 {
		return 0
	}
	v, err := rand.Int(rand.Reader, big.NewInt(int64(n)))
	if err != nil {
		return 0
	}
	return int(v.Int64())
}

func neighboursRandomLocal() string {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	var b strings.Builder
	b.WriteString("sdk")
	for i := 0; i < 16; i++ {
		b.WriteByte(chars[neighboursRandomInt(len(chars))])
	}
	return b.String()
}

func neighboursAnyString(value any) string {
	switch v := value.(type) {
	case nil:
		return ""
	case string:
		return strings.TrimSpace(v)
	case []any:
		for _, item := range v {
			if hit := neighboursAnyString(item); hit != "" {
				return hit
			}
		}
		return ""
	case map[string]any:
		if s, ok := v["address"].(string); ok && strings.TrimSpace(s) != "" {
			return strings.TrimSpace(s)
		}
		if s, ok := v["text"].(string); ok && strings.Contains(s, "@") {
			return strings.TrimSpace(s)
		}
		if inner, ok := v["value"]; ok {
			return neighboursAnyString(inner)
		}
		return ""
	default:
		return strings.TrimSpace(fmt.Sprint(v))
	}
}

func neighboursFirstAddress(value any) string {
	if hit := neighboursAnyString(value); hit != "" {
		return hit
	}
	return ""
}

func neighboursFlattenMessage(raw map[string]any, recipient string) map[string]any {
	text := neighboursAnyString(raw["text"])
	if text == "" {
		text = neighboursAnyString(raw["snippet"])
	}
	date := neighboursAnyString(raw["date"])
	if date == "" {
		date = neighboursAnyString(raw["received_at"])
	}
	to := neighboursFirstAddress(raw["to"])
	if to == "" {
		to = recipient
	}
	return map[string]any{
		"id":          neighboursAnyString(raw["uid"]),
		"from":        neighboursFirstAddress(raw["from"]),
		"to":          to,
		"subject":     neighboursAnyString(raw["subject"]),
		"text":        text,
		"html":        neighboursAnyString(raw["html"]),
		"date":        date,
		"attachments": raw["attachments"],
	}
}

func neighboursDetail(address, uid string) map[string]any {
	raw, err := neighboursRequest("/inbox/"+url.PathEscape(address)+"/"+url.PathEscape(uid), true)
	if err != nil || raw == nil {
		return nil
	}
	var data struct {
		Data map[string]any `json:"data"`
	}
	if err := json.Unmarshal(raw, &data); err != nil {
		return nil
	}
	if len(data.Data) == 0 {
		return nil
	}
	return data.Data
}

func NeighboursGenerate(domain *string) (*CreatedMailbox, error) {
	var data neighboursDomainListResponse
	if err := neighboursJSON("/config/domains", &data, false); err != nil {
		return nil, err
	}
	domains := make([]string, 0, len(data.Data.Domains))
	for _, item := range data.Data.Domains {
		if hit := strings.TrimSpace(strings.ToLower(item)); hit != "" {
			domains = append(domains, hit)
		}
	}
	if len(domains) == 0 {
		return nil, fmt.Errorf("neighbours: domain list is empty")
	}

	selected := domains[neighboursRandomInt(len(domains))]
	if domain != nil && strings.TrimSpace(*domain) != "" {
		wanted := strings.TrimSpace(strings.ToLower(*domain))
		selected = ""
		for _, item := range domains {
			if item == wanted {
				selected = item
				break
			}
		}
		if selected == "" {
			return nil, fmt.Errorf("neighbours: unsupported domain %s", *domain)
		}
	}

	email := neighboursRandomLocal() + "@" + selected
	return &CreatedMailbox{
		Channel: "neighbours",
		Email:   email,
		Token:   email,
	}, nil
}

func NeighboursGetEmails(email string) ([]NormEmail, error) {
	address := strings.TrimSpace(email)
	if address == "" {
		return nil, fmt.Errorf("neighbours: empty email")
	}

	var data neighboursInboxResponse
	if err := neighboursJSON("/inbox/"+url.PathEscape(address), &data, true); err != nil {
		return nil, err
	}
	if len(data.Data) == 0 {
		return []NormEmail{}, nil
	}

	out := make([]NormEmail, 0, len(data.Data))
	for _, row := range data.Data {
		raw := row
		if uid := neighboursAnyString(row["uid"]); uid != "" {
			if detail := neighboursDetail(address, uid); detail != nil {
				raw = detail
			}
		}
		out = append(out, NormalizeMap(neighboursFlattenMessage(raw, address), address))
	}
	return out, nil
}
