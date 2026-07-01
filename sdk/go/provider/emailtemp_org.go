package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"net/url"
	"regexp"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

/* emailtemp.org 临时邮箱服务商（与 tempmailten.com 共享 Laravel 模板）
 * 流程：GET /en 获取 session cookie + CSRF meta token
 *       POST /messages（body: _token={csrf}&captcha=）获取邮箱地址和邮件列表
 *       GET /view/{id} 获取单封邮件 HTML 正文
 * token 存储 CSRF token 值，session 由 tls-client 的 cookie jar 自动维护
 */

const emailtempOrgBaseURL = "https://emailtemp.org"

/* emailtempOrgCSRFRe 从 meta 标签提取 CSRF token */
var emailtempOrgCSRFRe = regexp.MustCompile(`<meta\s+name="csrf-token"\s+content="([^"]+)"`)

/* emailtempOrgBrowserHeaders 设置浏览器模拟请求头 */
func emailtempOrgBrowserHeaders(req *http.Request) {
	req.Header.Set("User-Agent", getCurrentUA())
	req.Header.Set("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8")
	req.Header.Set("Accept-Language", "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7")
}

/* emailtempOrgAjaxHeaders 设置 AJAX 请求头 */
func emailtempOrgAjaxHeaders(req *http.Request) {
	req.Header.Set("User-Agent", getCurrentUA())
	req.Header.Set("Accept", "application/json, text/javascript, */*; q=0.01")
	req.Header.Set("Accept-Language", "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7")
	req.Header.Set("X-Requested-With", "XMLHttpRequest")
	req.Header.Set("Referer", emailtempOrgBaseURL+"/en")
}

/* EmailtempOrgGenerate 创建 emailtemp.org 临时邮箱 */
func EmailtempOrgGenerate() (*CreatedMailbox, error) {
	client := HTTPClient()

	req, err := http.NewRequest("GET", emailtempOrgBaseURL+"/en", nil)
	if err != nil {
		return nil, fmt.Errorf("emailtemp-org: 创建首页请求失败: %w", err)
	}
	emailtempOrgBrowserHeaders(req)

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("emailtemp-org: 获取首页失败: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("emailtemp-org: 读取首页响应失败: %w", err)
	}

	if err := CheckHTTPStatus(resp, "emailtemp-org home"); err != nil {
		return nil, err
	}

	m := emailtempOrgCSRFRe.FindSubmatch(body)
	if m == nil {
		return nil, fmt.Errorf("emailtemp-org: 未能从首页提取 CSRF token")
	}
	csrfToken := string(m[1])

	form := url.Values{}
	form.Set("_token", csrfToken)
	form.Set("captcha", "")

	req2, err := http.NewRequest("POST", emailtempOrgBaseURL+"/messages", strings.NewReader(form.Encode()))
	if err != nil {
		return nil, fmt.Errorf("emailtemp-org: 创建消息请求失败: %w", err)
	}
	emailtempOrgAjaxHeaders(req2)
	req2.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	req2.Header.Set("X-CSRF-TOKEN", csrfToken)

	resp2, err := client.Do(req2)
	if err != nil {
		return nil, fmt.Errorf("emailtemp-org: 获取邮箱失败: %w", err)
	}
	defer resp2.Body.Close()

	body2, err := io.ReadAll(resp2.Body)
	if err != nil {
		return nil, fmt.Errorf("emailtemp-org: 读取邮箱响应失败: %w", err)
	}

	if err := CheckHTTPStatus(resp2, "emailtemp-org messages"); err != nil {
		return nil, err
	}

	var msgResp struct {
		Mailbox  string `json:"mailbox"`
		Messages []struct {
			ID        json.Number `json:"id"`
			From      string      `json:"from"`
			FromEmail string      `json:"from_email"`
			Subject   string      `json:"subject"`
			IsSeen    bool        `json:"is_seen"`
		} `json:"messages"`
	}
	if err := json.Unmarshal(body2, &msgResp); err != nil {
		return nil, fmt.Errorf("emailtemp-org: 解析响应失败: %w", err)
	}

	emailAddr := strings.TrimSpace(msgResp.Mailbox)
	if emailAddr == "" || !strings.Contains(emailAddr, "@") {
		return nil, fmt.Errorf("emailtemp-org: 邮箱地址无效: %q", emailAddr)
	}

	return &CreatedMailbox{
		Channel: "emailtemp-org",
		Email:   emailAddr,
		Token:   csrfToken,
	}, nil
}

/* EmailtempOrgGetEmails 获取 emailtemp.org 邮件列表 */
func EmailtempOrgGetEmails(email, token string) ([]NormEmail, error) {
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("emailtemp-org: 邮箱地址为空")
	}
	csrfToken := strings.TrimSpace(token)
	if csrfToken == "" {
		return nil, fmt.Errorf("emailtemp-org: 缺少 CSRF token")
	}

	client := HTTPClient()

	form := url.Values{}
	form.Set("_token", csrfToken)
	form.Set("captcha", "")

	req, err := http.NewRequest("POST", emailtempOrgBaseURL+"/messages", strings.NewReader(form.Encode()))
	if err != nil {
		return nil, fmt.Errorf("emailtemp-org: 创建消息请求失败: %w", err)
	}
	emailtempOrgAjaxHeaders(req)
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	req.Header.Set("X-CSRF-TOKEN", csrfToken)

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("emailtemp-org: 获取邮件列表失败: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("emailtemp-org: 读取邮件列表响应失败: %w", err)
	}

	if err := CheckHTTPStatus(resp, "emailtemp-org messages"); err != nil {
		return nil, err
	}

	var msgResp struct {
		Messages []struct {
			ID        json.Number `json:"id"`
			From      string      `json:"from"`
			FromEmail string      `json:"from_email"`
			Subject   string      `json:"subject"`
			IsSeen    bool        `json:"is_seen"`
		} `json:"messages"`
	}
	if err := json.Unmarshal(body, &msgResp); err != nil {
		return nil, fmt.Errorf("emailtemp-org: 解析邮件列表失败: %w", err)
	}

	if len(msgResp.Messages) == 0 {
		return []NormEmail{}, nil
	}

	emails := make([]NormEmail, 0, len(msgResp.Messages))
	for _, msg := range msgResp.Messages {
		id := msg.ID.String()

		fromAddr := msg.FromEmail
		if msg.From != "" && msg.From != fromAddr {
			fromAddr = fmt.Sprintf("%s <%s>", msg.From, msg.FromEmail)
		}

		htmlBody := emailtempOrgFetchView(client, id)

		flat := map[string]interface{}{
			"id":      id,
			"from":    fromAddr,
			"to":      email,
			"subject": msg.Subject,
			"html":    htmlBody,
			"isRead":  msg.IsSeen,
		}
		emails = append(emails, NormalizeMap(flat, email))
	}

	return emails, nil
}

/* emailtempOrgFetchView 获取单封邮件 HTML 正文 */
func emailtempOrgFetchView(client interface {
	Do(*http.Request) (*http.Response, error)
}, id string) string {
	if id == "" {
		return ""
	}

	req, err := http.NewRequest("GET", emailtempOrgBaseURL+"/view/"+url.PathEscape(id), nil)
	if err != nil {
		return ""
	}
	emailtempOrgBrowserHeaders(req)
	req.Header.Set("Referer", emailtempOrgBaseURL+"/en")

	resp, err := client.Do(req)
	if err != nil {
		return ""
	}
	defer resp.Body.Close()

	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		io.Copy(io.Discard, resp.Body)
		return ""
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return ""
	}

	return string(body)
}
