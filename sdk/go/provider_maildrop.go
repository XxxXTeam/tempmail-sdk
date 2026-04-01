package tempemail

import (
	"encoding/json"
	"fmt"
	"io"
	"math/rand"
	"net/url"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

/**
 * Maildrop — https://maildrop.cx
 * GET /api/suffixes.php、GET /api/emails.php
 */

const maildropBase = "https://maildrop.cx"
const maildropExcludedSuffix = "transformer.edu.kg"

func maildropDefaultHeaders(req *http.Request) {
	req.Header.Set("Accept", "application/json, text/plain, */*")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6")
	req.Header.Set("Cache-Control", "no-cache")
	req.Header.Set("DNT", "1")
	req.Header.Set("Pragma", "no-cache")
	req.Header.Set("Referer", "https://maildrop.cx/zh-cn/app")
	req.Header.Set("sec-ch-ua", `"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"`)
	req.Header.Set("sec-ch-ua-mobile", "?0")
	req.Header.Set("sec-ch-ua-platform", `"Windows"`)
	req.Header.Set("sec-fetch-dest", "empty")
	req.Header.Set("sec-fetch-mode", "cors")
	req.Header.Set("sec-fetch-site", "same-origin")
	req.Header.Set("User-Agent", GetCurrentUA())
	req.Header.Set("x-requested-with", "XMLHttpRequest")
}

func maildropRandomLocal(n int) string {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	b := make([]byte, n)
	for i := range b {
		b[i] = chars[rand.Intn(len(chars))]
	}
	return string(b)
}

func maildropFetchSuffixes() ([]string, error) {
	req, err := http.NewRequest("GET", maildropBase+"/api/suffixes.php", nil)
	if err != nil {
		return nil, err
	}
	maildropDefaultHeaders(req)

	client := HTTPClient()
	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("maildrop suffixes request: %w", err)
	}
	defer resp.Body.Close()
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("maildrop suffixes: %d", resp.StatusCode)
	}
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	var arr []string
	if err := json.Unmarshal(body, &arr); err != nil {
		return nil, fmt.Errorf("maildrop suffixes parse: %w", err)
	}
	ex := strings.ToLower(maildropExcludedSuffix)
	var out []string
	for _, s := range arr {
		t := strings.TrimSpace(s)
		if t == "" || strings.ToLower(t) == ex {
			continue
		}
		out = append(out, t)
	}
	if len(out) == 0 {
		return nil, fmt.Errorf("maildrop: no domains available")
	}
	return out, nil
}

func maildropPickSuffix(suffixes []string, preferred *string) string {
	if preferred != nil {
		p := strings.TrimSpace(*preferred)
		if p != "" {
			pl := strings.ToLower(p)
			for _, d := range suffixes {
				if strings.ToLower(d) == pl {
					return d
				}
			}
		}
	}
	return suffixes[rand.Intn(len(suffixes))]
}

func maildropCxDateToRFC3339(s string) string {
	s = strings.TrimSpace(s)
	if len(s) == 19 && s[10] == ' ' {
		return s[:10] + "T" + s[11:] + "Z"
	}
	return s
}

/*
 * maildropGenerate 随机本地部分 + 可选指定域名（须在后缀列表中且未被排除）
 */
func maildropGenerate(domain *string) (*EmailInfo, error) {
	suffixes, err := maildropFetchSuffixes()
	if err != nil {
		return nil, err
	}
	dom := maildropPickSuffix(suffixes, domain)
	local := maildropRandomLocal(10)
	email := local + "@" + dom
	return &EmailInfo{
		Channel: ChannelMaildrop,
		Email:   email,
		token:   email,
	}, nil
}

type maildropEmailsResp struct {
	Emails []struct {
		ID          json.RawMessage `json:"id"`
		FromAddr    string          `json:"from_addr"`
		Subject     string          `json:"subject"`
		Date        string          `json:"date"`
		IsRead      json.RawMessage `json:"isRead"`
		Description string          `json:"description"`
	} `json:"emails"`
}

func maildropRawToString(raw json.RawMessage) string {
	if len(raw) == 0 {
		return ""
	}
	var s string
	if json.Unmarshal(raw, &s) == nil {
		return s
	}
	var n float64
	if json.Unmarshal(raw, &n) == nil {
		return fmt.Sprintf("%.0f", n)
	}
	return string(raw)
}

func maildropParseIsRead(raw json.RawMessage) bool {
	if len(raw) == 0 {
		return false
	}
	var b bool
	if json.Unmarshal(raw, &b) == nil {
		return b
	}
	var n int
	if json.Unmarshal(raw, &n) == nil {
		return n != 0
	}
	return false
}

/*
 * maildropGetEmails 列表仅含 description 摘要
 */
func maildropGetEmails(token string, email string) ([]Email, error) {
	addr := strings.TrimSpace(email)
	if addr == "" {
		addr = strings.TrimSpace(token)
	}
	if addr == "" {
		return nil, fmt.Errorf("maildrop: empty address")
	}
	q := url.Values{}
	q.Set("addr", addr)
	q.Set("page", "1")
	q.Set("limit", "20")
	full := maildropBase + "/api/emails.php?" + q.Encode()

	req, err := http.NewRequest("GET", full, nil)
	if err != nil {
		return nil, err
	}
	maildropDefaultHeaders(req)

	client := HTTPClient()
	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("maildrop emails: %w", err)
	}
	defer resp.Body.Close()
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("maildrop emails: %d", resp.StatusCode)
	}
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	var data maildropEmailsResp
	if err := json.Unmarshal(body, &data); err != nil {
		return nil, fmt.Errorf("maildrop emails parse: %w", err)
	}

	out := make([]Email, 0, len(data.Emails))
	for _, row := range data.Emails {
		desc := strings.TrimSpace(row.Description)
		out = append(out, Email{
			ID:          maildropRawToString(row.ID),
			From:        strings.TrimSpace(row.FromAddr),
			To:          addr,
			Subject:     strings.TrimSpace(row.Subject),
			Text:        desc,
			HTML:        "",
			Date:        maildropCxDateToRFC3339(row.Date),
			IsRead:      maildropParseIsRead(row.IsRead),
			Attachments: []EmailAttachment{},
		})
	}
	return out, nil
}
