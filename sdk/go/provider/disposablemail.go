package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"regexp"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

/* disposablemail.com 临时邮箱服务商（METRONET s.r.o. 后端）
 * 与 fakemail.net / minuteinbox.com 共享相同的 PHP API 结构
 * 流程：GET / 获取 session cookie + CSRF token → GET /index/index?csrf_token={csrf} 创建邮箱
 *       GET /index/refresh 获取邮件列表 → POST /index/email 获取邮件 HTML 正文
 * token 存储 CSRF token 值，session 由 tls-client 的 cookie jar 自动维护
 */

const disposablemailBaseURL = "https://www.disposablemail.com"

/* disposablemailCSRFRe 从 HTML 中提取 CSRF token（格式: CSRF="xxx"） */
var disposablemailCSRFRe = regexp.MustCompile(`CSRF="([^"]+)"`)

/* disposablemailBrowserHeaders 设置浏览器模拟请求头 */
func disposablemailBrowserHeaders(req *http.Request) {
	req.Header.Set("User-Agent", getCurrentUA())
	req.Header.Set("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8")
	req.Header.Set("Accept-Language", "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7")
}

/* disposablemailAjaxHeaders 设置 AJAX 请求头 */
func disposablemailAjaxHeaders(req *http.Request) {
	req.Header.Set("User-Agent", getCurrentUA())
	req.Header.Set("Accept", "application/json, text/javascript, */*; q=0.01")
	req.Header.Set("Accept-Language", "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7")
	req.Header.Set("X-Requested-With", "XMLHttpRequest")
	req.Header.Set("Referer", disposablemailBaseURL+"/")
}

/* DisposablemailGenerate 创建 disposablemail.com 临时邮箱
 * 流程:
 *   1. GET / 获取 session cookie 和 CSRF token（正则匹配 CSRF="xxx"）
 *   2. GET /index/index?csrf_token={csrf} 创建邮箱，返回 {"email":"user@domain.com"}
 * token: CSRF token 值
 */
func DisposablemailGenerate() (*CreatedMailbox, error) {
	client := HTTPClient()

	req, err := http.NewRequest("GET", disposablemailBaseURL+"/", nil)
	if err != nil {
		return nil, fmt.Errorf("disposablemail: 创建首页请求失败: %w", err)
	}
	disposablemailBrowserHeaders(req)

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("disposablemail: 获取首页失败: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("disposablemail: 读取首页响应失败: %w", err)
	}

	if err := CheckHTTPStatus(resp, "disposablemail home"); err != nil {
		return nil, err
	}

	m := disposablemailCSRFRe.FindStringSubmatch(string(body))
	if len(m) < 2 || m[1] == "" {
		return nil, fmt.Errorf("disposablemail: 未能从首页提取 CSRF token")
	}
	csrfToken := m[1]

	createURL := fmt.Sprintf("%s/index/index?csrf_token=%s", disposablemailBaseURL, csrfToken)
	req2, err := http.NewRequest("GET", createURL, nil)
	if err != nil {
		return nil, fmt.Errorf("disposablemail: 创建邮箱请求失败: %w", err)
	}
	disposablemailAjaxHeaders(req2)

	resp2, err := client.Do(req2)
	if err != nil {
		return nil, fmt.Errorf("disposablemail: 创建邮箱失败: %w", err)
	}
	defer resp2.Body.Close()

	body2, err := io.ReadAll(resp2.Body)
	if err != nil {
		return nil, fmt.Errorf("disposablemail: 读取创建邮箱响应失败: %w", err)
	}

	if err := CheckHTTPStatus(resp2, "disposablemail create"); err != nil {
		return nil, err
	}

	var createResp struct {
		Email string `json:"email"`
	}
	if err := json.Unmarshal(body2, &createResp); err != nil {
		return nil, fmt.Errorf("disposablemail: 解析创建邮箱响应失败: %w", err)
	}

	emailAddr := strings.TrimSpace(createResp.Email)
	if emailAddr == "" || !strings.Contains(emailAddr, "@") {
		return nil, fmt.Errorf("disposablemail: 获取到的邮箱地址无效: %q", emailAddr)
	}

	return &CreatedMailbox{
		Channel: "disposablemail-com",
		Email:   emailAddr,
		Token:   csrfToken,
	}, nil
}

/* DisposablemailGetEmails 获取 disposablemail.com 邮件列表
 * 流程:
 *   1. GET /index/refresh 获取邮件列表（返回 JSON 数组，空收件箱返回数字 0）
 *   2. 对每封邮件 POST /index/email（body: id={id}）获取 HTML 正文
 * 字段说明（捷克语）: predmet=subject, od=from, id=邮件ID, kdy=when, precteno=read status
 */
func DisposablemailGetEmails(email, token string) ([]NormEmail, error) {
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("disposablemail: 邮箱地址为空")
	}

	client := HTTPClient()

	req, err := http.NewRequest("GET", disposablemailBaseURL+"/index/refresh", nil)
	if err != nil {
		return nil, fmt.Errorf("disposablemail: 创建刷新请求失败: %w", err)
	}
	disposablemailAjaxHeaders(req)

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("disposablemail: 获取邮件列表失败: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("disposablemail: 读取邮件列表响应失败: %w", err)
	}

	if err := CheckHTTPStatus(resp, "disposablemail refresh"); err != nil {
		return nil, err
	}

	trimmed := strings.TrimSpace(string(body))
	if trimmed == "0" || trimmed == "" || trimmed == "[]" {
		return []NormEmail{}, nil
	}

	var mailList []struct {
		Predmet  string      `json:"predmet"`
		Od       string      `json:"od"`
		ID       json.Number `json:"id"`
		Kdy      string      `json:"kdy"`
		Precteno string      `json:"precteno"`
	}
	if err := json.Unmarshal(body, &mailList); err != nil {
		return nil, fmt.Errorf("disposablemail: 解析邮件列表失败: %w", err)
	}

	if len(mailList) == 0 {
		return []NormEmail{}, nil
	}

	emails := make([]NormEmail, 0, len(mailList))
	for _, item := range mailList {
		mailID := item.ID.String()

		htmlBody := disposablemailFetchMailBody(client, mailID)

		isRead := item.Precteno == "precteno"

		flat := map[string]interface{}{
			"id":      mailID,
			"from":    item.Od,
			"to":      email,
			"subject": item.Predmet,
			"html":    htmlBody,
			"date":    item.Kdy,
			"isRead":  isRead,
		}
		emails = append(emails, NormalizeMap(flat, email))
	}

	return emails, nil
}

/* disposablemailFetchMailBody 获取单封邮件的 HTML 正文 */
func disposablemailFetchMailBody(client interface {
	Do(*http.Request) (*http.Response, error)
}, mailID string) string {
	if mailID == "" {
		return ""
	}

	reqURL := fmt.Sprintf("%s/email/id/%s", disposablemailBaseURL, mailID)
	req, err := http.NewRequest("GET", reqURL, nil)
	if err != nil {
		return ""
	}
	disposablemailAjaxHeaders(req)

	resp, err := client.Do(req)
	if err != nil {
		return ""
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return ""
	}

	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return ""
	}

	return string(body)
}
