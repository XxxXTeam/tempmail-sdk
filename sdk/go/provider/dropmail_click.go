package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"net/url"

	http "github.com/bogdanfinn/fhttp"
)

// dropmail.click：独立后端临时邮箱（MX 自研 mail.dropmail.click，与 dropmail.me 无关）
// 免注册、无鉴权、纯 REST API；邮箱有效期 10 分钟。

const dropmailClickBaseURL = "https://dropmail.click"

// DropmailClickGenerate 创建 dropmail.click 临时邮箱
// POST /api/v1/public/mailbox → {address, created_at, expires_at}
func DropmailClickGenerate() (*CreatedMailbox, error) {
	client := HTTPClient()
	req, err := http.NewRequest("POST", dropmailClickBaseURL+"/api/v1/public/mailbox", nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("User-Agent", getCurrentUA())
	req.Header.Set("Accept", "application/json")

	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if err := CheckHTTPStatus(resp, "dropmail-click generate"); err != nil {
		return nil, err
	}
	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	var data struct {
		Address   string `json:"address"`
		ExpiresAt string `json:"expires_at"`
	}
	if err := json.Unmarshal(raw, &data); err != nil {
		return nil, fmt.Errorf("dropmail-click: 无效响应 %s", string(raw))
	}
	if data.Address == "" {
		return nil, fmt.Errorf("dropmail-click: 缺少 address 字段")
	}
	return &CreatedMailbox{
		Channel:   "dropmail-click",
		Email:     data.Address,
		Token:     data.Address,
		ExpiresAt: data.ExpiresAt,
	}, nil
}

// DropmailClickGetEmails 获取 dropmail.click 邮件列表
// GET /api/v1/public/mailbox/{email} → {messages:[{id, address, from, subject, text, html, ...}]}
func DropmailClickGetEmails(email string) ([]NormEmail, error) {
	if email == "" {
		return nil, fmt.Errorf("dropmail-click: 缺少邮箱地址")
	}
	client := HTTPClient()
	reqURL := fmt.Sprintf("%s/api/v1/public/mailbox/%s", dropmailClickBaseURL, url.PathEscape(email))
	req, err := http.NewRequest("GET", reqURL, nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("User-Agent", getCurrentUA())
	req.Header.Set("Accept", "application/json")

	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if err := CheckHTTPStatus(resp, "dropmail-click getEmails"); err != nil {
		return nil, err
	}
	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	var data struct {
		Messages []json.RawMessage `json:"messages"`
	}
	if err := json.Unmarshal(raw, &data); err != nil {
		return nil, err
	}
	if len(data.Messages) == 0 {
		return []NormEmail{}, nil
	}
	return NormalizeRawMessages(data.Messages, email)
}
