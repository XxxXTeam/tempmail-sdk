package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"math/rand"
	"net/url"

	http "github.com/bogdanfinn/fhttp"
)

// freecustom.email：免注册临时邮箱，任意 local part @ 可用域名即时可收信。
// token = email；读信时动态获取匿名 JWT，无需持久化。

const (
	freecustomSiteURL   = "https://www.freecustom.email"
	freecustomReferer   = "https://www.freecustom.email/en"
	freecustomDomainURL = "https://api2.freecustom.email/domains"
	// 规格指定的固定 UA，与 npm 端一致
	freecustomUA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36"
)

func freecustomRandomLocal(length int) string {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	buf := make([]byte, length)
	for i := range buf {
		buf[i] = chars[rand.Intn(len(chars))]
	}
	return string(buf)
}

// freecustomDomain 域名列表项
type freecustomDomain struct {
	Domain       string `json:"domain"`
	Tier         string `json:"tier"`
	ExpiringSoon bool   `json:"expiring_soon"`
}

// freecustomPickDomain 挑选一个当前可收信的域名。
// /domains 无需鉴权，优先选择 tier=="free" 且未过期（expiring_soon 非 true）的域名；
// 若全部标记过期则退回全量列表随机。
func freecustomPickDomain() (string, error) {
	client := HTTPClient()
	req, err := http.NewRequest("GET", freecustomDomainURL, nil)
	if err != nil {
		return "", err
	}
	req.Header.Set("User-Agent", freecustomUA)
	req.Header.Set("Accept", "application/json")
	req.Header.Set("Referer", freecustomReferer)

	resp, err := client.Do(req)
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()
	if err := CheckHTTPStatus(resp, "freecustom domains"); err != nil {
		return "", err
	}

	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", err
	}

	var data struct {
		Success bool               `json:"success"`
		Data    []freecustomDomain `json:"data"`
	}
	if err := json.Unmarshal(raw, &data); err != nil {
		return "", fmt.Errorf("freecustom: 域名列表非 JSON: %s", string(raw[:min(len(raw), 120)]))
	}
	if len(data.Data) == 0 {
		return "", fmt.Errorf("freecustom: 域名列表为空")
	}

	// 优先未过期的 free 域名；若无则退回全量列表
	usable := make([]freecustomDomain, 0, len(data.Data))
	for _, d := range data.Data {
		if d.Tier == "free" && !d.ExpiringSoon {
			usable = append(usable, d)
		}
	}
	pool := usable
	if len(pool) == 0 {
		pool = data.Data
	}
	return pool[rand.Intn(len(pool))].Domain, nil
}

// freecustomFetchAuthToken 获取匿名访问令牌（JWT，有效期约 2 小时）
// POST /api/auth → { token }
func freecustomFetchAuthToken() (string, error) {
	client := HTTPClient()
	req, err := http.NewRequest("POST", freecustomSiteURL+"/api/auth", nil)
	if err != nil {
		return "", err
	}
	req.Header.Set("User-Agent", freecustomUA)
	req.Header.Set("Accept", "application/json")
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Referer", freecustomReferer)

	resp, err := client.Do(req)
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()
	if err := CheckHTTPStatus(resp, "freecustom auth"); err != nil {
		return "", err
	}

	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", err
	}

	var data struct {
		Token string `json:"token"`
	}
	if err := json.Unmarshal(raw, &data); err != nil {
		return "", fmt.Errorf("freecustom: 令牌响应非 JSON")
	}
	if data.Token == "" {
		return "", fmt.Errorf("freecustom: 令牌响应无效")
	}
	return data.Token, nil
}

// FreecustomGenerate 创建 freecustom.email 临时邮箱
func FreecustomGenerate() (*CreatedMailbox, error) {
	domain, err := freecustomPickDomain()
	if err != nil {
		return nil, err
	}
	email := fmt.Sprintf("%s@%s", freecustomRandomLocal(10), domain)
	return &CreatedMailbox{
		Channel: "freecustom",
		Email:   email,
		Token:   email,
	}, nil
}

