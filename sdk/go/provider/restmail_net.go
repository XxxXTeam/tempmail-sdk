package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"math/rand"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

const (
	restmailNetBase   = "https://restmail.net"
	restmailNetDomain = "restmail.net"
)

/*
 * restmailNetRandomUsername 生成 10-12 位随机字母数字用户名
 */
func restmailNetRandomUsername() string {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	length := 10 + rand.Intn(3) // 10-12
	var b strings.Builder
	for i := 0; i < length; i++ {
		b.WriteByte(chars[rand.Intn(len(chars))])
	}
	return b.String()
}

/*
 * restmailNetGetJSON 使用 SDK 共享客户端发起 GET 请求并返回响应体字节
 */
func restmailNetGetJSON(u string) ([]byte, int, error) {
	req, err := http.NewRequest(http.MethodGet, u, nil)
	if err != nil {
		return nil, 0, err
	}
	ua := getCurrentUA()
	if ua != "" {
		req.Header.Set("User-Agent", ua)
	}
	req.Header.Set("Accept", "application/json")
	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, 0, err
	}
	defer resp.Body.Close()
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, resp.StatusCode, err
	}
	return body, resp.StatusCode, nil
}

/*
 * RestmailNetGenerate 创建 restmail.net 临时邮箱
 * 公共收件箱模式（Mozilla 开源项目），随机生成用户名即可收信，无需注册
 * Token 存空字符串
 */
func RestmailNetGenerate() (*CreatedMailbox, error) {
	username := restmailNetRandomUsername()
	email := username + "@" + restmailNetDomain
	return &CreatedMailbox{
		Channel: "restmail-net",
		Email:   email,
		Token:   "",
	}, nil
}

/*
 * RestmailNetGetEmails 获取 restmail.net 邮件列表
 * GET /mail/{username} 返回 JSON 数组
 * @param token - 不使用（空字符串）
 * @param email - 邮箱地址
 */
func RestmailNetGetEmails(token, email string) ([]NormEmail, error) {
	address := strings.TrimSpace(email)
	if address == "" {
		return nil, fmt.Errorf("restmail-net: 缺少邮箱地址")
	}

	/* 从邮箱地址中提取 username */
	username := address
	if idx := strings.Index(address, "@"); idx > 0 {
		username = address[:idx]
	}

	/* 获取邮件列表 */
	u := fmt.Sprintf("%s/mail/%s", restmailNetBase, username)
	body, status, err := restmailNetGetJSON(u)
	if err != nil {
		return nil, err
	}
	if status < 200 || status >= 300 {
		return nil, fmt.Errorf("restmail-net: 获取邮件列表失败 http %d", status)
	}

	/* 解析 JSON 数组 */
	var messages []map[string]interface{}
	if err := json.Unmarshal(body, &messages); err != nil {
		return nil, fmt.Errorf("restmail-net: 解析响应失败: %w", err)
	}

	if len(messages) == 0 {
		return []NormEmail{}, nil
	}

	out := make([]NormEmail, 0, len(messages))
	for _, msg := range messages {
		raw := restmailNetFlatten(msg, address)
		out = append(out, NormalizeMap(raw, address))
	}
	return out, nil
}

/*
 * restmailNetFlatten 将 restmail.net 原始消息结构展平为 NormalizeMap 可处理的 map
 * from: 优先取 from[0].address，否则取 headers.from
 * subject: 取 subject 字段
 * body: 优先 html，其次 text
 * date: 取 receivedAt
 */
func restmailNetFlatten(msg map[string]interface{}, recipient string) map[string]interface{} {
	flat := map[string]interface{}{
		"to": recipient,
	}

	/* 提取 from */
	if fromArr, ok := msg["from"].([]interface{}); ok && len(fromArr) > 0 {
		if fromObj, ok := fromArr[0].(map[string]interface{}); ok {
			if addr, ok := fromObj["address"].(string); ok && addr != "" {
				flat["from"] = addr
			}
		}
	}
	if _, exists := flat["from"]; !exists {
		/* 回退到 headers.from */
		if headers, ok := msg["headers"].(map[string]interface{}); ok {
			if hFrom, ok := headers["from"].(string); ok {
				flat["from"] = hFrom
			}
		}
	}

	/* 提取 subject */
	if subject, ok := msg["subject"].(string); ok {
		flat["subject"] = subject
	}

	/* 提取 body：优先 html，其次 text */
	if html, ok := msg["html"].(string); ok && html != "" {
		flat["html"] = html
	}
	if text, ok := msg["text"].(string); ok && text != "" {
		flat["text"] = text
	}

	/* 提取日期 */
	if receivedAt, ok := msg["receivedAt"].(string); ok {
		flat["date"] = receivedAt
	}

	return flat
}
