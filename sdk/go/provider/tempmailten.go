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

/* tempmailten.com 临时邮箱服务商（Laravel + CSRF）
 * 流程：GET /en 获取 session cookie（cookie jar 自动维护）+ 从 HTML meta 提取 CSRF token
 *       POST /messages（body: _token={csrf}&captcha=）获取邮箱地址和邮件列表
 *       GET /view/{id} 获取单封邮件 HTML 正文
 * token 存储 CSRF token 值，session 由 tls-client 的 cookie jar 自动维护
 * 该站点 reCAPTCHA 已禁用（check_recaptcha 直接设为 true），captcha 参数传空即可
 */

const tempmailtenBaseURL = "https://tempmailten.com"

/* tempmailtenCSRFRe 从 meta 标签提取 CSRF token（<meta name="csrf-token" content="xxx">） */
var tempmailtenCSRFRe = regexp.MustCompile(`<meta\s+name="csrf-token"\s+content="([^"]+)"`)

/* tempmailtenBrowserHeaders 设置浏览器模拟请求头 */
func tempmailtenBrowserHeaders(req *http.Request) {
	req.Header.Set("User-Agent", getCurrentUA())
	req.Header.Set("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8")
	req.Header.Set("Accept-Language", "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7")
}

/* tempmailtenAjaxHeaders 设置 AJAX 请求头 */
func tempmailtenAjaxHeaders(req *http.Request) {
	req.Header.Set("User-Agent", getCurrentUA())
	req.Header.Set("Accept", "application/json, text/javascript, */*; q=0.01")
	req.Header.Set("Accept-Language", "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7")
	req.Header.Set("X-Requested-With", "XMLHttpRequest")
	req.Header.Set("Referer", tempmailtenBaseURL+"/en")
}

/* TempmailtenGenerate 创建 tempmailten.com 临时邮箱
 * 流程:
 *   1. GET /en 获取 session cookie 和 CSRF token（正则匹配 meta csrf-token）
 *   2. POST /messages（body: _token={csrf}&captcha=）获取分配的邮箱地址
 * token: CSRF token 值
 */
func TempmailtenGenerate() (*CreatedMailbox, error) {
	client := HTTPClient()

	req, err := http.NewRequest("GET", tempmailtenBaseURL+"/en", nil)
	if err != nil {
		return nil, fmt.Errorf("tempmailten: 创建首页请求失败: %w", err)
	}
	tempmailtenBrowserHeaders(req)

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("tempmailten: 获取首页失败: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("tempmailten: 读取首页响应失败: %w", err)
	}

	if err := CheckHTTPStatus(resp, "tempmailten home"); err != nil {
		return nil, err
	}

	m := tempmailtenCSRFRe.FindSubmatch(body)
	if m == nil {
		return nil, fmt.Errorf("tempmailten: 未能从首页提取 CSRF token")
	}
	csrfToken := string(m[1])

	form := url.Values{}
	form.Set("_token", csrfToken)
	form.Set("captcha", "")

	req2, err := http.NewRequest("POST", tempmailtenBaseURL+"/messages", strings.NewReader(form.Encode()))
	if err != nil {
		return nil, fmt.Errorf("tempmailten: 创建消息请求失败: %w", err)
	}
	tempmailtenAjaxHeaders(req2)
	req2.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	req2.Header.Set("X-CSRF-TOKEN", csrfToken)

	resp2, err := client.Do(req2)
	if err != nil {
		return nil, fmt.Errorf("tempmailten: 获取邮箱失败: %w", err)
	}
	defer resp2.Body.Close()

	body2, err := io.ReadAll(resp2.Body)
	if err != nil {
		return nil, fmt.Errorf("tempmailten: 读取邮箱响应失败: %w", err)
	}

	if err := CheckHTTPStatus(resp2, "tempmailten messages"); err != nil {
		return nil, err
	}

	emailAddr, err := tempmailtenExtractMailbox(body2)
	if err != nil {
		return nil, err
	}

	return &CreatedMailbox{
		Channel: "tempmailten",
		Email:   emailAddr,
		Token:   csrfToken,
	}, nil
}

/* TempmailtenGetEmails 获取 tempmailten.com 邮件列表
 * 流程:
 *   1. POST /messages（body: _token={csrf}&captcha=）获取 {mailbox, messages} JSON
 *   2. 对每封邮件 GET /view/{id} 获取 HTML 正文
 * messages 数组每个元素: {id, from, from_email, subject, is_seen}
 */
