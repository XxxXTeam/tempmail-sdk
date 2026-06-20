package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"math/rand"
	"net/url"
	"strings"
	"time"

	http "github.com/bogdanfinn/fhttp"
	tls_client "github.com/bogdanfinn/tls-client"
)

const mailCxBaseURL = "https://mail.cx"

type mailCxDomain struct {
	Domain  string `json:"domain"`
	Default bool   `json:"default"`
}

type mailCxConfig struct {
	SystemDomains []mailCxDomain `json:"system_domains"`
	TTLSeconds    int64          `json:"ttl_seconds"`
}

type mailCxInboxResponse struct {
	Emails []map[string]interface{} `json:"emails"`
}

func mailCxClientID() string {
	return "tempmail-sdk-" + randomStr(16)
}

func mailCxApplyHeaders(req *http.Request, clientID string) {
	req.Header.Set("Accept", "application/json")
	req.Header.Set("Origin", mailCxBaseURL)
	req.Header.Set("Referer", mailCxBaseURL+"/")
	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0")
	req.Header.Set("X-Client-ID", clientID)
}

func mailCxHTTPClient() tls_client.HttpClient {
	timeout := 35 * time.Second
	if GetConfigSnapshot != nil {
		cfg := GetConfigSnapshot()
		if cfg.Timeout > int64(timeout) {
			timeout = time.Duration(cfg.Timeout)
		}
		options := []tls_client.HttpClientOption{
			tls_client.WithTimeoutSeconds(int(timeout.Seconds())),
			tls_client.WithCookieJar(tls_client.NewCookieJar()),
		}
		if cfg.Insecure {
			options = append(options, tls_client.WithInsecureSkipVerify())
		}
		if cfg.Proxy != "" {
			options = append(options, tls_client.WithProxyUrl(cfg.Proxy))
		}
		client, err := tls_client.NewHttpClient(tls_client.NewNoopLogger(), options...)
		if err == nil {
			return client
		}
	}
	client, err := tls_client.NewHttpClient(
		tls_client.NewNoopLogger(),
		tls_client.WithTimeoutSeconds(int(timeout.Seconds())),
		tls_client.WithCookieJar(tls_client.NewCookieJar()),
	)
	if err == nil {
		return client
	}
	return HTTPClient()
}

func mailCxGetConfig(clientID string) (*mailCxConfig, error) {
	req, err := http.NewRequest("GET", mailCxBaseURL+"/v1/config", nil)
	if err != nil {
		return nil, err
	}
	mailCxApplyHeaders(req, clientID)

	resp, err := mailCxHTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("mail-cx config: %d", resp.StatusCode)
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	var cfg mailCxConfig
	if err := json.Unmarshal(body, &cfg); err != nil {
		return nil, err
	}
	return &cfg, nil
}

func mailCxPickDomain(cfg *mailCxConfig, preferred *string) (string, error) {
	domains := make([]string, 0, len(cfg.SystemDomains))
	for _, d := range cfg.SystemDomains {
		if strings.TrimSpace(d.Domain) != "" {
			domains = append(domains, strings.TrimSpace(d.Domain))
		}
	}
	if len(domains) == 0 {
		return "", fmt.Errorf("mail-cx: no system domains")
	}

	if preferred != nil {
		want := strings.TrimPrefix(strings.ToLower(strings.TrimSpace(*preferred)), "@")
		if want != "" {
			for _, d := range domains {
				if strings.ToLower(d) == want {
					return d, nil
				}
			}
		}
	}

	for _, d := range cfg.SystemDomains {
		if d.Default && strings.TrimSpace(d.Domain) != "" {
			return strings.TrimSpace(d.Domain), nil
		}
	}
	return domains[0], nil
}

func mailCxFlattenListMessage(raw map[string]interface{}, recipient string) map[string]interface{} {
	return map[string]interface{}{
		"id":          raw["id"],
		"from":        mailCxFirstNonEmpty(raw["from_email"], raw["from_name"]),
		"to":          recipient,
		"subject":     raw["subject"],
		"text":        raw["preview_text"],
		"created_at":  raw["created_at"],
		"attachments": raw["attachments"],
	}
}

func mailCxFlattenDetail(raw map[string]interface{}, recipient string) map[string]interface{} {
	return map[string]interface{}{
		"id":          raw["id"],
		"from":        mailCxFirstNonEmpty(raw["from_email"], raw["from_name"]),
		"to":          recipient,
		"subject":     raw["subject"],
		"text_body":   raw["text_body"],
		"html_body":   raw["html_body"],
		"text":        mailCxFirstNonEmpty(raw["text_body"], raw["preview_text"]),
		"html":        raw["html_body"],
		"created_at":  raw["created_at"],
		"attachments": raw["attachments"],
	}
}

func mailCxGetDetail(clientID, id string) (map[string]interface{}, error) {
	req, err := http.NewRequest("GET", mailCxBaseURL+"/v1/email/"+url.PathEscape(id), nil)
	if err != nil {
		return nil, err
	}
	mailCxApplyHeaders(req, clientID)

	resp, err := mailCxHTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("mail-cx detail: %d", resp.StatusCode)
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	var data map[string]interface{}
	if err := json.Unmarshal(body, &data); err != nil {
		return nil, err
	}
	return data, nil
}

func MailCxGenerate(domain *string) (*CreatedMailbox, error) {
	clientID := mailCxClientID()
	cfg, err := mailCxGetConfig(clientID)
	if err != nil {
		return nil, err
	}

	pickedDomain, err := mailCxPickDomain(cfg, domain)
	if err != nil {
		return nil, err
	}

	email := randomStr(12) + "@" + pickedDomain
	info := &CreatedMailbox{
		Channel:   "mail-cx",
		Email:     email,
		Token:     clientID,
		CreatedAt: time.Now().UTC().Format(time.RFC3339),
	}
	if cfg.TTLSeconds > 0 {
		info.ExpiresAt = time.Now().Add(time.Duration(cfg.TTLSeconds) * time.Second).UTC().Format(time.RFC3339)
	}
	return info, nil
}

func MailCxGetEmails(clientID, email string) ([]NormEmail, error) {
	req, err := http.NewRequest("GET", mailCxBaseURL+"/v1/inbox/"+url.PathEscape(email), nil)
	if err != nil {
		return nil, err
	}
	mailCxApplyHeaders(req, clientID)

	resp, err := mailCxHTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if resp.StatusCode == 204 {
		return []NormEmail{}, nil
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("mail-cx inbox: %d", resp.StatusCode)
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	var data mailCxInboxResponse
	if err := json.Unmarshal(body, &data); err != nil {
		return nil, err
	}

	emails := make([]NormEmail, 0, len(data.Emails))
	for _, row := range data.Emails {
		raw := mailCxFlattenListMessage(row, email)
		if id, ok := row["id"].(string); ok && id != "" {
			if detail, err := mailCxGetDetail(clientID, id); err == nil {
				raw = mailCxFlattenDetail(detail, email)
			}
		}
		emails = append(emails, NormalizeMap(raw, email))
	}
	return emails, nil
}

func mailCxFirstNonEmpty(values ...interface{}) string {
	for _, value := range values {
		s := strings.TrimSpace(fmt.Sprint(value))
		if s != "" && s != "<nil>" {
			return s
		}
	}
	return ""
}

func init() {
	rand.Seed(time.Now().UnixNano())
}
