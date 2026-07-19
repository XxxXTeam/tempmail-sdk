package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"net/url"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

// minuteinbox.com：基于 PHP session 的临时邮箱服务，域名 minafter.com

const minuteinboxBaseURL = "https://www.minuteinbox.com"

// minuteinboxSession 保存 PHPSESSID 和 CSRF token，序列化为 JSON 作为 token 存储
type minuteinboxSession struct {
	PHPSESSID string `json:"phpsessid"`
	CSRF      string `json:"csrf"`
}

// minuteinboxExtractCSRF 从 HTML 中提取 CSRF token（格式: const CSRF="xxx"）
func minuteinboxExtractCSRF(html string) string {
	marker := `CSRF="`
	idx := strings.Index(html, marker)
	if idx == -1 {
		return ""
	}
	sub := html[idx+len(marker):]
	endIdx := strings.Index(sub, `"`)
	if endIdx == -1 {
		return ""
	}
	return sub[:endIdx]
}

// minuteinboxEncodeSession 将 session 序列化为 JSON 字符串作为 token
func minuteinboxEncodeSession(sess minuteinboxSession) string {
	raw, _ := json.Marshal(sess)
	return string(raw)
}

// minuteinboxDecodeSession 从 JSON token 反序列化 session
func minuteinboxDecodeSession(token string) (minuteinboxSession, error) {
	var sess minuteinboxSession
	if err := json.Unmarshal([]byte(token), &sess); err != nil {
		return sess, fmt.Errorf("minuteinbox: 无效的 session token: %w", err)
	}
	if sess.PHPSESSID == "" || sess.CSRF == "" {
		return sess, fmt.Errorf("minuteinbox: session token 缺少必要字段")
	}
	return sess, nil
}

/*
 * MinuteinboxGenerate 创建 minuteinbox.com 临时邮箱
 * 流程:
 *   1. GET / 获取 PHPSESSID cookie 和 CSRF token（从 HTML 中提取 const CSRF="xxx"）
 *   2. GET /index/index?csrf_token={csrf} 创建邮箱，返回 {"email":"user@minafter.com"}
 * token: JSON {"phpsessid":"...","csrf":"..."}
 * duration/domain 参数保留用于接口一致性，当前未使用
 */
func MinuteinboxGenerate(duration int, domain string) (*CreatedMailbox, error) {
	client := HTTPClient()

	// 步骤 1：GET / 获取 PHPSESSID 和 CSRF token
	req, err := http.NewRequest("GET", minuteinboxBaseURL+"/", nil)
	if err != nil {
		return nil, fmt.Errorf("minuteinbox: 创建首页请求失败: %w", err)
	}
	req.Header.Set("User-Agent", getCurrentUA())
	req.Header.Set("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8")

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("minuteinbox: 获取首页失败: %w", err)
	}
	defer resp.Body.Close()

	if err := CheckHTTPStatus(resp, "minuteinbox home"); err != nil {
		return nil, err
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("minuteinbox: 读取首页响应失败: %w", err)
	}

	// 提取 CSRF token
	csrf := minuteinboxExtractCSRF(string(body))
	if csrf == "" {
		return nil, fmt.Errorf("minuteinbox: 未能从首页提取 CSRF token")
	}

	// 提取 PHPSESSID cookie
	var phpsessid string
	for _, c := range resp.Cookies() {
		if c.Name == "PHPSESSID" {
			phpsessid = c.Value
			break
		}
	}
	if phpsessid == "" {
		return nil, fmt.Errorf("minuteinbox: 未获取到 PHPSESSID cookie")
	}

	// 步骤 2：GET /index/index?csrf_token={csrf} 创建邮箱
	createURL := fmt.Sprintf("%s/index/index?csrf_token=%s", minuteinboxBaseURL, url.QueryEscape(csrf))
	req2, err := http.NewRequest("GET", createURL, nil)
	if err != nil {
		return nil, fmt.Errorf("minuteinbox: 创建邮箱请求失败: %w", err)
	}
	req2.Header.Set("User-Agent", getCurrentUA())
	req2.Header.Set("X-Requested-With", "XMLHttpRequest")
	req2.Header.Set("Cookie", "PHPSESSID="+phpsessid)

	resp2, err := client.Do(req2)
	if err != nil {
		return nil, fmt.Errorf("minuteinbox: 创建邮箱请求失败: %w", err)
	}
	defer resp2.Body.Close()

	if err := CheckHTTPStatus(resp2, "minuteinbox create"); err != nil {
		return nil, err
	}

	body2, err := io.ReadAll(resp2.Body)
	if err != nil {
		return nil, fmt.Errorf("minuteinbox: 读取创建邮箱响应失败: %w", err)
	}

	// 解析 JSON 响应 {"email":"user@minafter.com"}
	var createResp struct {
		Email string `json:"email"`
	}
	if err := json.Unmarshal(body2, &createResp); err != nil {
		return nil, fmt.Errorf("minuteinbox: 解析创建邮箱响应失败: %w", err)
	}

	emailAddr := strings.TrimSpace(createResp.Email)
	if emailAddr == "" || !strings.Contains(emailAddr, "@") {
		return nil, fmt.Errorf("minuteinbox: 获取到的邮箱地址无效: %q", emailAddr)
	}

	sess := minuteinboxSession{PHPSESSID: phpsessid, CSRF: csrf}
	return &CreatedMailbox{
		Channel: "minuteinbox",
		Email:   emailAddr,
		Token:   minuteinboxEncodeSession(sess),
	}, nil
}

