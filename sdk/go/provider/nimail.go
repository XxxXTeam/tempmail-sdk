package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"math/rand"
	"net/url"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

// nimail.cn：简单 POST API 临时邮箱，无需认证

const nimailBaseURL = "https://www.nimail.cn"

func nimailRandomLocal(length int) string {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	buf := make([]byte, length)
	for i := range buf {
		buf[i] = chars[rand.Intn(len(chars))]
	}
	return string(buf)
}

// NimailGenerate 创建 nimail.cn 临时邮箱
func NimailGenerate() (*CreatedMailbox, error) {
	client := HTTPClient()
	name := nimailRandomLocal(10)
	email := fmt.Sprintf("%s@nimail.cn", name)

	body := fmt.Sprintf("mail=%s", url.QueryEscape(email))
	req, err := http.NewRequest("POST", nimailBaseURL+"/api/applymail", strings.NewReader(body))
	if err != nil {
		return nil, err
	}
	req.Header.Set("User-Agent", getCurrentUA())
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	req.Header.Set("Origin", nimailBaseURL)
	req.Header.Set("Referer", nimailBaseURL+"/")

	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if err := CheckHTTPStatus(resp, "nimail generate"); err != nil {
		return nil, err
	}

	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	var data struct {
		Success string `json:"success"`
		User    string `json:"user"`
	}
	if err := json.Unmarshal(raw, &data); err != nil {
		return nil, fmt.Errorf("nimail: 返回非 JSON: %s", string(raw[:min(len(raw), 120)]))
	}
	if data.Success != "true" || data.User == "" {
		return nil, fmt.Errorf("nimail: 创建邮箱失败 %s", string(raw))
	}

	return &CreatedMailbox{
		Channel: "nimail",
		Email:   data.User,
		Token:   data.User,
	}, nil
}

// NimailGetEmails 获取 nimail.cn 邮件列表
func NimailGetEmails(email string) ([]NormEmail, error) {
	client := HTTPClient()

	body := fmt.Sprintf("mail=%s&time=0", url.QueryEscape(email))
	req, err := http.NewRequest("POST", nimailBaseURL+"/api/getmails", strings.NewReader(body))
	if err != nil {
		return nil, err
	}
	req.Header.Set("User-Agent", getCurrentUA())
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	req.Header.Set("Origin", nimailBaseURL)
	req.Header.Set("Referer", nimailBaseURL+"/")

	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if err := CheckHTTPStatus(resp, "nimail getEmails"); err != nil {
		return nil, err
	}

	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	var data struct {
		Success string            `json:"success"`
		Mail    []json.RawMessage `json:"mail"`
	}
	if err := json.Unmarshal(raw, &data); err != nil {
		return nil, err
	}
	if data.Success != "true" || len(data.Mail) == 0 {
		return []NormEmail{}, nil
	}

	return NormalizeRawMessages(data.Mail, email)
}
