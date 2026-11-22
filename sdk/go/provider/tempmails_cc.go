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

const tempmailsCcBase = "https://tempmails.cc"

func tempmailsCcRandomLocal() string {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	b := make([]byte, 16)
	for i := range b {
		b[i] = chars[rand.Intn(len(chars))]
	}
	return string(b)
}

func tempmailsCcGetJSON(path string) (map[string]any, error) {
	req, err := http.NewRequest("GET", tempmailsCcBase+path, nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("Accept", "application/json")
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
		return nil, fmt.Errorf("tempmails-cc: HTTP %d", resp.StatusCode)
	}
	var out map[string]any
	if err := json.Unmarshal(body, &out); err != nil {
		return nil, err
	}
	return out, nil
}

/* tempmailsCcPickDomain 选择公开免费域名列表中的第一个 */
func tempmailsCcPickDomain() (string, error) {
	raw, err := tempmailsCcGetJSON("/api/domains")
	if err != nil {
		return "", err
	}
	items, _ := raw["domainInfo"].([]any)
	for _, item := range items {
		if info, ok := item.(map[string]any); ok {
			if domain := strings.TrimSpace(fmt.Sprint(info["domain"])); domain != "" {
				return domain, nil
			}
		}
	}
	return "", fmt.Errorf("tempmails-cc: no public domain")
}

/* TempmailsCcGenerate 在公开免费域名上生成地址，地址即读取凭据 */
func TempmailsCcGenerate() (*CreatedMailbox, error) {
	domain, err := tempmailsCcPickDomain()
	if err != nil {
		return nil, err
	}
	return &CreatedMailbox{
		Channel: "tempmails-cc",
		Email:   fmt.Sprintf("%s@%s", tempmailsCcRandomLocal(), domain),
	}, nil
}

/* TempmailsCcGenerateAtDomain 跳过选域，在指定固定域名上生成地址 */
func TempmailsCcGenerateAtDomain(domain, channel string) (*CreatedMailbox, error) {
	if strings.TrimSpace(domain) == "" {
		return nil, fmt.Errorf("tempmails-cc: empty domain")
	}
	return &CreatedMailbox{
		Channel: channel,
		Email:   fmt.Sprintf("%s@%s", tempmailsCcRandomLocal(), domain),
	}, nil
}

func tempmailsCcFlatten(raw map[string]any, recipient string) map[string]any {
	to := raw["to"]
	if arr, ok := to.([]any); ok && len(arr) > 0 {
		to = arr[0]
	}
	return map[string]any{
		"id":      raw["id"],
		"from":    raw["from"],
		"to":      to,
		"subject": raw["subject"],
		"text":    raw["textBody"],
		"html":    raw["htmlBody"],
		"date":    raw["date"],
		"seen":    raw["isRead"],
	}
}

/* TempmailsCcGetEmails 匿名读取详情必须携带 address 参数 */
func TempmailsCcGetEmails(email string) ([]NormEmail, error) {
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("tempmails-cc: empty email")
	}
	list, err := tempmailsCcGetJSON("/api/inbox/" + url.PathEscape(email))
	if err != nil {
		return nil, err
	}
	items, _ := list["messages"].([]any)
	out := make([]NormEmail, 0, len(items))
	for _, item := range items {
		summary, _ := item.(map[string]any)
		id := strings.TrimSpace(fmt.Sprint(summary["id"]))
		if id == "" {
			continue
		}
		detail, detailErr := tempmailsCcGetJSON(
			"/api/message/" + url.PathEscape(id) +
				"?address=" + url.QueryEscape(email) + "&mark_read=false")
		if detailErr == nil {
			out = append(out, NormalizeMap(tempmailsCcFlatten(detail, email), email))
			continue
		}
		out = append(out, NormalizeMap(map[string]any{
			"id":      id,
			"from":    summary["from"],
			"to":      email,
			"subject": summary["subject"],
			"date":    summary["date"],
			"seen":    summary["isRead"],
		}, email))
	}
	return out, nil
}