func TempmailtenGetEmails(email, token string) ([]NormEmail, error) {
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("tempmailten: 邮箱地址为空")
	}
	csrfToken := strings.TrimSpace(token)
	if csrfToken == "" {
		return nil, fmt.Errorf("tempmailten: 缺少 CSRF token")
	}

	client := HTTPClient()

	form := url.Values{}
	form.Set("_token", csrfToken)
	form.Set("captcha", "")

	req, err := http.NewRequest("POST", tempmailtenBaseURL+"/messages", strings.NewReader(form.Encode()))
	if err != nil {
		return nil, fmt.Errorf("tempmailten: 创建消息请求失败: %w", err)
	}
	tempmailtenAjaxHeaders(req)
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	req.Header.Set("X-CSRF-TOKEN", csrfToken)

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("tempmailten: 获取邮件列表失败: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("tempmailten: 读取邮件列表响应失败: %w", err)
	}

	if err := CheckHTTPStatus(resp, "tempmailten messages"); err != nil {
		return nil, err
	}

	return tempmailtenParseMessages(body, email, csrfToken, client)
}

/* tempmailtenExtractMailbox 从 POST /messages 的 JSON 响应中提取邮箱地址 */
func tempmailtenExtractMailbox(data []byte) (string, error) {
	parsed := tempmailtenParseJSON(data)
	if parsed == nil {
		return "", fmt.Errorf("tempmailten: 解析响应 JSON 失败")
	}

	mailbox, ok := parsed["mailbox"].(string)
	if !ok || mailbox == "" {
		return "", fmt.Errorf("tempmailten: 响应中未包含有效的 mailbox 字段")
	}

	mailbox = strings.TrimSpace(mailbox)
	if !strings.Contains(mailbox, "@") {
		return "", fmt.Errorf("tempmailten: 邮箱地址无效: %q", mailbox)
	}

	return mailbox, nil
}

/* tempmailtenParseMessages 解析邮件列表并拉取正文 */
func tempmailtenParseMessages(data []byte, email, csrfToken string, client interface {
	Do(*http.Request) (*http.Response, error)
}) ([]NormEmail, error) {
	parsed := tempmailtenParseJSON(data)
	if parsed == nil {
		return nil, fmt.Errorf("tempmailten: 解析响应 JSON 失败")
	}

	msgsRaw, ok := parsed["messages"].([]interface{})
	if !ok || len(msgsRaw) == 0 {
		return []NormEmail{}, nil
	}

	emails := make([]NormEmail, 0, len(msgsRaw))
	for _, raw := range msgsRaw {
		msg, ok := raw.(map[string]interface{})
		if !ok {
			continue
		}

		id := tempmailtenStr(msg["id"])
		if id == "" {
			continue
		}

		fromAddr := tempmailtenStr(msg["from_email"])
		fromName := tempmailtenStr(msg["from"])
		if fromName != "" && fromName != fromAddr {
			fromAddr = fmt.Sprintf("%s <%s>", fromName, fromAddr)
		}

		htmlBody := tempmailtenFetchView(client, id)

		isSeen, _ := msg["is_seen"].(bool)

		flat := map[string]interface{}{
			"id":      id,
			"from":    fromAddr,
			"to":      email,
			"subject": tempmailtenStr(msg["subject"]),
			"html":    htmlBody,
			"isRead":  isSeen,
		}
		emails = append(emails, NormalizeMap(flat, email))
	}

	return emails, nil
}

/* tempmailtenFetchView 获取单封邮件的 HTML 正文（GET /view/{id}） */
func tempmailtenFetchView(client interface {
	Do(*http.Request) (*http.Response, error)
}, id string) string {
	if id == "" {
		return ""
	}

	req, err := http.NewRequest("GET", tempmailtenBaseURL+"/view/"+url.PathEscape(id), nil)
	if err != nil {
		return ""
	}
	tempmailtenBrowserHeaders(req)
	req.Header.Set("Referer", tempmailtenBaseURL+"/en")

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

/* tempmailtenParseJSON 解析 JSON 为 map */
func tempmailtenParseJSON(data []byte) map[string]interface{} {
	var result map[string]interface{}
	if err := json.Unmarshal(data, &result); err != nil {
		return nil
	}
	return result
}

/* tempmailtenStr 安全提取 interface{} 为 string */
func tempmailtenStr(v interface{}) string {
	if v == nil {
		return ""
	}
	switch s := v.(type) {
	case string:
		return strings.TrimSpace(s)
	case float64:
		return fmt.Sprintf("%.0f", s)
	default:
		return fmt.Sprintf("%v", s)
	}
}
