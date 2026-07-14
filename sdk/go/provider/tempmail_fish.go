package provider

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/url"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

const tempmailFishBase = "https://api.tempmail.fish"

/* tempmailFishNewResponse 创建邮箱响应：{"email":"xxx","authKey":"xxx","emails":[]} */
type tempmailFishNewResponse struct {
	Email   string `json:"email"`
	AuthKey string `json:"authKey"`
}

/* tempmailFishToken 存入 CreatedMailbox.Token 的 JSON 结构，getEmails 时解析出 authKey */
type tempmailFishToken struct {
	Email   string `json:"email"`
	AuthKey string `json:"authKey"`
}

/* tempmailFishMessage tempmail.fish 邮件字段，textBody 常内嵌 HTML */
type tempmailFishMessage struct {
	To          string        `json:"to"`
	From        string        `json:"from"`
	Subject     string        `json:"subject"`
	TextBody    string        `json:"textBody"`
	HTMLBody    string        `json:"htmlBody"`
	Attachments []interface{} `json:"attachments"`
	Timestamp   float64       `json:"timestamp"`
}

/*
 * tempmailFishFlatten 将 tempmail.fish 原始邮件映射为标准化中间 map
 * textBody 中常内嵌 HTML，交由 NormalizeMap 自动识别归位；timestamp 为毫秒时间戳
 */
func tempmailFishFlatten(raw tempmailFishMessage, recipient string) map[string]any {
	to := raw.To
	if strings.TrimSpace(to) == "" {
		to = recipient
	}
	m := map[string]any{
		"from":        raw.From,
		"to":          to,
		"subject":     raw.Subject,
		"text":        raw.TextBody,
		"html":        raw.HTMLBody,
		"attachments": raw.Attachments,
	}
	if raw.Timestamp > 0 {
		m["timestamp"] = raw.Timestamp
	}
	return m
}

/*
 * TempmailFishGenerate 创建 tempmail.fish 临时邮箱
 * API: POST /emails/new-email → {"email":"xxx","authKey":"xxx","emails":[]}
 * Token 存储为 JSON 字符串 {"email":"...","authKey":"..."}，供 getEmails 解析
 */
func TempmailFishGenerate() (*CreatedMailbox, error) {
	req, err := http.NewRequest(http.MethodPost, tempmailFishBase+"/emails/new-email", bytes.NewReader(nil))
	if err != nil {
		return nil, err
	}
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Accept", "application/json")
	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("tempmail-fish: 创建邮箱失败 http %d", resp.StatusCode)
	}

	var data tempmailFishNewResponse
	if err := json.Unmarshal(body, &data); err != nil {
		return nil, err
	}
	email := strings.TrimSpace(data.Email)
	authKey := strings.TrimSpace(data.AuthKey)
	if email == "" || !strings.Contains(email, "@") || authKey == "" {
		return nil, fmt.Errorf("tempmail-fish: 创建邮箱响应无效")
	}

	tokenJSON, err := json.Marshal(tempmailFishToken{Email: email, AuthKey: authKey})
	if err != nil {
		return nil, err
	}
	return &CreatedMailbox{
		Channel: "tempmail-fish",
		Email:   email,
		Token:   string(tokenJSON),
	}, nil
}

/*
 * TempmailFishGetEmails 获取 tempmail.fish 邮件列表
 * API: GET /emails/emails?emailAddress={email}，请求头 Authorization: {authKey}（不带 Bearer 前缀）
 * @param token - JSON 字符串，包含 email 和 authKey
 * @param email - 邮箱地址
 */
func TempmailFishGetEmails(token, email string) ([]NormEmail, error) {
	token = strings.TrimSpace(token)
	if token == "" {
		return nil, fmt.Errorf("tempmail-fish: token 不能为空")
	}

	var parsed tempmailFishToken
	if err := json.Unmarshal([]byte(token), &parsed); err != nil {
		return nil, fmt.Errorf("tempmail-fish: token 格式无效")
	}
	address := strings.TrimSpace(parsed.Email)
	if address == "" {
		address = strings.TrimSpace(email)
	}
	authKey := strings.TrimSpace(parsed.AuthKey)
	if address == "" || authKey == "" {
		return nil, fmt.Errorf("tempmail-fish: token 缺少 email 或 authKey")
	}

	u := fmt.Sprintf("%s/emails/emails?emailAddress=%s", tempmailFishBase, url.QueryEscape(address))
	req, err := http.NewRequest(http.MethodGet, u, nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("Accept", "application/json")
	req.Header.Set("Authorization", authKey)
	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("tempmail-fish: 获取邮件失败 http %d", resp.StatusCode)
	}

	/* 响应通常是邮件数组，个别情况可能包裹在 {"emails":[...]} 中 */
	var rows []tempmailFishMessage
	if err := json.Unmarshal(body, &rows); err != nil {
		var wrapped struct {
			Emails []tempmailFishMessage `json:"emails"`
		}
		if err2 := json.Unmarshal(body, &wrapped); err2 != nil {
			return nil, err
		}
		rows = wrapped.Emails
	}

	out := make([]NormEmail, 0, len(rows))
	for _, row := range rows {
		out = append(out, NormalizeMap(tempmailFishFlatten(row, address), address))
	}
	return out, nil
}
