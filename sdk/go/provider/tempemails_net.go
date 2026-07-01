package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"regexp"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

/* tempemails.net 临时邮箱服务商（Laravel Cookie Session + CSRF + REST JSON）
 * 流程：GET / 获取 session cookie + 从 HTML 提取 CSRF token
 *       POST /get_messages 获取自动分配的邮箱地址和邮件列表
 *       POST /change 切换/创建新邮箱（可选）
 *       GET /view/{id} 获取邮件 HTML 正文
 * token 存储 CSRF token 字符串
 */

const tempemailsNetBaseURL = "https://tempemails.net"

/* tempemailsNetCSRFRe 从 HTML meta 标签中提取 CSRF token */
var tempemailsNetCSRFRe = regexp.MustCompile(`<meta\s+name="csrf-token"\s+content="([^"]+)"`)

/* tempemailsNetSetXHRHeaders 设置 AJAX 请求所需的通用请求头 */
func tempemailsNetSetXHRHeaders(req *http.Request, csrf string) {
	req.Header.Set("User-Agent", getCurrentUA())
	req.Header.Set("Accept", "application/json, text/plain, */*")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
	req.Header.Set("X-Requested-With", "XMLHttpRequest")
	req.Header.Set("X-CSRF-TOKEN", csrf)
	req.Header.Set("Referer", tempemailsNetBaseURL+"/")
	req.Header.Set("Origin", tempemailsNetBaseURL)
}

/* tempemailsNetFetchCSRF 访问首页建立 session 并从 HTML 中提取 CSRF token
 * GET / 返回 HTML 页面，从 <meta name="csrf-token" content="xxx"> 提取 CSRF token
 * 同时服务端会设置 XSRF-TOKEN 和 temp_mail_session Cookie
 */
