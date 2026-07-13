package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"math/rand"
	"net/url"

	http "github.com/bogdanfinn/fhttp"
)

// mailmomy.com：免注册、无鉴权、纯 GET JSON API 临时邮箱。
// token = email 无状态模式；MX 自研 mail.mailmomy.com。

const (
	mailmomyBaseURL = "https://mailmomy.com"
	// 规格指定的固定 UA，与 npm 端一致
	mailmomyUA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36"
)

func mailmomyRandomLocal(length int) string {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	buf := make([]byte, length)
	for i := range buf {
		buf[i] = chars[rand.Intn(len(chars))]
	}
	return string(buf)
}

// mailmomyPickDomain 拉取当前可用域名池并随机选取。
// GET /api/domains/active → JSON 字符串数组；请求失败或列表为空时回退 mailmomy.com。
func mailmomyPickDomain() string {
	const fallback = "mailmomy.com"
	client := HTTPClient()
	req, err := http.NewRequest("GET", mailmomyBaseURL+"/api/domains/active", nil)
	if err != nil {
		return fallback
	}
	req.Header.Set("User-Agent", mailmomyUA)
	req.Header.Set("Accept", "application/json")

	resp, err := client.Do(req)
	if err != nil {
		return fallback
	}
	defer resp.Body.Close()
	if resp.StatusCode >= 400 {
		return fallback
	}

	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return fallback
	}

	var list []string
	if err := json.Unmarshal(raw, &list); err != nil {
		return fallback
	}
	usable := make([]string, 0, len(list))
	for _, d := range list {
		if d != "" {
			usable = append(usable, d)
		}
	}
	if len(usable) == 0 {
		return fallback
	}
	return usable[rand.Intn(len(usable))]
}

// MailmomyGenerate 创建 mailmomy.com 临时邮箱
func MailmomyGenerate() (*CreatedMailbox, error) {
	domain := mailmomyPickDomain()
	email := fmt.Sprintf("%s@%s", mailmomyRandomLocal(10), domain)
	return &CreatedMailbox{
		Channel: "mailmomy",
		Email:   email,
		Token:   email,
	}, nil
}

// mailmomyMessage mailmomy 邮件消息
type mailmomyMessage struct {
	ID         string `json:"id"`
	Recipient  string `json:"recipient"`
	From       string `json:"from"`
	Subject    string `json:"subject"`
	Message    string `json:"message"`
	BodyText   string `json:"bodyText"`
	ReceivedAt string `json:"receivedAt"`
}

// MailmomyGetEmails 获取 mailmomy.com 邮件列表
// GET /api/mail/messages?to=<email>&page=1&limit=20
// 返回 {emails:[{id,recipient,from,subject,message,bodyText,receivedAt}], total, page, limit, pages}
func MailmomyGetEmails(email string) ([]NormEmail, error) {
	if email == "" {
		return nil, fmt.Errorf("mailmomy: 缺少邮箱地址")
	}

	client := HTTPClient()
	u := fmt.Sprintf("%s/api/mail/messages?to=%s&page=1&limit=20", mailmomyBaseURL, url.QueryEscape(email))
	req, err := http.NewRequest("GET", u, nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("User-Agent", mailmomyUA)
	req.Header.Set("Accept", "application/json")

	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if err := CheckHTTPStatus(resp, "mailmomy getEmails"); err != nil {
		return nil, err
	}

	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	var data struct {
		Emails []mailmomyMessage `json:"emails"`
	}
	if err := json.Unmarshal(raw, &data); err != nil {
		return nil, err
	}
	if len(data.Emails) == 0 {
		return []NormEmail{}, nil
	}

	emails := make([]NormEmail, 0, len(data.Emails))
	for _, msg := range data.Emails {
		to := msg.Recipient
		if to == "" {
			to = email
		}
		text := msg.BodyText
		if text == "" {
			text = msg.Message
		}
		flat := map[string]interface{}{
			"id":      msg.ID,
			"from":    msg.From,
			"to":      to,
			"subject": msg.Subject,
			"text":    text,
			"html":    msg.Message,
			"date":    msg.ReceivedAt,
			"isRead":  false,
		}
		emails = append(emails, NormalizeMap(flat, email))
	}
	return emails, nil
}
