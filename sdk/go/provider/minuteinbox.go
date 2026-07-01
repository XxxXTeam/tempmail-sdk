package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"regexp"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

/* minuteinbox.com 临时邮箱服务商
 * 流程：GET / 获取 session cookie + CSRF token → GET /index/index?csrf_token={csrf} 创建邮箱
 *       GET /index/refresh 获取邮件列表 → GET /email/id/{id} 获取邮件 HTML 正文
 * token 存储 CSRF token 值，session 由 tls-client 的 cookie jar 自动维护
 */

const minuteinboxBaseURL = "https://www.minuteinbox.com"

/* minuteinboxCSRFRe 从 HTML 中提取 CSRF token（格式: CSRF="xxx"） */
var minuteinboxCSRFRe = regexp.MustCompile(`CSRF="([^"]+)"`)

/* minuteinboxBrowserHeaders 设置浏览器模拟请求头 */
func minuteinboxBrowserHeaders(req *http.Request) {
	req.Header.Set("User-Agent", getCurrentUA())
	req.Header.Set("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8")
	req.Header.Set("Accept-Language", "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7")
}

/* minuteinboxAjaxHeaders 设置 AJAX 请求头 */
func minuteinboxAjaxHeaders(req *http.Request) {
	req.Header.Set("User-Agent", getCurrentUA())
	req.Header.Set("Accept", "application/json, text/javascript, */*; q=0.01")
	req.Header.Set("Accept-Language", "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7")
	req.Header.Set("X-Requested-With", "XMLHttpRequest")
	req.Header.Set("Referer", minuteinboxBaseURL+"/")
}

/* MinuteinboxGenerate 创建 minuteinbox.com 临时邮箱
 * 流程:
 *   1. GET / 获取 session cookie 和 CSRF token（正则匹配 CSRF="xxx"）
 *   2. GET /index/index?csrf_token={csrf} 创建邮箱，返回 {"email":"user@minafter.com"}
 * token: CSRF token 值
 */
func MinuteinboxGenerate() (*CreatedMailbox, error) {
	client := HTTPClient()

	/* 步骤 1：GET / 获取 session cookie 和 CSRF token */
	req, err := http.NewRequest("GET", minuteinboxBaseURL+"/", nil)
	if err != nil {
		return nil, fmt.Errorf("minuteinbox: 创建首页请求失败: %w", err)
	}
	minuteinboxBrowserHeaders(req)

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("minuteinbox: 获取首页失败: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("minuteinbox: 读取首页响应失败: %w", err)
	}

	if err := CheckHTTPStatus(resp, "minuteinbox home"); err != nil {
		return nil, err
	}

	/* 从 HTML 中提取 CSRF token */
	m := minuteinboxCSRFRe.FindStringSubmatch(string(body))
	if len(m) < 2 || m[1] == "" {
		return nil, fmt.Errorf("minuteinbox: 未能从首页提取 CSRF token")
	}
	csrfToken := m[1]

	/* 步骤 2：GET /index/index?csrf_token={csrf} 创建邮箱 */
	createURL := fmt.Sprintf("%s/index/index?csrf_token=%s", minuteinboxBaseURL, csrfToken)
	req2, err := http.NewRequest("GET", createURL, nil)
	if err != nil {
		return nil, fmt.Errorf("minuteinbox: 创建邮箱请求失败: %w", err)
	}
	minuteinboxAjaxHeaders(req2)

	resp2, err := client.Do(req2)
	if err != nil {
		return nil, fmt.Errorf("minuteinbox: 创建邮箱失败: %w", err)
	}
	defer resp2.Body.Close()

	body2, err := io.ReadAll(resp2.Body)
	if err != nil {
		return nil, fmt.Errorf("minuteinbox: 读取创建邮箱响应失败: %w", err)
	}

	if err := CheckHTTPStatus(resp2, "minuteinbox create"); err != nil {
		return nil, err
	}

	/* 解析 JSON 响应 {"email":"user@minafter.com"} */
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

	return &CreatedMailbox{
		Channel: "minuteinbox",
		Email:   emailAddr,
		Token:   csrfToken,
	}, nil
}

/* MinuteinboxGetEmails 获取 minuteinbox.com 邮件列表
 * 流程:
 *   1. GET /index/refresh 获取邮件列表（返回 JSON 数组，空收件箱返回数字 0）
 *   2. 对每封邮件 GET /email/id/{id} 获取 HTML 正文
 * 字段说明（捷克语）: predmet=subject, od=from, id=邮件ID, kdy=when, precteno=read status
 */
func MinuteinboxGetEmails(email, token string) ([]NormEmail, error) {
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("minuteinbox: 邮箱地址为空")
	}

	client := HTTPClient()

	/* GET /index/refresh 获取邮件列表 */
	req, err := http.NewRequest("GET", minuteinboxBaseURL+"/index/refresh", nil)
	if err != nil {
		return nil, fmt.Errorf("minuteinbox: 创建刷新请求失败: %w", err)
	}
	minuteinboxAjaxHeaders(req)

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("minuteinbox: 获取邮件列表失败: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("minuteinbox: 读取邮件列表响应失败: %w", err)
	}

	if err := CheckHTTPStatus(resp, "minuteinbox refresh"); err != nil {
		return nil, err
	}

	/* 空收件箱返回数字 0，直接返回空列表 */
	trimmed := strings.TrimSpace(string(body))
	if trimmed == "0" || trimmed == "" || trimmed == "[]" {
		return []NormEmail{}, nil
	}

	/* 解析邮件列表 JSON 数组
	 * 每个元素: {"predmet":"subject", "od":"from", "id":1, "kdy":"time ago", "precteno":"new|precteno"}
	 */
	var mailList []struct {
		Predmet  string      `json:"predmet"`  /* 邮件主题（捷克语） */
		Od       string      `json:"od"`       /* 发件人地址（捷克语） */
		ID       json.Number `json:"id"`       /* 邮件 ID */
		Kdy      string      `json:"kdy"`      /* 时间描述（捷克语） */
		Precteno string      `json:"precteno"` /* 已读状态: "new" 或 "precteno"（捷克语） */
	}
	if err := json.Unmarshal(body, &mailList); err != nil {
		return nil, fmt.Errorf("minuteinbox: 解析邮件列表失败: %w", err)
	}

	if len(mailList) == 0 {
		return []NormEmail{}, nil
	}

	emails := make([]NormEmail, 0, len(mailList))
	for _, item := range mailList {
		mailID := item.ID.String()

		/* GET /email/id/{id} 获取邮件 HTML 正文 */
		htmlBody := minuteinboxFetchMailBody(client, mailID)

		/* 已读状态: "precteno" 表示已读，"new" 表示未读 */
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

/* minuteinboxFetchMailBody 获取单封邮件的 HTML 正文 */
func minuteinboxFetchMailBody(client interface {
	Do(*http.Request) (*http.Response, error)
}, mailID string) string {
	if mailID == "" {
		return ""
	}

	reqURL := fmt.Sprintf("%s/email/id/%s", minuteinboxBaseURL, mailID)
	req, err := http.NewRequest("GET", reqURL, nil)
	if err != nil {
		return ""
	}
	minuteinboxAjaxHeaders(req)

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