func tempemailsNetFetchCSRF(client interface {
	Do(*http.Request) (*http.Response, error)
}) (string, error) {
	req, err := http.NewRequest("GET", tempemailsNetBaseURL+"/", nil)
	if err != nil {
		return "", fmt.Errorf("tempemails-net: 创建首页请求失败: %w", err)
	}
	req.Header.Set("User-Agent", getCurrentUA())
	req.Header.Set("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")

	resp, err := client.Do(req)
	if err != nil {
		return "", fmt.Errorf("tempemails-net: 获取首页失败: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", fmt.Errorf("tempemails-net: 读取首页响应失败: %w", err)
	}

	if err := CheckHTTPStatus(resp, "tempemails-net home"); err != nil {
		return "", err
	}

	/* 从 HTML meta 标签中提取 CSRF token */
	m := tempemailsNetCSRFRe.FindStringSubmatch(string(body))
	if len(m) < 2 || m[1] == "" {
		return "", fmt.Errorf("tempemails-net: 未能从首页提取 CSRF token")
	}

	return m[1], nil
}

/* TempemailsNetGenerate 创建 tempemails.net 临时邮箱
 * 流程:
 *   1. GET / 获取 session cookie 和 CSRF token
 *   2. POST /get_messages 获取服务端自动分配的邮箱地址
 * token: CSRF token 字符串
 */
func TempemailsNetGenerate() (*CreatedMailbox, error) {
	client := HTTPClient()

	/* 步骤 1：GET / 获取 session cookie + CSRF token */
	csrf, err := tempemailsNetFetchCSRF(client)
	if err != nil {
		return nil, err
	}

	/* 步骤 2：POST /get_messages 获取自动分配的邮箱地址 */
	req, err := http.NewRequest("POST", tempemailsNetBaseURL+"/get_messages", nil)
	if err != nil {
		return nil, fmt.Errorf("tempemails-net: 创建获取消息请求失败: %w", err)
	}
	tempemailsNetSetXHRHeaders(req, csrf)

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("tempemails-net: 获取消息失败: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("tempemails-net: 读取消息响应失败: %w", err)
	}

	if err := CheckHTTPStatus(resp, "tempemails-net get_messages"); err != nil {
		return nil, err
	}

	var msgResp struct {
		Status     bool   `json:"status"`
		Mailbox    string `json:"mailbox"`
		EmailToken string `json:"email_token"`
	}
	if err := json.Unmarshal(body, &msgResp); err != nil {
		return nil, fmt.Errorf("tempemails-net: 解析消息响应失败: %w", err)
	}

	if !msgResp.Status {
		return nil, fmt.Errorf("tempemails-net: 获取邮箱失败, status=false")
	}

	mailbox := strings.TrimSpace(msgResp.Mailbox)
	if mailbox == "" || !strings.Contains(mailbox, "@") {
		return nil, fmt.Errorf("tempemails-net: 返回的邮箱地址无效: %q", mailbox)
	}

	return &CreatedMailbox{
		Channel: "tempemails-net",
		Email:   mailbox,
		Token:   csrf,
	}, nil
}

/* TempemailsNetGetEmails 获取 tempemails.net 邮件列表
 * 流程:
 *   1. GET / 重新建立 session 并获取新的 CSRF token（旧 session 可能已失效）
 *   2. POST /get_messages 获取邮件列表
 *   3. 对每封邮件 GET /view/{id} 获取 HTML 正文（可选）
 */
func TempemailsNetGetEmails(email, token string) ([]NormEmail, error) {
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("tempemails-net: 邮箱地址为空")
	}

	client := HTTPClient()

	/* 步骤 1：GET / 重新建立 session + 获取新 CSRF token */
	csrf, err := tempemailsNetFetchCSRF(client)
	if err != nil {
		return nil, err
	}

	/* 步骤 2：POST /get_messages 获取邮件列表 */
	req, err := http.NewRequest("POST", tempemailsNetBaseURL+"/get_messages", nil)
	if err != nil {
		return nil, fmt.Errorf("tempemails-net: 创建获取邮件请求失败: %w", err)
	}
	tempemailsNetSetXHRHeaders(req, csrf)

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("tempemails-net: 获取邮件列表失败: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("tempemails-net: 读取邮件列表响应失败: %w", err)
	}

	if err := CheckHTTPStatus(resp, "tempemails-net get_messages"); err != nil {
		return nil, err
	}

	var msgResp struct {
		Status   bool              `json:"status"`
		Mailbox  string            `json:"mailbox"`
		Messages []json.RawMessage `json:"messages"`
	}
	if err := json.Unmarshal(body, &msgResp); err != nil {
		return nil, fmt.Errorf("tempemails-net: 解析邮件列表失败: %w", err)
	}

	if !msgResp.Status {
		return nil, fmt.Errorf("tempemails-net: 获取邮件列表失败, status=false")
	}

	if len(msgResp.Messages) == 0 {
		return []NormEmail{}, nil
	}

	/* 步骤 3：解析每封邮件并获取 HTML 正文 */
	emails := make([]NormEmail, 0, len(msgResp.Messages))
	for _, raw := range msgResp.Messages {
		var item map[string]interface{}
		if err := json.Unmarshal(raw, &item); err != nil {
			continue
		}

		/* 尝试获取邮件 ID 用于请求正文 */
		mailID := tempemailsNetExtractID(item)
		if mailID != "" {
			htmlBody := tempemailsNetFetchMailBody(client, mailID)
			if htmlBody != "" {
				item["html"] = htmlBody
			}
		}

		/* 补充 to 字段 */
		item["to"] = email

		/* 映射 from_email 到 from（如果 from 不存在） */
		if _, ok := item["from"]; !ok {
			if fromEmail, ok := item["from_email"]; ok {
				item["from"] = fromEmail
			}
		}

		emails = append(emails, NormalizeMap(item, email))
	}

	return emails, nil
}

/* tempemailsNetExtractID 从邮件 map 中提取 ID 字段 */
func tempemailsNetExtractID(item map[string]interface{}) string {
	for _, key := range []string{"id", "Id", "ID", "mail_id"} {
		if v, ok := item[key]; ok {
			switch val := v.(type) {
			case string:
				return val
			case float64:
				return fmt.Sprintf("%.0f", val)
			case json.Number:
				return val.String()
			}
		}
	}
	return ""
}

/* tempemailsNetFetchMailBody 获取单封邮件的 HTML 正文
 * GET /view/{id} 返回 HTML 内容
 */
func tempemailsNetFetchMailBody(client interface {
	Do(*http.Request) (*http.Response, error)
}, mailID string) string {
	if mailID == "" {
		return ""
	}

	viewURL := fmt.Sprintf("%s/view/%s", tempemailsNetBaseURL, mailID)
	req, err := http.NewRequest("GET", viewURL, nil)
	if err != nil {
		return ""
	}
	req.Header.Set("User-Agent", getCurrentUA())
	req.Header.Set("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
	req.Header.Set("Referer", tempemailsNetBaseURL+"/")

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
