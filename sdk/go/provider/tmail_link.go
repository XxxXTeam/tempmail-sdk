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

/**
 * TmailLink — https://tmail.link
 * Django 临时邮箱服务（CSRF token 认证）
 * 流程:
 *   1. GET / → HTML 包含随机邮箱地址 (xxx@tmail.link)
 *   2. GET /inbox/{email}/ → 获取 csrftoken cookie
 *   3. POST /inbox/{email}/ + format=json + csrfmiddlewaretoken → JSON 邮件列表
 * Token 格式: csrftoken 字符串
 */

const tmailLinkBase = "https://tmail.link"

var tmailLinkEmailRegex = regexp.MustCompile(`([a-zA-Z0-9._%+-]+@tmail\.link)`)

/**
 * TmailLinkGenerate — 创建 tmail.link 临时邮箱
 */
func TmailLinkGenerate() (*CreatedMailbox, error) {
	client := HTTPClient()

	/* 步骤1: GET / 获取随机邮箱地址 */
	req, err := http.NewRequest("GET", tmailLinkBase+"/", nil)
	if err != nil {
		return nil, fmt.Errorf("tmail-link: 创建请求失败: %w", err)
	}
	req.Header.Set("Accept", "text/html")
	req.Header.Set("User-Agent", getCurrentUA())

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("tmail-link: 请求失败: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("tmail-link: 读取响应失败: %w", err)
	}

	if resp.StatusCode != 200 {
		return nil, fmt.Errorf("tmail-link: HTTP %d", resp.StatusCode)
	}

	matches := tmailLinkEmailRegex.FindSubmatch(body)
	if matches == nil || len(matches) < 2 {
		return nil, fmt.Errorf("tmail-link: 无法从响应中解析邮箱地址")
	}
	email := string(matches[1])

	/* 步骤2: GET /inbox/{email}/ 获取 csrftoken cookie */
	inboxURL := fmt.Sprintf("%s/inbox/%s/", tmailLinkBase, url.PathEscape(email))
	req2, err := http.NewRequest("GET", inboxURL, nil)
	if err != nil {
		return nil, fmt.Errorf("tmail-link: 创建 inbox 请求失败: %w", err)
	}
	req2.Header.Set("Accept", "text/html")
	req2.Header.Set("User-Agent", getCurrentUA())

	resp2, err := client.Do(req2)
	if err != nil {
		return nil, fmt.Errorf("tmail-link: inbox 请求失败: %w", err)
	}
	io.ReadAll(resp2.Body)
	resp2.Body.Close()

	var csrfToken string
	for _, cookie := range resp2.Cookies() {
		if cookie.Name == "csrftoken" {
			csrfToken = cookie.Value
			break
		}
	}
	if csrfToken == "" {
		return nil, fmt.Errorf("tmail-link: 未获取到 csrftoken")
	}

	return &CreatedMailbox{
		Channel: "tmail-link",
		Email:   email,
		Token:   csrfToken,
	}, nil
}

/**
 * TmailLinkGetEmails — 获取 tmail.link 邮件列表
 */
func TmailLinkGetEmails(token, email string) ([]NormEmail, error) {
	if token == "" {
		return nil, fmt.Errorf("tmail-link: token 为空")
	}
	if email == "" {
		return nil, fmt.Errorf("tmail-link: 邮箱地址为空")
	}

	client := HTTPClient()
	inboxURL := fmt.Sprintf("%s/inbox/%s/", tmailLinkBase, url.PathEscape(email))

	/* 步骤1: GET inbox 页面获取最新 csrftoken */
	req, err := http.NewRequest("GET", inboxURL, nil)
	if err != nil {
		return nil, fmt.Errorf("tmail-link: 创建 GET inbox 请求失败: %w", err)
	}
	req.Header.Set("Accept", "text/html")
	req.Header.Set("User-Agent", getCurrentUA())
	req.Header.Set("Cookie", "csrftoken="+token)

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("tmail-link: GET inbox 请求失败: %w", err)
	}
	io.ReadAll(resp.Body)
	resp.Body.Close()

	freshToken := token
	for _, cookie := range resp.Cookies() {
		if cookie.Name == "csrftoken" && cookie.Value != "" {
			freshToken = cookie.Value
			break
		}
	}

	/* 步骤2: POST inbox 获取 JSON 邮件列表 */
	formData := url.Values{}
	formData.Set("format", "json")
	formData.Set("csrfmiddlewaretoken", freshToken)

	req2, err := http.NewRequest("POST", inboxURL, strings.NewReader(formData.Encode()))
	if err != nil {
		return nil, fmt.Errorf("tmail-link: 创建 POST inbox 请求失败: %w", err)
	}
	req2.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	req2.Header.Set("Accept", "application/json")
	req2.Header.Set("User-Agent", getCurrentUA())
	req2.Header.Set("Cookie", "csrftoken="+freshToken)
	req2.Header.Set("X-CSRFToken", freshToken)
	req2.Header.Set("Referer", inboxURL)

	resp2, err := client.Do(req2)
	if err != nil {
		return nil, fmt.Errorf("tmail-link: POST inbox 请求失败: %w", err)
	}
	defer resp2.Body.Close()

	if resp2.StatusCode < 200 || resp2.StatusCode >= 300 {
		return nil, fmt.Errorf("tmail-link: 邮件请求 HTTP %d", resp2.StatusCode)
	}

	body, err := io.ReadAll(resp2.Body)
	if err != nil {
		return nil, fmt.Errorf("tmail-link: 读取邮件响应失败: %w", err)
	}

	var data struct {
		Messages []json.RawMessage `json:"messages"`
	}
	if err := json.Unmarshal(body, &data); err != nil {
		return nil, fmt.Errorf("tmail-link: 邮件响应解析失败: %w", err)
	}

	if len(data.Messages) == 0 {
		return []NormEmail{}, nil
	}

	out := make([]NormEmail, 0, len(data.Messages))
	for _, raw := range data.Messages {
		var msg map[string]interface{}
		if err := json.Unmarshal(raw, &msg); err != nil {
			continue
		}
		row := map[string]any{
			"id":         msg["key"],
			"from":       firstStr(msg, "sender", "from"),
			"to":         email,
			"subject":    msg["subject"],
			"text":       firstStr(msg, "body", "text"),
			"html":       firstStr(msg, "html", "body"),
			"created_at": firstStr(msg, "date", "created_at"),
		}
		out = append(out, NormalizeMap(row, email))
	}
	return out, nil
}