// freecustomMessage 邮件消息（列表项 + 单封正文字段）
type freecustomMessage struct {
	ID      string `json:"id"`
	From    string `json:"from"`
	To      string `json:"to"`
	Subject string `json:"subject"`
	Date    string `json:"date"`
	HTML    string `json:"html"`
	Text    string `json:"text"`
}

// freecustomFetchMessage 补全单封邮件正文；失败时返回 nil（由调用方退回列表元数据）
func freecustomFetchMessage(email, msgID string, authHeaders map[string]string) *freecustomMessage {
	client := HTTPClient()
	u := fmt.Sprintf("%s/api/public-mailbox?fullMailboxId=%s&messageId=%s",
		freecustomSiteURL, url.QueryEscape(email), url.QueryEscape(msgID))
	req, err := http.NewRequest("GET", u, nil)
	if err != nil {
		return nil
	}
	for k, v := range authHeaders {
		req.Header.Set(k, v)
	}
	resp, err := client.Do(req)
	if err != nil {
		return nil
	}
	defer resp.Body.Close()
	if resp.StatusCode >= 400 {
		return nil
	}
	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil
	}
	var data struct {
		Success bool               `json:"success"`
		Data    *freecustomMessage `json:"data"`
	}
	if err := json.Unmarshal(raw, &data); err != nil {
		return nil
	}
	if !data.Success || data.Data == nil {
		return nil
	}
	return data.Data
}

// FreecustomGetEmails 获取 freecustom.email 邮件列表
//  1. POST /api/auth 取 JWT
//  2. GET /api/public-mailbox?fullMailboxId=<email> 取邮件元数据列表
//  3. 对每封 GET /api/public-mailbox?fullMailboxId=<email>&messageId=<id> 补全正文
func FreecustomGetEmails(email string) ([]NormEmail, error) {
	if email == "" {
		return nil, fmt.Errorf("freecustom: 缺少邮箱地址")
	}

	jwt, err := freecustomFetchAuthToken()
	if err != nil {
		return nil, err
	}
	authHeaders := map[string]string{
		"User-Agent":    freecustomUA,
		"Accept":        "application/json",
		"Referer":       freecustomReferer,
		"Authorization": "Bearer " + jwt,
		"x-fce-client":  "web-client",
	}

	client := HTTPClient()
	listURL := fmt.Sprintf("%s/api/public-mailbox?fullMailboxId=%s", freecustomSiteURL, url.QueryEscape(email))
	req, err := http.NewRequest("GET", listURL, nil)
	if err != nil {
		return nil, err
	}
	for k, v := range authHeaders {
		req.Header.Set(k, v)
	}
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if err := CheckHTTPStatus(resp, "freecustom getEmails"); err != nil {
		return nil, err
	}

	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	var data struct {
		Success bool                `json:"success"`
		Data    []freecustomMessage `json:"data"`
	}
	if err := json.Unmarshal(raw, &data); err != nil {
		return nil, err
	}
	if !data.Success || len(data.Data) == 0 {
		return []NormEmail{}, nil
	}

	emails := make([]NormEmail, 0, len(data.Data))
	for _, item := range data.Data {
		if item.ID == "" {
			continue
		}
		full := item
		// 补全正文，失败则退回列表元数据
		if body := freecustomFetchMessage(email, item.ID, authHeaders); body != nil {
			full = *body
		}

		to := full.To
		if to == "" {
			to = email
		}
		flat := map[string]interface{}{
			"id":      full.ID,
			"from":    full.From,
			"to":      to,
			"subject": full.Subject,
			"text":    full.Text,
			"html":    full.HTML,
			"date":    full.Date,
			"isRead":  false,
		}
		emails = append(emails, NormalizeMap(flat, email))
	}
	return emails, nil
}
