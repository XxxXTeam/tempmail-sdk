package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"sort"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

/**
 * TempMailNow — https://temp-mail.now
 * 基于 Session Cookie 的临时邮箱服务
 * 流程:
 *   1. GET /en/ 获取会话 Cookie（Set-Cookie）
 *   2. POST /change_email 携带 Cookie → 返回 {"new_email":"...@dpmurt.my"}
 *   3. GET /fetch_emails 携带 Cookie → 返回 {"emails":[...],"remaining_time":...}
 * Token 存储: 会话 Cookie 字符串
 */

const tempMailNowBase = "https://temp-mail.now"

/**
 * tempMailNowChangeEmailResponse — 更换/创建邮箱的响应结构
 */
type tempMailNowChangeEmailResponse struct {
	NewEmail string `json:"new_email"`
}

/**
 * tempMailNowFetchEmailsResponse — 获取邮件列表的响应结构
 */
type tempMailNowFetchEmailsResponse struct {
	Emails []struct {
		ID      string `json:"id"`
		From    string `json:"from"`
		Subject string `json:"subject"`
		Body    string `json:"body"`
		Date    string `json:"date"`
	} `json:"emails"`
	RemainingTime int `json:"remaining_time"`
}

/**
 * tempMailNowMergeCookies — 合并已有 Cookie 字符串与响应中的 Set-Cookie
 * 新 Cookie 覆盖同名旧值，最终按 key 排序拼接
 */
func tempMailNowMergeCookies(prev string, cookies []*http.Cookie) string {
	m := make(map[string]string)
	for _, part := range strings.Split(prev, ";") {
		part = strings.TrimSpace(part)
		if part == "" {
			continue
		}
		i := strings.Index(part, "=")
		if i <= 0 || i >= len(part)-1 {
			continue
		}
		k := strings.TrimSpace(part[:i])
		v := strings.TrimSpace(part[i+1:])
		if k != "" {
			m[k] = v
		}
	}
	for _, c := range cookies {
		if c == nil || c.Name == "" {
			continue
		}
		m[c.Name] = c.Value
	}
	keys := make([]string, 0, len(m))
	for k := range m {
		keys = append(keys, k)
	}
	sort.Strings(keys)
	parts := make([]string, 0, len(keys))
	for _, k := range keys {
		parts = append(parts, k+"="+m[k])
	}
	return strings.Join(parts, "; ")
}

/**
 * TempMailNowGenerate — 创建 temp-mail.now 临时邮箱
 * 流程:
 *   1. GET /en/ 获取会话 Cookie
 *   2. POST /change_email 携带 Cookie 创建新邮箱
 *   3. 将合并后的 Cookie 字符串作为 Token 存储
 */
func TempMailNowGenerate() (*CreatedMailbox, error) {
	client := HTTPClient()

	/* 第一步：GET /en/ 获取会话 Cookie */
	req, err := http.NewRequest("GET", tempMailNowBase+"/en/", nil)
	if err != nil {
		return nil, fmt.Errorf("temp-mail-now: 创建初始请求失败: %w", err)
	}
	req.Header.Set("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8")
	req.Header.Set("Accept-Language", "en-US,en;q=0.9")
	req.Header.Set("User-Agent", getCurrentUA())

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("temp-mail-now: 初始请求失败: %w", err)
	}
	defer resp.Body.Close()
	_, _ = io.Copy(io.Discard, resp.Body)

	if resp.StatusCode < 200 || resp.StatusCode >= 400 {
		return nil, fmt.Errorf("temp-mail-now: 初始请求 HTTP %d", resp.StatusCode)
	}

	cookieHdr := tempMailNowMergeCookies("", resp.Cookies())
	if cookieHdr == "" {
		return nil, fmt.Errorf("temp-mail-now: 未获取到会话 Cookie")
	}

	/* 第二步：POST /change_email 创建新邮箱 */
	req2, err := http.NewRequest("POST", tempMailNowBase+"/change_email", nil)
	if err != nil {
		return nil, fmt.Errorf("temp-mail-now: 创建 change_email 请求失败: %w", err)
	}
	req2.Header.Set("Accept", "application/json, text/plain, */*")
	req2.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	req2.Header.Set("Cookie", cookieHdr)
	req2.Header.Set("Referer", tempMailNowBase+"/en/")
	req2.Header.Set("User-Agent", getCurrentUA())

	resp2, err := client.Do(req2)
	if err != nil {
		return nil, fmt.Errorf("temp-mail-now: change_email 请求失败: %w", err)
	}
	defer resp2.Body.Close()

	body, err := io.ReadAll(resp2.Body)
	if err != nil {
		return nil, fmt.Errorf("temp-mail-now: 读取 change_email 响应失败: %w", err)
	}

	if resp2.StatusCode < 200 || resp2.StatusCode >= 300 {
		return nil, fmt.Errorf("temp-mail-now: change_email HTTP %d", resp2.StatusCode)
	}

	/* 合并第二次请求可能返回的新 Cookie */
	cookieHdr = tempMailNowMergeCookies(cookieHdr, resp2.Cookies())

	var data tempMailNowChangeEmailResponse
	if err := json.Unmarshal(body, &data); err != nil {
		return nil, fmt.Errorf("temp-mail-now: change_email 响应解析失败: %w", err)
	}

	email := strings.TrimSpace(data.NewEmail)
	if email == "" || !strings.Contains(email, "@") {
		return nil, fmt.Errorf("temp-mail-now: 无效的邮箱响应")
	}

	return &CreatedMailbox{
		Channel: "temp-mail-now",
		Email:   email,
		Token:   cookieHdr,
	}, nil
}

/**
 * TempMailNowGetEmails — 获取 temp-mail.now 邮件列表
 * 使用存储的会话 Cookie 调用 GET /fetch_emails
 * 每条邮件经过 NormalizeMap 归一化处理
 */
func TempMailNowGetEmails(token, email string) ([]NormEmail, error) {
	if token == "" {
		return nil, fmt.Errorf("temp-mail-now: 会话 Cookie 为空")
	}

	client := HTTPClient()

	req, err := http.NewRequest("GET", tempMailNowBase+"/fetch_emails", nil)
	if err != nil {
		return nil, fmt.Errorf("temp-mail-now: 创建 fetch_emails 请求失败: %w", err)
	}
	req.Header.Set("Accept", "application/json, text/plain, */*")
	req.Header.Set("Cookie", token)
	req.Header.Set("Referer", tempMailNowBase+"/en/")
	req.Header.Set("User-Agent", getCurrentUA())

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("temp-mail-now: fetch_emails 请求失败: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("temp-mail-now: fetch_emails HTTP %d", resp.StatusCode)
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("temp-mail-now: 读取 fetch_emails 响应失败: %w", err)
	}

	var data tempMailNowFetchEmailsResponse
	if err := json.Unmarshal(body, &data); err != nil {
		return nil, fmt.Errorf("temp-mail-now: fetch_emails 响应解析失败: %w", err)
	}

	out := make([]NormEmail, 0, len(data.Emails))
	for _, msg := range data.Emails {
		row := map[string]any{
			"id":      msg.ID,
			"from":    msg.From,
			"to":      email,
			"subject": msg.Subject,
			"html":    msg.Body,
			"date":    msg.Date,
		}
		out = append(out, NormalizeMap(row, email))
	}
	return out, nil
}
