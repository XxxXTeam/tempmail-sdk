package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"math/rand"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

/**
 * Anonymmail — https://anonymmail.net
 * POST JSON API，需要 cookie session
 * 创建邮箱: HEAD https://anonymmail.net/ 获取 cookie session，然后 POST /api/create
 * 获取邮件: POST /api/get
 */

const anonymmailBase = "https://anonymmail.net"

/* anonymmailRandomLocal 生成随机用户名 */
func anonymmailRandomLocal(n int) string {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	b := make([]byte, n)
	for i := range b {
		b[i] = chars[rand.Intn(len(chars))]
	}
	return string(b)
}

/* anonymmailDefaultHeaders 设置通用请求头 */
func anonymmailDefaultHeaders(req *http.Request) {
	req.Header.Set("Accept", "*/*")
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	req.Header.Set("Referer", "https://anonymmail.net/")
	req.Header.Set("User-Agent", getCurrentUA())
}

/* anonymmailFetchDomains 获取可用域名列表 */
func anonymmailFetchDomains() ([]string, error) {
	req, err := http.NewRequest("POST", anonymmailBase+"/api/getDomains", nil)
	if err != nil {
		return nil, err
	}
	anonymmailDefaultHeaders(req)

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
		return nil, fmt.Errorf("anonymmail domains: %d", resp.StatusCode)
	}

	var domainList []struct {
		Domain string `json:"domain"`
	}
	if err := json.Unmarshal(body, &domainList); err != nil {
		return nil, err
	}

	var domains []string
	for _, d := range domainList {
		s := strings.TrimSpace(d.Domain)
		if s != "" {
			domains = append(domains, s)
		}
	}
	if len(domains) == 0 {
		return nil, fmt.Errorf("anonymmail: no domains available")
	}
	return domains, nil
}

/*
 * AnonymmailGenerate 创建 anonymmail.net 临时邮箱
 * 1. HEAD https://anonymmail.net/ 获取 session cookie
 * 2. 获取可用域名
 * 3. POST /api/create 创建邮箱
 */
func AnonymmailGenerate(channel ...string) (*CreatedMailbox, error) {
	client := HTTPClient()

	/* 步骤 1: HEAD 请求获取 session cookie */
	headReq, err := http.NewRequest("HEAD", anonymmailBase+"/", nil)
	if err != nil {
		return nil, err
	}
	headReq.Header.Set("User-Agent", getCurrentUA())
	headResp, err := client.Do(headReq)
	if err != nil {
		return nil, fmt.Errorf("anonymmail: HEAD failed: %w", err)
	}
	headResp.Body.Close()

	/* 步骤 2: 获取可用域名 */
	domains, err := anonymmailFetchDomains()
	if err != nil {
		return nil, err
	}
	domain := domains[rand.Intn(len(domains))]

	/* 步骤 3: POST /api/create 创建邮箱 */
	user := anonymmailRandomLocal(10)
	email := user + "@" + domain
	createBody := "email=" + email

	createReq, err := http.NewRequest("POST", anonymmailBase+"/api/create", strings.NewReader(createBody))
	if err != nil {
		return nil, err
	}
	anonymmailDefaultHeaders(createReq)

	createResp, err := client.Do(createReq)
	if err != nil {
		return nil, err
	}
	defer createResp.Body.Close()

	respBody, err := io.ReadAll(createResp.Body)
	if err != nil {
		return nil, err
	}
	if createResp.StatusCode < 200 || createResp.StatusCode >= 300 {
		return nil, fmt.Errorf("anonymmail create: %d", createResp.StatusCode)
	}

	var createResult struct {
		Success bool   `json:"success"`
		Email   string `json:"email"`
	}
	if err := json.Unmarshal(respBody, &createResult); err != nil {
		return nil, err
	}
	if !createResult.Success {
		return nil, fmt.Errorf("anonymmail: create failed")
	}
	resultEmail := strings.TrimSpace(createResult.Email)
	if resultEmail == "" {
		resultEmail = email
	}

	ch := "anonymmail"
	if len(channel) > 0 && channel[0] != "" {
		ch = channel[0]
	}
	return &CreatedMailbox{Channel: ch, Email: resultEmail, Token: ""}, nil
}

/*
 * AnonymmailGetEmails 获取 anonymmail.net 邮件列表
 * POST /api/get body: email={email}
 * 响应: {"email@domain":{"created_at":"...","emails":[...]}}
 */
func AnonymmailGetEmails(email string) ([]NormEmail, error) {
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("anonymmail: empty email")
	}

	reqBody := "email=" + email
	req, err := http.NewRequest("POST", anonymmailBase+"/api/get", strings.NewReader(reqBody))
	if err != nil {
		return nil, err
	}
	anonymmailDefaultHeaders(req)

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
		return nil, fmt.Errorf("anonymmail get: %d", resp.StatusCode)
	}

	/*
	 * 响应格式: {"email@domain": {"created_at":"...", "emails":[...]}}
	 * 需要从外层 map 中提取对应邮箱的邮件列表
	 */
	var outer map[string]json.RawMessage
	if err := json.Unmarshal(body, &outer); err != nil {
		return nil, err
	}

	/* 查找邮箱对应的数据 */
	var inboxRaw json.RawMessage
	found := false
	for key, val := range outer {
		if strings.EqualFold(key, email) {
			inboxRaw = val
			found = true
			break
		}
	}
	if !found {
		/* 如果找不到精确匹配，取第一个 */
		for _, val := range outer {
			inboxRaw = val
			found = true
			break
		}
	}
	if !found {
		return []NormEmail{}, nil
	}

	var inbox struct {
		Emails []map[string]interface{} `json:"emails"`
	}
	if err := json.Unmarshal(inboxRaw, &inbox); err != nil {
		return nil, err
	}

	if len(inbox.Emails) == 0 {
		return []NormEmail{}, nil
	}

	/* 将 "body" 字段映射为标准化字段 */
	for _, m := range inbox.Emails {
		if htmlBody, ok := m["body"].(string); ok {
			if _, hasHTML := m["html"]; !hasHTML {
				m["html"] = htmlBody
			}
		}
		/* token 字段映射为 id */
		if token, ok := m["token"].(string); ok {
			if _, hasID := m["id"]; !hasID {
				m["id"] = token
			}
		}
	}

	return normEmailsFromMaps(inbox.Emails, email), nil
}
