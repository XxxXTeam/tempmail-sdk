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

const temporamOrigin = "https://temporam.com"

type temporamDomainsResp struct {
	Data []struct {
		Domain string `json:"domain"`
	} `json:"data"`
}

type temporamEmailsResp struct {
	Data []map[string]any `json:"data"`
}

type temporamDetailResp struct {
	Data map[string]any `json:"data"`
}

func temporamJSONRequest(path string, out any) error {
	req, err := http.NewRequest("GET", temporamOrigin+path, nil)
	if err != nil {
		return err
	}
	req.Header.Set("Accept", "application/json")
	req.Header.Set("Accept-Encoding", "gzip, deflate, br")
	req.Header.Set("User-Agent", GetCurrentUA())

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	if err := CheckHTTPStatus(resp, "temporam"); err != nil {
		return err
	}
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return err
	}
	if err := json.Unmarshal(body, out); err != nil {
		return fmt.Errorf("temporam: parse response: %w", err)
	}
	return nil
}

func temporamRandomInt(n int) int {
	if n <= 1 {
		return 0
	}
	v, err := rand.Int(rand.Reader, big.NewInt(int64(n)))
	if err != nil {
		return 0
	}
	return int(v.Int64())
}

func temporamRandomLocal() string {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	var b strings.Builder
	b.WriteString("sdk")
	for i := 0; i < 18; i++ {
		b.WriteByte(chars[temporamRandomInt(len(chars))])
	}
	return b.String()
}

func temporamDomains() ([]string, error) {
	var data temporamDomainsResp
	if err := temporamJSONRequest("/api/domains", &data); err != nil {
		return nil, err
	}
	domains := make([]string, 0, len(data.Data))
	for _, item := range data.Data {
		domain := strings.TrimSpace(item.Domain)
		if domain != "" {
			domains = append(domains, domain)
		}
	}
	if len(domains) == 0 {
		return nil, fmt.Errorf("temporam: domain list is empty")
	}
	return domains, nil
}

func TemporamGenerate(domain *string) (*CreatedMailbox, error) {
	domains, err := temporamDomains()
	if err != nil {
		return nil, err
	}
	selected := domains[temporamRandomInt(len(domains))]
	if domain != nil && *domain != "" {
		selected = ""
		for _, item := range domains {
			if item == *domain {
				selected = item
				break
			}
		}
		if selected == "" {
			return nil, fmt.Errorf("temporam: unsupported domain %s", *domain)
		}
	}

	email := temporamRandomLocal() + "@" + selected
	return &CreatedMailbox{
		Channel: "temporam",
		Email:   email,
		Token:   email,
	}, nil
}

func temporamNormalize(raw map[string]any, recipient string) NormEmail {
	if raw["id"] == nil {
		raw["id"] = raw["uuid"]
	}
	if raw["from"] == nil {
		raw["from"] = raw["fromEmail"]
	}
	if raw["to"] == nil {
		raw["to"] = raw["toEmail"]
	}
	if raw["date"] == nil {
		raw["date"] = raw["createdAt"]
	}
	return NormalizeMap(raw, recipient)
}

func TemporamGetEmails(token string, email string) ([]NormEmail, error) {
	recipient := token
	if recipient == "" {
		recipient = email
	}
	if recipient == "" {
		return nil, fmt.Errorf("temporam: missing email")
	}

	var list temporamEmailsResp
	path := "/api/emails?email=" + url.QueryEscape(recipient) + "&limit=50"
	if err := temporamJSONRequest(path, &list); err != nil {
		return nil, err
	}

	out := make([]NormEmail, 0, len(list.Data))
	for _, row := range list.Data {
		raw := row
		if id, ok := row["id"].(string); ok && id != "" {
			var detail temporamDetailResp
			if err := temporamJSONRequest("/api/emails/"+url.PathEscape(id), &detail); err == nil && detail.Data != nil {
				raw = detail.Data
			}
		}
		out = append(out, temporamNormalize(raw, recipient))
	}
	return out, nil
}
