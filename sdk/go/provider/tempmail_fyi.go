package provider

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"regexp"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

/* temp-mail.fyi 临时邮箱服务商
 * 流程：GET / 获取 PHPSESSID cookie + 从 HTML 正则提取 csrfToken
 *       POST /api/generate_email.php 创建邮箱（需 X-CSRF-Token 头 + session cookie）
 *       POST /api/get_emails.php 获取邮件列表（body 携带 email_address）
 * token 存储 CSRF token 值，session 由 tls-client 的 cookie jar 自动维护
 */

const tempmailFyiBaseURL = "https://temp-mail.fyi"

/* tempmailFyiCSRFRe 从 HTML 中提取 CSRF token（格式: csrfToken" value="xxx"） */
var tempmailFyiCSRFRe = regexp.MustCompile(`csrfToken"\s*value="([^"]+)"`)

/* tempmailFyiBrowserHeaders 设置浏览器模拟请求头（首页 GET） */
func tempmailFyiBrowserHeaders(req *http.Request) {
	req.Header.Set("User-Agent", getCurrentUA())
	req.Header.Set("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8")
	req.Header.Set("Accept-Language", "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7")
}

/* tempmailFyiAPIHeaders 设置 API 请求头（携带 CSRF token） */
func tempmailFyiAPIHeaders(req *http.Request, csrfToken string) {
	req.Header.Set("User-Agent", getCurrentUA())
	req.Header.Set("Accept", "application/json, text/javascript, */*; q=0.01")
	req.Header.Set("Accept-Language", "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7")
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-CSRF-Token", csrfToken)
	req.Header.Set("X-Requested-With", "XMLHttpRequest")
	req.Header.Set("Referer", tempmailFyiBaseURL+"/")
}

/* TempMailFyiGenerate 创建 temp-mail.fyi 临时邮箱
 * 流程:
 *   1. GET / 获取 PHPSESSID cookie 和 CSRF token（正则匹配 csrfToken" value="xxx"）
 *   2. POST /api/generate_email.php 创建邮箱，返回 {"success":true,"email_address":"xxx@...","expires_at":"...","error":null}
 * token: CSRF token 值（GetEmails 时复用）
 */
func TempMailFyiGenerate() (*CreatedMailbox, error) {
	client := HTTPClient()

	/* 步骤 1：GET / 获取 session cookie 和 CSRF token */
	req, err := http.NewRequest("GET", tempmailFyiBaseURL+"/", nil)
	if err != nil {
		return nil, fmt.Errorf("tempmail-fyi: 创建首页请求失败: %w", err)
	}
	tempmailFyiBrowserHeaders(req)

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("tempmail-fyi: 获取首页失败: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("tempmail-fyi: 读取首页响应失败: %w", err)
	}

	if err := CheckHTTPStatus(resp, "tempmail-fyi home"); err != nil {
		return nil, err
	}

	/* 从 HTML 中提取 CSRF token */
	m := tempmailFyiCSRFRe.FindStringSubmatch(string(body))
	if len(m) < 2 || m[1] == "" {
		return nil, fmt.Errorf("tempmail-fyi: 未能从首页提取 CSRF token")
	}
	csrfToken := m[1]

	/* 步骤 2：POST /api/generate_email.php 创建邮箱 */
	req2, err := http.NewRequest("POST", tempmailFyiBaseURL+"/api/generate_email.php", bytes.NewReader([]byte("{}")))
	if err != nil {
		return nil, fmt.Errorf("tempmail-fyi: 创建邮箱请求失败: %w", err)
	}
	tempmailFyiAPIHeaders(req2, csrfToken)

	resp2, err := client.Do(req2)
	if err != nil {
		return nil, fmt.Errorf("tempmail-fyi: 创建邮箱失败: %w", err)
	}
	defer resp2.Body.Close()

	body2, err := io.ReadAll(resp2.Body)
	if err != nil {
		return nil, fmt.Errorf("tempmail-fyi: 读取创建邮箱响应失败: %w", err)
	}

	if err := CheckHTTPStatus(resp2, "tempmail-fyi create"); err != nil {
		return nil, err
	}

	/* 解析 JSON 响应 {"success":true,"email_address":"xxx@...","expires_at":"...","error":null} */
	var createResp struct {
		Success      bool   `json:"success"`
		EmailAddress string `json:"email_address"`
		ExpiresAt    string `json:"expires_at"`
		Error        string `json:"error"`
	}
	if err := json.Unmarshal(body2, &createResp); err != nil {
		return nil, fmt.Errorf("tempmail-fyi: 解析创建邮箱响应失败: %w", err)
	}

	if !createResp.Success {
		if createResp.Error != "" {
			return nil, fmt.Errorf("tempmail-fyi: 创建邮箱失败: %s", createResp.Error)
		}
		return nil, fmt.Errorf("tempmail-fyi: 创建邮箱失败（success=false）")
	}

	emailAddr := strings.TrimSpace(createResp.EmailAddress)
	if emailAddr == "" || !strings.Contains(emailAddr, "@") {
		return nil, fmt.Errorf("tempmail-fyi: 获取到的邮箱地址无效: %q", emailAddr)
	}

	return &CreatedMailbox{
		Channel:   "tempmail-fyi",
		Email:     emailAddr,
		Token:     csrfToken,
		ExpiresAt: createResp.ExpiresAt,
	}, nil
}

