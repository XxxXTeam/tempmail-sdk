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

/* mailgolem.com 临时邮箱服务商
 * 流程：GET / 获取 session cookie + CSRF token → GET /random-email-address 创建邮箱
 *       GET / 重新获取 session + CSRF → POST /fetch-emails/{email} 获取邮件列表
 * token 存储 CSRF token 值
 */

const mailgolemBaseURL = "https://mailgolem.com"

/* mailgolemCSRFRe 从 HTML 中提取 CSRF token */
var mailgolemCSRFRe = regexp.MustCompile(`<input[^>]+name="_token"[^>]+id="token"[^>]+value="([^"]*)"`)

/* mailgolemBrowserHeaders 设置浏览器模拟请求头 */
func mailgolemBrowserHeaders(req *http.Request) {
	req.Header.Set("User-Agent", getCurrentUA())
	req.Header.Set("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8")
	req.Header.Set("Accept-Language", "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7")
}

/* mailgolemFetchCSRF 访问首页建立 session 并从 HTML 中提取 CSRF token
 * 返回提取到的 CSRF token 值
 */
func mailgolemFetchCSRF(client interface {
	Do(*http.Request) (*http.Response, error)
}) (string, error) {
	req, err := http.NewRequest("GET", mailgolemBaseURL+"/", nil)
	if err != nil {
		return "", fmt.Errorf("mailgolem: 创建首页请求失败: %w", err)
	}
	mailgolemBrowserHeaders(req)

	resp, err := client.Do(req)
	if err != nil {
		return "", fmt.Errorf("mailgolem: 获取首页失败: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", fmt.Errorf("mailgolem: 读取首页响应失败: %w", err)
	}

	if err := CheckHTTPStatus(resp, "mailgolem home"); err != nil {
		return "", err
	}

	/* 从 HTML 中提取 CSRF token */
	m := mailgolemCSRFRe.FindStringSubmatch(string(body))
	if len(m) < 2 || m[1] == "" {
		return "", fmt.Errorf("mailgolem: 未能从首页提取 CSRF token")
	}

	return m[1], nil
}

/* MailgolemGenerate 创建 mailgolem.com 临时邮箱
 * 流程:
 *   1. GET / 获取 session cookie 和 CSRF token
 *   2. GET /random-email-address 获取随机邮箱地址（纯文本响应）
 * token: CSRF token 值
 */
func MailgolemGenerate() (*CreatedMailbox, error) {
	client := HTTPClient()

	/* 步骤 1：GET / 获取 session + CSRF token */
	csrfToken, err := mailgolemFetchCSRF(client)
	if err != nil {
		return nil, err
	}

	/* 步骤 2：GET /random-email-address 获取邮箱地址 */
	req, err := http.NewRequest("GET", mailgolemBaseURL+"/random-email-address", nil)
	if err != nil {
		return nil, fmt.Errorf("mailgolem: 创建随机邮箱请求失败: %w", err)
	}
	mailgolemBrowserHeaders(req)
	req.Header.Set("Referer", mailgolemBaseURL+"/")

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("mailgolem: 获取随机邮箱失败: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("mailgolem: 读取随机邮箱响应失败: %w", err)
	}

	if err := CheckHTTPStatus(resp, "mailgolem random-email-address"); err != nil {
		return nil, err
	}

	emailAddr := strings.TrimSpace(string(body))
	if emailAddr == "" || !strings.Contains(emailAddr, "@") {
		return nil, fmt.Errorf("mailgolem: 获取到的邮箱地址无效: %q", emailAddr)
	}

	return &CreatedMailbox{
		Channel: "mailgolem",
		Email:   emailAddr,
		Token:   csrfToken,
	}, nil
}

/* MailgolemGetEmails 获取 mailgolem.com 邮件列表
 * 流程:
 *   1. GET / 重新建立 session 并获取新的 CSRF token（旧 session 可能已失效）
 *   2. POST /fetch-emails/{email} 获取邮件列表
 * 返回 JSON 数组 [{id, from, subject}]
 */
func MailgolemGetEmails(email, token string) ([]NormEmail, error) {
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("mailgolem: 邮箱地址为空")
	}

	client := HTTPClient()

	/* 步骤 1：GET / 重新建立 session + 获取新 CSRF token */
	csrfToken, err := mailgolemFetchCSRF(client)
	if err != nil {
		return nil, err
	}

	/* 步骤 2：POST /fetch-emails/{email} 获取邮件列表 */
	form := url.Values{}
	form.Set("_token", csrfToken)

	fetchURL := fmt.Sprintf("%s/fetch-emails/%s", mailgolemBaseURL, url.PathEscape(email))
	req, err := http.NewRequest("POST", fetchURL, strings.NewReader(form.Encode()))
	if err != nil {
		return nil, fmt.Errorf("mailgolem: 创建获取邮件请求失败: %w", err)
	}
	req.Header.Set("User-Agent", getCurrentUA())
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	req.Header.Set("X-Requested-With", "XMLHttpRequest")
	req.Header.Set("Accept", "application/json, text/plain, */*")
	req.Header.Set("Accept-Language", "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7")
	req.Header.Set("Referer", mailgolemBaseURL+"/")
	req.Header.Set("Origin", mailgolemBaseURL)

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("mailgolem: 获取邮件列表失败: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("mailgolem: 读取邮件列表响应失败: %w", err)
	}

	if err := CheckHTTPStatus(resp, "mailgolem fetch-emails"); err != nil {
		return nil, err
	}

	/* 解析 JSON 数组 [{id, from, subject}, ...] */
	var rawItems []map[string]interface{}
	if err := json.Unmarshal(body, &rawItems); err != nil {
		/* 可能返回空数组或非 JSON 内容 */
		trimmed := strings.TrimSpace(string(body))
		if trimmed == "[]" || trimmed == "" {
			return []NormEmail{}, nil
		}
		return nil, fmt.Errorf("mailgolem: 解析邮件列表失败: %w", err)
	}

	if len(rawItems) == 0 {
		return []NormEmail{}, nil
	}

	/* 为每封邮件补充 to 字段后归一化 */
	emails := make([]NormEmail, 0, len(rawItems))
	for _, item := range rawItems {
		item["to"] = email
		emails = append(emails, NormalizeMap(item, email))
	}

	return emails, nil
}
