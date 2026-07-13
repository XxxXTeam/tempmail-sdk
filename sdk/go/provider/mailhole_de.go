package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"regexp"

	http "github.com/bogdanfinn/fhttp"
)

/**
 * MailholeDe — https://mailhole.de
 * 公共临时邮箱，无需认证
 * 流程:
 *   1. GET /api/random → HTML 包含随机邮箱地址
 *   2. GET /json/{email} → JSON 数组获取邮件
 * Token: 邮箱地址本身
 */

const mailholeDeBase = "https://mailhole.de"

var mailholeDeEmailRegex = regexp.MustCompile(`([a-z0-9.]+@mailhole\.de)`)

/**
 * MailholeDeGenerate — 创建 mailhole.de 临时邮箱
 */
func MailholeDeGenerate() (*CreatedMailbox, error) {
	client := HTTPClient()

	req, err := http.NewRequest("GET", mailholeDeBase+"/api/random", nil)
	if err != nil {
		return nil, fmt.Errorf("mailhole-de: 创建请求失败: %w", err)
	}
	req.Header.Set("Accept", "text/html")
	req.Header.Set("User-Agent", getCurrentUA())

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("mailhole-de: 请求失败: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("mailhole-de: 读取响应失败: %w", err)
	}

	if resp.StatusCode != 200 {
		return nil, fmt.Errorf("mailhole-de: HTTP %d", resp.StatusCode)
	}

	matches := mailholeDeEmailRegex.FindSubmatch(body)
	if matches == nil || len(matches) < 2 {
		return nil, fmt.Errorf("mailhole-de: 无法从响应中解析邮箱地址")
	}

	email := string(matches[1])
	return &CreatedMailbox{
		Channel: "mailhole-de",
		Email:   email,
		Token:   email,
	}, nil
}

/**
 * MailholeDeGetEmails — 获取 mailhole.de 邮件列表
 */
func MailholeDeGetEmails(token, email string) ([]NormEmail, error) {
	addr := token
	if addr == "" {
		addr = email
	}
	if addr == "" {
		return nil, fmt.Errorf("mailhole-de: 缺少邮箱地址")
	}

	client := HTTPClient()
	u := fmt.Sprintf("%s/json/%s", mailholeDeBase, addr)
	req, err := http.NewRequest("GET", u, nil)
	if err != nil {
		return nil, fmt.Errorf("mailhole-de: 创建邮件请求失败: %w", err)
	}
	req.Header.Set("Accept", "application/json")
	req.Header.Set("User-Agent", getCurrentUA())

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("mailhole-de: 邮件请求失败: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("mailhole-de: 邮件请求 HTTP %d", resp.StatusCode)
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("mailhole-de: 读取邮件响应失败: %w", err)
	}

	if len(body) == 0 || string(body) == "[]" {
		return []NormEmail{}, nil
	}

	var messages []map[string]interface{}
	if err := json.Unmarshal(body, &messages); err != nil {
		return nil, fmt.Errorf("mailhole-de: 邮件响应解析失败: %w", err)
	}

	out := make([]NormEmail, 0, len(messages))
	for _, msg := range messages {
		row := map[string]any{
			"id":         msg["id"],
			"from":       firstStr(msg, "sender", "from"),
			"to":         addr,
			"subject":    msg["subject"],
			"text":       firstStr(msg, "body", "text"),
			"html":       firstStr(msg, "html", "body"),
			"created_at": firstStr(msg, "date", "received"),
		}
		out = append(out, NormalizeMap(row, addr))
	}
	return out, nil
}

func firstStr(m map[string]interface{}, keys ...string) string {
	for _, k := range keys {
		if v, ok := m[k]; ok {
			if s, ok := v.(string); ok && s != "" {
				return s
			}
		}
	}
	return ""
}