/* TempMailFyiGetEmails 获取 temp-mail.fyi 邮件列表
 * 流程:
 *   POST /api/get_emails.php，body 携带 {"email_address":"xxx@..."}，需 X-CSRF-Token 头 + session cookie
 *   返回 {"success":true,"emails":[...],"error":null}
 * token: 创建邮箱时获得的 CSRF token
 */
func TempMailFyiGetEmails(email, token string) ([]NormEmail, error) {
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("tempmail-fyi: 邮箱地址为空")
	}
	if token == "" {
		return nil, fmt.Errorf("tempmail-fyi: CSRF token 为空")
	}

	client := HTTPClient()

	/* 构造请求体 {"email_address":"xxx@..."} */
	reqBody, err := json.Marshal(map[string]string{"email_address": email})
	if err != nil {
		return nil, fmt.Errorf("tempmail-fyi: 构造请求体失败: %w", err)
	}

	req, err := http.NewRequest("POST", tempmailFyiBaseURL+"/api/get_emails.php", bytes.NewReader(reqBody))
	if err != nil {
		return nil, fmt.Errorf("tempmail-fyi: 创建获取邮件请求失败: %w", err)
	}
	tempmailFyiAPIHeaders(req, token)

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("tempmail-fyi: 获取邮件列表失败: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("tempmail-fyi: 读取邮件列表响应失败: %w", err)
	}

	if err := CheckHTTPStatus(resp, "tempmail-fyi get_emails"); err != nil {
		return nil, err
	}

	/* 解析响应 {"success":true,"emails":[...],"error":null}
	 * emails 字段保留为原始 JSON，逐封交由 NormalizeMap 标准化
	 */
	var listResp struct {
		Success bool                     `json:"success"`
		Emails  []map[string]interface{} `json:"emails"`
		Error   string                   `json:"error"`
	}
	if err := json.Unmarshal(body, &listResp); err != nil {
		return nil, fmt.Errorf("tempmail-fyi: 解析邮件列表失败: %w", err)
	}

	if !listResp.Success {
		if listResp.Error != "" {
			return nil, fmt.Errorf("tempmail-fyi: 获取邮件列表失败: %s", listResp.Error)
		}
		return nil, fmt.Errorf("tempmail-fyi: 获取邮件列表失败（success=false）")
	}

	if len(listResp.Emails) == 0 {
		return []NormEmail{}, nil
	}

	emails := make([]NormEmail, 0, len(listResp.Emails))
	for _, item := range listResp.Emails {
		emails = append(emails, NormalizeMap(item, email))
	}

	return emails, nil
}
