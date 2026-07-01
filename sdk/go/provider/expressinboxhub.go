package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"regexp"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

/* expressinboxhub.com：GET 首页提取 CSRF token，POST /messages 创建邮箱并获取邮件 */

const expressinboxhubBase = "https://expressinboxhub.com"

var expressinboxhubCsrfRe = regexp.MustCompile(`<meta\s+name="csrf-token"\s+content="([^"]+)"`)

/* expressinboxhubBrowserHeaders 设置浏览器模拟请求头 */
func expressinboxhubBrowserHeaders(req *http.Request) {
	req.Header.Set("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
	req.Header.Set("Cache-Control", "no-cache")
	req.Header.Set("DNT", "1")
	req.Header.Set("Pragma", "no-cache")
	req.Header.Set("Upgrade-Insecure-Requests", "1")
	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36")
	req.Header.Set("sec-ch-ua", `"Chromium";v="146", "Not-A.Brand";v="24", "Google Chrome";v="146"`)
	req.Header.Set("sec-ch-ua-mobile", "?0")
	req.Header.Set("sec-ch-ua-platform", `"Windows"`)
}

/* expressinboxhubAjaxHeaders 设置 AJAX POST 请求头 */
func expressinboxhubAjaxHeaders(req *http.Request) {
	req.Header.Set("Accept", "application/json, text/plain, */*")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded; charset=UTF-8")
	req.Header.Set("Origin", expressinboxhubBase)
	req.Header.Set("Referer", expressinboxhubBase+"/")
	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36")
	req.Header.Set("X-Requested-With", "XMLHttpRequest")
	req.Header.Set("sec-ch-ua", `"Chromium";v="146", "Not-A.Brand";v="24", "Google Chrome";v="146"`)
	req.Header.Set("sec-ch-ua-mobile", "?0")
	req.Header.Set("sec-ch-ua-platform", `"Windows"`)
	req.Header.Set("sec-fetch-dest", "empty")
	req.Header.Set("sec-fetch-mode", "cors")
	req.Header.Set("sec-fetch-site", "same-origin")
}

/* expressinboxhubExtractCSRF 从 HTML 中提取 CSRF token */
func expressinboxhubExtractCSRF(html string) (string, error) {
	if m := expressinboxhubCsrfRe.FindStringSubmatch(html); len(m) > 1 {
		return m[1], nil
	}
	return "", fmt.Errorf("expressinboxhub: 未找到 CSRF token")
}

/* expressinboxhubMergeCookies 合并响应中的 Set-Cookie 为 Cookie 字符串 */
func expressinboxhubMergeCookies(existing string, cookies []*http.Cookie) string {
	parts := []string{}
	if existing != "" {
		parts = append(parts, existing)
	}
	for _, c := range cookies {
		parts = append(parts, c.Name+"="+c.Value)
	}
	return strings.Join(parts, "; ")
}

/*
 * ExpressinboxhubGenerate 创建 expressinboxhub 临时邮箱
 * 流程: GET 首页获取 CSRF token 和 session cookies → POST /messages 创建邮箱
 */
func ExpressinboxhubGenerate() (*CreatedMailbox, error) {
	/* 第一步: GET 首页，提取 CSRF token 并收集 session cookies */
	getReq, err := http.NewRequest("GET", expressinboxhubBase, nil)
	if err != nil {
		return nil, err
	}
	expressinboxhubBrowserHeaders(getReq)

	getResp, err := HTTPClient().Do(getReq)
	if err != nil {
		return nil, err
	}
	defer getResp.Body.Close()
	if err := CheckHTTPStatus(getResp, "expressinboxhub session"); err != nil {
		return nil, err
	}

	cookie := expressinboxhubMergeCookies("", getResp.Cookies())
	raw, err := io.ReadAll(getResp.Body)
	if err != nil {
		return nil, err
	}
	html := string(raw)

	csrf, err := expressinboxhubExtractCSRF(html)
	if err != nil {
		return nil, err
	}

	/* 第二步: POST /messages 携带 CSRF token 创建邮箱 */
	body := "_token=" + csrf
	postReq, err := http.NewRequest("POST", expressinboxhubBase+"/messages", strings.NewReader(body))
	if err != nil {
		return nil, err
	}
	expressinboxhubAjaxHeaders(postReq)
	postReq.Header.Set("Cookie", cookie)
	postReq.Header.Set("X-CSRF-TOKEN", csrf)

	postResp, err := HTTPClient().Do(postReq)
	if err != nil {
		return nil, err
	}
	defer postResp.Body.Close()
	if err := CheckHTTPStatus(postResp, "expressinboxhub generate"); err != nil {
		return nil, err
	}

	/* 合并新的 session cookies */
	cookie = expressinboxhubMergeCookies(cookie, postResp.Cookies())

	postRaw, err := io.ReadAll(postResp.Body)
	if err != nil {
		return nil, err
	}

	var result struct {
		Mailbox  string `json:"mailbox"`
		Messages []any  `json:"messages"`
	}
	if err := json.Unmarshal(postRaw, &result); err != nil {
		return nil, fmt.Errorf("expressinboxhub: 响应解析失败: %s", string(postRaw[:min(len(postRaw), 120)]))
	}

	emailAddr := strings.TrimSpace(result.Mailbox)
	if emailAddr == "" {
		return nil, fmt.Errorf("expressinboxhub: 响应中未包含邮箱地址")
	}

	/* token 中编码 cookie 和 CSRF，后续获取邮件时需要 */
	token := cookie + "\n" + csrf

	return &CreatedMailbox{
		Channel: "expressinboxhub",
		Email:   emailAddr,
		Token:   token,
	}, nil
}

/*
 * ExpressinboxhubGetEmails 获取 expressinboxhub 邮件列表
 * token 格式: "cookie\ncsrf"
 */
func ExpressinboxhubGetEmails(email string, token string) ([]NormEmail, error) {
	parts := strings.SplitN(token, "\n", 2)
	if len(parts) != 2 || parts[0] == "" || parts[1] == "" {
		return nil, fmt.Errorf("expressinboxhub: 无效的 session token")
	}
	cookie := parts[0]
	csrf := parts[1]

	body := "_token=" + csrf
	req, err := http.NewRequest("POST", expressinboxhubBase+"/messages", strings.NewReader(body))
	if err != nil {
		return nil, err
	}
	expressinboxhubAjaxHeaders(req)
	req.Header.Set("Cookie", cookie)
	req.Header.Set("X-CSRF-TOKEN", csrf)

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if err := CheckHTTPStatus(resp, "expressinboxhub getEmails"); err != nil {
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
		return nil, fmt.Errorf("expressinboxhub: 返回非 JSON: %s", string(rawBytes[:min(len(rawBytes), 120)]))
	}

	if data.Messages == nil || len(data.Messages) == 0 {
		return []NormEmail{}, nil
	}

	emails := make([]NormEmail, 0, len(data.Messages))
	for i, msg := range data.Messages {
		id := ""
		if v, ok := msg["id"]; ok {
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

		htmlContent, _ := msg["content"].(string)
		text, _ := msg["text"].(string)
		if text == "" {
			text, _ = msg["body"].(string)
		}

		date, _ := msg["receivedAt"].(string)
		if date == "" {
			date, _ = msg["date"].(string)
		}
		if date == "" {
			date, _ = msg["created_at"].(string)
		}

		flat := map[string]interface{}{
			"id":          id,
			"from":        from,
			"to":          to,
			"subject":     subject,
			"text":        text,
			"html":        htmlContent,
			"date":        date,
			"isRead":      false,
			"attachments": []any{},
		}
		emails = append(emails, NormalizeMap(flat, email))
	}
	return emails, nil
}
