package provider

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"math/rand"
	"strings"
	"sync"

	http "github.com/bogdanfinn/fhttp"
)

const mailCxBase = "https://api.mail.cx"

type mailCxDomainRow struct {
	Domain string `json:"domain"`
}

type mailCxDomainsResp struct {
	Domains []mailCxDomainRow `json:"domains"`
}

type mailCxAccountResp struct {
	Address string `json:"address"`
	Token   string `json:"token"`
}

type mailCxListResp struct {
	Messages []map[string]interface{} `json:"messages"`
}

func mailCxRandomStr(n int) string {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	b := make([]byte, n)
	for i := range b {
		b[i] = chars[rand.Intn(len(chars))]
	}
	return string(b)
}

func mailCxGetDomains() ([]string, error) {
	req, err := http.NewRequest("GET", mailCxBase+"/api/domains", nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("Accept", "application/json")
	req.Header.Set("Origin", "https://mail.cx")
	req.Header.Set("Referer", "https://mail.cx/")

	client := HTTPClient()
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	if resp.StatusCode != 200 {
		return nil, fmt.Errorf("mail-cx domains: %d", resp.StatusCode)
	}

	var parsed mailCxDomainsResp
	if err := json.Unmarshal(body, &parsed); err != nil {
		return nil, err
	}
	var out []string
	for _, d := range parsed.Domains {
		if d.Domain != "" {
			out = append(out, d.Domain)
		}
	}
	return out, nil
}

func mailCxCreateAccount(address, password string) (*mailCxAccountResp, error) {
	reqBody, _ := json.Marshal(map[string]string{"address": address, "password": password})
	req, err := http.NewRequest("POST", mailCxBase+"/api/accounts", bytes.NewReader(reqBody))
	if err != nil {
		return nil, err
	}
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Accept", "application/json")
	req.Header.Set("Origin", "https://mail.cx")
	req.Header.Set("Referer", "https://mail.cx/")

	client := HTTPClient()
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	if resp.StatusCode != 201 {
		return nil, fmt.Errorf("mail-cx create account: %d %s", resp.StatusCode, string(body))
	}
	var acc mailCxAccountResp
	if err := json.Unmarshal(body, &acc); err != nil {
		return nil, err
	}
	if acc.Token == "" || acc.Address == "" {
		return nil, fmt.Errorf("mail-cx: invalid account response")
	}
	return &acc, nil
}

// MailCxGenerate 创建临时邮箱；可选 domain 指定系统域名（须在列表中）
func MailCxGenerate(domain *string) (*CreatedMailbox, error) {
	domains, err := mailCxGetDomains()
	if err != nil {
		return nil, err
	}
	if len(domains) == 0 {
		return nil, fmt.Errorf("no available domains")
	}
	if domain != nil && *domain != "" {
		want := strings.ToLower(strings.TrimPrefix(*domain, "@"))
		var filtered []string
		for _, d := range domains {
			if strings.ToLower(d) == want {
				filtered = append(filtered, d)
			}
		}
		if len(filtered) > 0 {
			domains = filtered
		}
	}

	var lastErr error
	for i := 0; i < 8; i++ {
		domain := domains[rand.Intn(len(domains))]
		address := fmt.Sprintf("%s@%s", mailCxRandomStr(12), domain)
		password := mailCxRandomStr(16)
		acc, err := mailCxCreateAccount(address, password)
		if err != nil {
			lastErr = err
			if strings.Contains(err.Error(), "409") {
				continue
			}
			return nil, err
		}
		return &CreatedMailbox{Channel: "mail-cx", Email: acc.Address, Token: acc.Token}, nil
	}
	if lastErr != nil {
		return nil, lastErr
	}
	return nil, fmt.Errorf("mail-cx: could not create account")
}

func mailCxMessageID(raw map[string]interface{}) string {
	switch v := raw["id"].(type) {
	case string:
		return v
	case float64:
		return fmt.Sprintf("%.0f", v)
	default:
		return fmt.Sprint(v)
	}
}

func mailCxFlattenMessage(raw map[string]interface{}, recipientEmail string) map[string]interface{} {
	id := mailCxMessageID(raw)

	flat := map[string]interface{}{
		"id":           id,
		"sender":       raw["sender"],
		"from":         raw["from"],
		"address":      raw["address"],
		"subject":      raw["subject"],
		"preview_text": raw["preview_text"],
		"text_body":    raw["text_body"],
		"html_body":    raw["html_body"],
		"created_at":   raw["created_at"],
	}
	if flat["address"] == nil || flat["address"] == "" {
		flat["address"] = recipientEmail
	}

	if attArr, ok := raw["attachments"].([]interface{}); ok {
		for _, a := range attArr {
			if m, ok := a.(map[string]interface{}); ok {
				idx := m["index"]
				m["url"] = fmt.Sprintf("%s/api/messages/%s/attachments/%v", mailCxBase, id, idx)
			}
		}
		flat["attachments"] = attArr
	}

	return flat
}

func MailCxGetEmails(token string, email string) ([]NormEmail, error) {
	req, err := http.NewRequest("GET", mailCxBase+"/api/messages?page=1", nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("Accept", "application/json")
	req.Header.Set("Origin", "https://mail.cx")
	req.Header.Set("Referer", "https://mail.cx/")
	req.Header.Set("Authorization", "Bearer "+token)

	client := HTTPClient()
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	if resp.StatusCode != 200 {
		return nil, fmt.Errorf("mail-cx list messages: %d", resp.StatusCode)
	}

	var list mailCxListResp
	if err := json.Unmarshal(body, &list); err != nil {
		return nil, err
	}
	if len(list.Messages) == 0 {
		return []NormEmail{}, nil
	}

	type res struct {
		idx int
		raw map[string]interface{}
	}
	results := make([]res, len(list.Messages))
	var wg sync.WaitGroup

	for i, msg := range list.Messages {
		wg.Add(1)
		go func(idx int, m map[string]interface{}) {
			defer wg.Done()
			mid := ""
			switch v := m["id"].(type) {
			case string:
				mid = v
			case float64:
				mid = fmt.Sprintf("%.0f", v)
			}
			if mid == "" {
				results[idx] = res{idx: idx, raw: m}
				return
			}
			dreq, err := http.NewRequest("GET", mailCxBase+"/api/messages/"+mid, nil)
			if err != nil {
				results[idx] = res{idx: idx, raw: m}
				return
			}
			dreq.Header.Set("Accept", "application/json")
			dreq.Header.Set("Origin", "https://mail.cx")
			dreq.Header.Set("Referer", "https://mail.cx/")
			dreq.Header.Set("Authorization", "Bearer "+token)
			dresp, err := client.Do(dreq)
			if err != nil {
				results[idx] = res{idx: idx, raw: m}
				return
			}
			func() {
				defer dresp.Body.Close()
				db, err := io.ReadAll(dresp.Body)
				if err != nil || dresp.StatusCode != 200 {
					results[idx] = res{idx: idx, raw: m}
					return
				}
				var detail map[string]interface{}
				if json.Unmarshal(db, &detail) != nil {
					results[idx] = res{idx: idx, raw: m}
					return
				}
				results[idx] = res{idx: idx, raw: detail}
			}()
		}(i, msg)
	}
	wg.Wait()

	emails := make([]NormEmail, 0, len(results))
	for _, r := range results {
		if r.raw == nil {
			continue
		}
		flat := mailCxFlattenMessage(r.raw, email)
		emails = append(emails, NormalizeMap(flat, email))
	}
	return emails, nil
}
