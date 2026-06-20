package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"strconv"
	"strings"
	"time"
	"unicode"

	http "github.com/bogdanfinn/fhttp"
)

const etempmailBaseURL = "https://etempmail.com"

func etempmailSetHeaders(req *http.Request) {
	req.Header.Set("Accept", "*/*")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6")
	req.Header.Set("Cache-Control", "no-cache")
	req.Header.Set("DNT", "1")
	req.Header.Set("Origin", etempmailBaseURL)
	req.Header.Set("Pragma", "no-cache")
	req.Header.Set("Referer", etempmailBaseURL+"/zh")
	req.Header.Set("sec-ch-ua", `"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"`)
	req.Header.Set("sec-ch-ua-mobile", "?0")
	req.Header.Set("sec-ch-ua-platform", `"Windows"`)
	req.Header.Set("sec-fetch-dest", "empty")
	req.Header.Set("sec-fetch-mode", "cors")
	req.Header.Set("sec-fetch-site", "same-origin")
	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0")
	req.Header.Set("X-Requested-With", "XMLHttpRequest")
}

func etempmailValidGeneratedAddress(address string) bool {
	if address == "" || len(address) > 254 || strings.TrimSpace(address) != address {
		return false
	}
	if strings.Count(address, "@") != 1 {
		return false
	}
	parts := strings.Split(address, "@")
	local, domain := parts[0], parts[1]
	if local == "" || len(local) > 64 ||
		strings.HasPrefix(local, ".") ||
		strings.HasSuffix(local, ".") ||
		strings.Contains(local, "..") {
		return false
	}
	for _, r := range local {
		if unicode.IsSpace(r) || unicode.IsControl(r) {
			return false
		}
	}
	return etempmailValidDomain(domain)
}

func etempmailValidDomain(domain string) bool {
	if domain == "" || len(domain) > 253 ||
		strings.HasPrefix(domain, ".") ||
		strings.HasSuffix(domain, ".") {
		return false
	}
	labels := strings.Split(domain, ".")
	if len(labels) < 2 {
		return false
	}
	for _, label := range labels {
		if label == "" || len(label) > 63 ||
			strings.HasPrefix(label, "-") ||
			strings.HasSuffix(label, "-") {
			return false
		}
		for _, r := range label {
			if (r < 'A' || r > 'Z') &&
				(r < 'a' || r > 'z') &&
				(r < '0' || r > '9') &&
				r != '-' {
				return false
			}
		}
	}
	return true
}

// EtempmailGenerate etempmail.com：先 GET /zh 拿会话 Cookie，再 POST /getEmailAddress；Token 为 Cookie 串供 getInbox 使用。
func EtempmailGenerate() (*CreatedMailbox, error) {
	req, err := http.NewRequest("GET", etempmailBaseURL+"/zh", nil)
	if err != nil {
		return nil, err
	}
	etempmailSetHeaders(req)
	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	if err := CheckHTTPStatus(resp, "etempmail session"); err != nil {
		resp.Body.Close()
		return nil, err
	}
	cookieHdr := moaktMergeCookies("", resp.Cookies())
	_, _ = io.Copy(io.Discard, resp.Body)
	resp.Body.Close()
	if cookieHdr == "" {
		return nil, fmt.Errorf("etempmail: no session cookie")
	}

	req2, err := http.NewRequest("POST", etempmailBaseURL+"/getEmailAddress", nil)
	if err != nil {
		return nil, err
	}
	etempmailSetHeaders(req2)
	req2.Header.Set("Cookie", cookieHdr)
	resp2, err := HTTPClient().Do(req2)
	if err != nil {
		return nil, err
	}
	defer resp2.Body.Close()
	if err := CheckHTTPStatus(resp2, "etempmail getEmailAddress"); err != nil {
		return nil, err
	}
	cookieHdr = moaktMergeCookies(cookieHdr, resp2.Cookies())
	raw, err := io.ReadAll(resp2.Body)
	if err != nil {
		return nil, err
	}
	var data struct {
		Address      string `json:"address"`
		CreationTime string `json:"creation_time"`
	}
	if err := json.Unmarshal(raw, &data); err != nil {
		return nil, fmt.Errorf("etempmail: invalid JSON: %w", err)
	}
	if data.Address == "" {
		return nil, fmt.Errorf("etempmail: no address in response")
	}
	if !etempmailValidGeneratedAddress(data.Address) {
		return nil, fmt.Errorf("etempmail: invalid address in response")
	}
	mb := &CreatedMailbox{Channel: "etempmail", Email: data.Address, Token: cookieHdr}
	if sec, err := strconv.ParseInt(data.CreationTime, 10, 64); err == nil && sec > 0 {
		mb.CreatedAt = time.Unix(sec, 0).UTC().Format(time.RFC3339)
	}
	return mb, nil
}

// EtempmailGetEmails POST /getInbox，收件箱由会话 Cookie 绑定。
func EtempmailGetEmails(email string, cookie string) ([]NormEmail, error) {
	if cookie == "" {
		return nil, fmt.Errorf("etempmail: cookie token required")
	}
	req, err := http.NewRequest("POST", etempmailBaseURL+"/getInbox", nil)
	if err != nil {
		return nil, err
	}
	etempmailSetHeaders(req)
	req.Header.Set("Cookie", cookie)
	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if err := CheckHTTPStatus(resp, "etempmail getInbox"); err != nil {
		return nil, err
	}
	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	var rows []map[string]interface{}
	if err := json.Unmarshal(raw, &rows); err != nil {
		return nil, err
	}
	emails := make([]NormEmail, 0, len(rows))
	for i, row := range rows {
		from, _ := row["from"].(string)
		subj, _ := row["subject"].(string)
		dt, _ := row["date"].(string)
		flat := map[string]interface{}{
			"id":      fmt.Sprintf("%s\n%s\n%s\n%d\n%s", from, subj, dt, i, email),
			"from":    from,
			"to":      email,
			"subject": subj,
			"body":    row["body"],
			"date":    dt,
			"isRead":  false,
		}
		emails = append(emails, NormalizeMap(flat, email))
	}
	return emails, nil
}
