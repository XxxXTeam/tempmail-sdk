package provider

import (
	"encoding/base64"
	"encoding/json"
	"fmt"
	"io"
	"regexp"
	"strings"
	"time"

	http "github.com/bogdanfinn/fhttp"
)

// email10min.com：GET /zh 拿 CSRF + Cookie，POST /messages 获取邮件。

const email10minBase = "https://email10min.com"

var (
	email10minCsrfMetaRe   = regexp.MustCompile(`<meta\s+name="csrf-token"\s+content="([^"]+)"`)
	email10minCsrfInputRe  = regexp.MustCompile(`<input[^>]+name="_token"[^>]+value="([^"]+)"`)
	email10minEmailIdRe    = regexp.MustCompile(`id="emailAddress"[^>]*>([^<]+)`)
	email10minEmailClsRe   = regexp.MustCompile(`class="[^"]*email[^"]*"[^>]*>([^<]*@[^<]+)`)
	email10minDataEmailRe  = regexp.MustCompile(`data-email="([^"]+@[^"]+)"`)
	email10minValueEmailRe = regexp.MustCompile(`value="([a-zA-Z0-9._%+\-]+@[a-zA-Z0-9.\-]+\.[a-zA-Z]{2,})"`)
	email10minJSONEmailRe  = regexp.MustCompile(`"mailbox"\s*:\s*"([^"]+@[^"]+)"`)
	email10minGenericRe    = regexp.MustCompile(`([a-zA-Z0-9._%+\-]+@[a-zA-Z0-9.\-]+\.[a-zA-Z]{2,})`)
)

func email10minBrowserHeaders(req *http.Request) {
	req.Header.Set("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6")
	req.Header.Set("Cache-Control", "no-cache")
	req.Header.Set("DNT", "1")
	req.Header.Set("Pragma", "no-cache")
	req.Header.Set("Upgrade-Insecure-Requests", "1")
	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0")
	req.Header.Set("sec-ch-ua", `"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"`)
	req.Header.Set("sec-ch-ua-mobile", "?0")
	req.Header.Set("sec-ch-ua-platform", `"Windows"`)
}

func email10minAjaxHeaders(req *http.Request) {
	req.Header.Set("Accept", "application/json, text/plain, */*")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6")
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded; charset=UTF-8")
	req.Header.Set("Origin", email10minBase)
	req.Header.Set("Referer", email10minBase+"/zh")
	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0")
	req.Header.Set("X-Requested-With", "XMLHttpRequest")
	req.Header.Set("sec-ch-ua", `"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"`)
	req.Header.Set("sec-ch-ua-mobile", "?0")
	req.Header.Set("sec-ch-ua-platform", `"Windows"`)
	req.Header.Set("sec-fetch-dest", "empty")
	req.Header.Set("sec-fetch-mode", "cors")
	req.Header.Set("sec-fetch-site", "same-origin")
}

type email10minTokenData struct {
	Cookie string `json:"c"`
	CSRF   string `json:"t"`
}

const email10minTokPrefix = "e10m:"

func email10minEncodeToken(cookie, csrf string) string {
	raw, _ := json.Marshal(email10minTokenData{Cookie: cookie, CSRF: csrf})
	return email10minTokPrefix + base64.StdEncoding.EncodeToString(raw)
}

func email10minDecodeToken(token string) (cookie, csrf string, err error) {
	if !strings.HasPrefix(token, email10minTokPrefix) {
		return "", "", fmt.Errorf("email10min: invalid session token")
	}
	decoded, err := base64.StdEncoding.DecodeString(token[len(email10minTokPrefix):])
	if err != nil {
		return "", "", fmt.Errorf("email10min: invalid session token: %w", err)
	}
	var data email10minTokenData
	if err := json.Unmarshal(decoded, &data); err != nil {
		return "", "", fmt.Errorf("email10min: invalid session token: %w", err)
	}
	if data.Cookie == "" || data.CSRF == "" {
		return "", "", fmt.Errorf("email10min: invalid session token (empty fields)")
	}
	return data.Cookie, data.CSRF, nil
}

func email10minExtractCSRF(html string) (string, error) {
	if m := email10minCsrfMetaRe.FindStringSubmatch(html); len(m) > 1 {
		return m[1], nil
	}
	if m := email10minCsrfInputRe.FindStringSubmatch(html); len(m) > 1 {
		return m[1], nil
	}
	return "", fmt.Errorf("email10min: 未找到 CSRF token")
}

