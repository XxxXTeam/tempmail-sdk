package provider

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

// tempgmailer.com：提供真实 @gmail.com 地址（dot trick）的临时邮箱服务

const tempgmailerBase = "https://tempgmailer.com"

// tempgmailerSession 保存 CSRF token 和 Laravel session cookie
type tempgmailerSession struct {
	CSRFToken string `json:"csrfToken"`
	Cookie    string `json:"cookie"`
}

// tempgmailerGenerateResponse 创建邮箱 API 响应
type tempgmailerGenerateResponse struct {
	Success bool `json:"success"`
	Data    struct {
		Email string `json:"email"`
	} `json:"data"`
}

// tempgmailerInboxResponse 收件箱 API 响应
type tempgmailerInboxResponse struct {
	Success bool `json:"success"`
	Data    struct {
		Email    string                   `json:"email"`
		Messages []tempgmailerMessageItem `json:"messages"`
	} `json:"data"`
}

// tempgmailerMessageItem 单封邮件结构
type tempgmailerMessageItem struct {
	From struct {
		Address string `json:"address"`
	} `json:"from"`
	Subject   string `json:"subject"`
	Intro     string `json:"intro"`
	Body      string `json:"body"`
	CreatedAt string `json:"createdAt"`
}

// tempgmailerInitSession 访问首页获取 CSRF token 和 session cookie
func tempgmailerInitSession() (tempgmailerSession, error) {
	req, err := http.NewRequest("GET", tempgmailerBase, nil)
	if err != nil {
		return tempgmailerSession{}, err
	}
	req.Header.Set("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8")
	req.Header.Set("User-Agent", getCurrentUA())

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return tempgmailerSession{}, err
	}
	defer resp.Body.Close()
	if err := CheckHTTPStatus(resp, "tempgmailer home"); err != nil {
		return tempgmailerSession{}, err
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return tempgmailerSession{}, err
	}

	// 从 HTML 中提取 <meta name="csrf-token" content="...">
	csrfToken := tempgmailerExtractCSRF(string(body))
	if csrfToken == "" {
		return tempgmailerSession{}, fmt.Errorf("tempgmailer: 未找到 CSRF token")
	}

	// 合并 Set-Cookie
	cookie := moaktMergeCookies("", resp.Cookies())
	if cookie == "" {
		return tempgmailerSession{}, fmt.Errorf("tempgmailer: 未获取到 session cookie")
	}

	return tempgmailerSession{CSRFToken: csrfToken, Cookie: cookie}, nil
}

// tempgmailerExtractCSRF 从 HTML 中提取 csrf-token meta 标签的 content 值
func tempgmailerExtractCSRF(html string) string {
	// 查找 <meta name="csrf-token" content="...">
	marker := `name="csrf-token"`
	idx := strings.Index(html, marker)
	if idx == -1 {
		return ""
	}
	// 在 marker 之后查找 content="
	sub := html[idx+len(marker):]
	contentPrefix := `content="`
	cIdx := strings.Index(sub, contentPrefix)
	if cIdx == -1 {
		return ""
	}
	sub = sub[cIdx+len(contentPrefix):]
	endIdx := strings.Index(sub, `"`)
	if endIdx == -1 {
		return ""
	}
	return sub[:endIdx]
}

// tempgmailerPost 发送 POST 请求到 tempgmailer API
func tempgmailerPost(session tempgmailerSession, path string, body any) ([]byte, error) {
	reqBody, err := json.Marshal(body)
	if err != nil {
		return nil, err
	}
	req, err := http.NewRequest("POST", tempgmailerBase+path, bytes.NewReader(reqBody))
	if err != nil {
		return nil, err
	}
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Accept", "application/json, text/plain, */*")
	req.Header.Set("X-CSRF-TOKEN", session.CSRFToken)
	req.Header.Set("X-TempGmailer-Auth", "frontend")
	req.Header.Set("X-Requested-With", "XMLHttpRequest")
	req.Header.Set("Cookie", session.Cookie)
	req.Header.Set("User-Agent", getCurrentUA())

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("tempgmailer %s: %d %s", path, resp.StatusCode, string(raw))
	}
	return raw, nil
}

// tempgmailerEncodeSession 将 session 序列化为 JSON 字符串（作为 token 存储）
func tempgmailerEncodeSession(session tempgmailerSession) string {
	raw, _ := json.Marshal(session)
	return string(raw)
}

// tempgmailerDecodeSession 从 JSON token 反序列化 session
func tempgmailerDecodeSession(token string) (tempgmailerSession, error) {
	var session tempgmailerSession
	if err := json.Unmarshal([]byte(token), &session); err != nil {
		return session, fmt.Errorf("tempgmailer: 无效的 session token: %w", err)
	}
	if session.Cookie == "" || session.CSRFToken == "" {
		return session, fmt.Errorf("tempgmailer: session token 缺少必要字段")
	}
	return session, nil
}

// TempgmailerGenerate 创建临时 Gmail 邮箱
func TempgmailerGenerate() (*CreatedMailbox, error) {
	session, err := tempgmailerInitSession()
	if err != nil {
		return nil, err
	}

	raw, err := tempgmailerPost(session, "/get-gmail", map[string]interface{}{
		"refresh": true,
		"adblock": 0,
	})
	if err != nil {
		return nil, err
	}

	var data tempgmailerGenerateResponse
	if err := json.Unmarshal(raw, &data); err != nil {
		return nil, fmt.Errorf("tempgmailer: 解析响应失败: %w", err)
	}
	if !data.Success || data.Data.Email == "" {
		return nil, fmt.Errorf("tempgmailer: 创建邮箱失败，响应: %s", string(raw))
	}

	return &CreatedMailbox{
		Channel: "tempgmailer",
		Email:   data.Data.Email,
		Token:   tempgmailerEncodeSession(session),
	}, nil
}

// TempgmailerGetEmails 获取邮件列表
func TempgmailerGetEmails(token string, email string) ([]NormEmail, error) {
	session, err := tempgmailerDecodeSession(token)
	if err != nil {
		return nil, err
	}

	raw, err := tempgmailerPost(session, "/get-inbox", map[string]interface{}{
		"email":  email,
		"adblock": 0,
	})
	if err != nil {
		return nil, err
	}

	var data tempgmailerInboxResponse
	if err := json.Unmarshal(raw, &data); err != nil {
		return nil, fmt.Errorf("tempgmailer: 解析收件箱响应失败: %w", err)
	}
	if !data.Success {
		return nil, fmt.Errorf("tempgmailer: 获取收件箱失败，响应: %s", string(raw))
	}

	out := make([]NormEmail, 0, len(data.Data.Messages))
	for _, msg := range data.Data.Messages {
		htmlContent := msg.Body
		if htmlContent == "" {
			htmlContent = msg.Intro
		}
		out = append(out, NormalizeMap(map[string]interface{}{
			"from":        msg.From.Address,
			"to":          email,
			"subject":     msg.Subject,
			"text":        msg.Intro,
			"html":        htmlContent,
			"date":        msg.CreatedAt,
			"isRead":      false,
			"attachments": []interface{}{},
		}, email))
	}
	return out, nil
}