/*
 * MinuteinboxGetEmails 获取 minuteinbox.com 邮件列表
 * 流程:
 *   1. GET /index/refresh 获取邮件列表 JSON 数组
 *   2. 对每封邮件 POST /index/email (body: id=X) 获取详情
 * 字段说明（捷克语）: predmet=subject, od=from, id=邮件ID, kdy=when, precteno=read status
 */
func MinuteinboxGetEmails(token, email string) ([]NormEmail, error) {
	sess, err := minuteinboxDecodeSession(token)
	if err != nil {
		return nil, err
	}

	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("minuteinbox: 邮箱地址为空")
	}

	client := HTTPClient()
	cookieHeader := "PHPSESSID=" + sess.PHPSESSID

	// 步骤 1：GET /index/refresh 获取邮件列表
	req, err := http.NewRequest("GET", minuteinboxBaseURL+"/index/refresh", nil)
	if err != nil {
		return nil, fmt.Errorf("minuteinbox: 创建刷新请求失败: %w", err)
	}
	req.Header.Set("User-Agent", getCurrentUA())
	req.Header.Set("X-Requested-With", "XMLHttpRequest")
	req.Header.Set("Cookie", cookieHeader)

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("minuteinbox: 获取邮件列表失败: %w", err)
	}
	defer resp.Body.Close()

	if err := CheckHTTPStatus(resp, "minuteinbox refresh"); err != nil {
		return nil, err
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("minuteinbox: 读取邮件列表响应失败: %w", err)
	}

	// 空收件箱可能返回数字 0 或空数组
	trimmed := strings.TrimSpace(string(body))
	if trimmed == "0" || trimmed == "" || trimmed == "[]" {
		return []NormEmail{}, nil
	}

	// 解析邮件列表 JSON
	var mailList []struct {
		Predmet  string      `json:"predmet"`  // 邮件主题
		Od       string      `json:"od"`       // 发件人
		ID       json.Number `json:"id"`       // 邮件 ID
		Kdy      string      `json:"kdy"`      // 时间
		Precteno string      `json:"precteno"` // 已读状态: "new" 或 "precteno"
	}
	if err := json.Unmarshal(body, &mailList); err != nil {
		return nil, fmt.Errorf("minuteinbox: 解析邮件列表失败: %w", err)
	}

	if len(mailList) == 0 {
		return []NormEmail{}, nil
	}

	// 步骤 2：对每封邮件获取详情
	emails := make([]NormEmail, 0, len(mailList))
	for _, item := range mailList {
		mailID := item.ID.String()
		detail := minuteinboxFetchDetail(client, cookieHeader, mailID)

		isRead := item.Precteno != "new"

		htmlBody := ""
		from := item.Od
		subject := item.Predmet
		to := email
		if detail != nil {
			htmlBody = detail.Body
			if detail.From != "" {
				from = detail.From
			}
			if detail.Subject != "" {
				subject = detail.Subject
			}
			if detail.To != "" {
				to = detail.To
			}
		}

		flat := map[string]interface{}{
			"id":      mailID,
			"from":    from,
			"to":      to,
			"subject": subject,
			"html":    htmlBody,
			"date":    item.Kdy,
			"isRead":  isRead,
		}
		emails = append(emails, NormalizeMap(flat, email))
	}

	return emails, nil
}

// minuteinboxDetail 邮件详情响应结构
type minuteinboxDetail struct {
	Subject string `json:"predmet"`
	From    string `json:"od"`
	ID      string `json:"id"`
	To      string `json:"komu"`
	Body    string `json:"telo"`
}

// minuteinboxFetchDetail 通过 POST /index/email 获取单封邮件详情
func minuteinboxFetchDetail(client interface {
	Do(*http.Request) (*http.Response, error)
}, cookieHeader, mailID string) *minuteinboxDetail {
	if mailID == "" {
		return nil
	}

	formData := "id=" + url.QueryEscape(mailID)
	req, err := http.NewRequest("POST", minuteinboxBaseURL+"/index/email", strings.NewReader(formData))
	if err != nil {
		return nil
	}
	req.Header.Set("User-Agent", getCurrentUA())
	req.Header.Set("X-Requested-With", "XMLHttpRequest")
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	req.Header.Set("Cookie", cookieHeader)

	resp, err := client.Do(req)
	if err != nil {
		return nil
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil
	}

	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil
	}

	var detail minuteinboxDetail
	if err := json.Unmarshal(body, &detail); err != nil {
		return nil
	}
	return &detail
}