func email10minExtractEmail(html string) (string, error) {
	if m := email10minEmailIdRe.FindStringSubmatch(html); len(m) > 1 && strings.Contains(m[1], "@") {
		return strings.TrimSpace(m[1]), nil
	}
	if m := email10minEmailClsRe.FindStringSubmatch(html); len(m) > 1 {
		return strings.TrimSpace(m[1]), nil
	}
	if m := email10minDataEmailRe.FindStringSubmatch(html); len(m) > 1 {
		return strings.TrimSpace(m[1]), nil
	}
	if m := email10minValueEmailRe.FindStringSubmatch(html); len(m) > 1 {
		return strings.TrimSpace(m[1]), nil
	}
	if m := email10minJSONEmailRe.FindStringSubmatch(html); len(m) > 1 {
		return strings.TrimSpace(m[1]), nil
	}
	if m := email10minGenericRe.FindStringSubmatch(html); len(m) > 1 {
		addr := m[1]
		if !strings.Contains(addr, "email10min") && !strings.Contains(addr, "example") && !strings.Contains(addr, "google") {
			return strings.TrimSpace(addr), nil
		}
	}
	return "", fmt.Errorf("email10min: 未找到邮箱地址")
}

// Email10minGenerate 创建 email10min 临时邮箱
func Email10minGenerate() (*CreatedMailbox, error) {
	req, err := http.NewRequest("GET", email10minBase+"/zh", nil)
	if err != nil {
		return nil, err
	}
	email10minBrowserHeaders(req)

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if err := CheckHTTPStatus(resp, "email10min session"); err != nil {
		return nil, err
	}

	cookie := moaktMergeCookies("", resp.Cookies())
	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	html := string(raw)

	csrf, err := email10minExtractCSRF(html)
	if err != nil {
		return nil, err
	}
	emailAddr, err := email10minExtractEmail(html)
	if err != nil {
		return nil, err
	}

	token := email10minEncodeToken(cookie, csrf)

	return &CreatedMailbox{
		Channel: "email10min",
		Email:   emailAddr,
		Token:   token,
	}, nil
}

// Email10minGetEmails 获取 email10min 邮件列表
func Email10minGetEmails(email string, token string) ([]NormEmail, error) {
	cookie, csrf, err := email10minDecodeToken(token)
	if err != nil {
		return nil, err
	}

	ts := fmt.Sprintf("%d", time.Now().UnixMilli())
	body := fmt.Sprintf("_token=%s&captcha=", csrf)
	req, err := http.NewRequest("POST", email10minBase+"/messages?"+ts, strings.NewReader(body))
	if err != nil {
		return nil, err
	}
	email10minAjaxHeaders(req)
	req.Header.Set("Cookie", cookie)

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if err := CheckHTTPStatus(resp, "email10min getEmails"); err != nil {
		return nil, err
	}

	rawBytes, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	var data struct {
		Mailbox  string                   `json:"mailbox"`
		Messages []map[string]interface{} `json:"messages"`
	}
	if err := json.Unmarshal(rawBytes, &data); err != nil {
		return nil, fmt.Errorf("email10min: 返回非 JSON: %s", string(rawBytes[:min(len(rawBytes), 120)]))
	}

	if data.Messages == nil {
		return []NormEmail{}, nil
	}

	emails := make([]NormEmail, 0, len(data.Messages))
	for i, msg := range data.Messages {
		id := ""
		if v, ok := msg["id"]; ok {
			id = fmt.Sprintf("%v", v)
		} else if v, ok := msg["message_id"]; ok {
			id = fmt.Sprintf("%v", v)
		} else {
			id = fmt.Sprintf("%d", i)
		}
		from, _ := msg["from"].(string)
		if from == "" {
			if v, ok := msg["sender"]; ok {
				from = fmt.Sprintf("%v", v)
			}
		}
		to, _ := msg["to"].(string)
		if to == "" {
			to = email
		}
		subject, _ := msg["subject"].(string)
		text, _ := msg["text"].(string)
		if text == "" {
			text, _ = msg["body"].(string)
		}
		htmlContent, _ := msg["html"].(string)
		if htmlContent == "" {
			htmlContent, _ = msg["body_html"].(string)
		}
		date, _ := msg["date"].(string)
		if date == "" {
			date, _ = msg["created_at"].(string)
		}

		flat := map[string]interface{}{
			"id":      id,
			"from":    from,
			"to":      to,
			"subject": subject,
			"text":    text,
			"html":    htmlContent,
			"date":    date,
			"isRead":  false,
		}
		emails = append(emails, NormalizeMap(flat, email))
	}
	return emails, nil
}
