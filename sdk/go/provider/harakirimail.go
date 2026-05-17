package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"math/rand"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

const harakirimailBase = "https://harakirimail.com"

/* harakirimailDefaultHeaders 设置通用请求头 */
func harakirimailDefaultHeaders(req *http.Request) {
	req.Header.Set("Accept", "application/json, text/plain, */*")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0")
}

/* 随机生成 12 位字母数字字符串作为收件箱名 */
func harakirimailRandomName() string {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	b := make([]byte, 12)
	for i := range b {
		b[i] = chars[rand.Intn(len(chars))]
	}
	return string(b)
}

/* HarakirimailGenerate 创建 harakirimail 临时邮箱 */
func HarakirimailGenerate() (*CreatedMailbox, error) {
	name := harakirimailRandomName()
	email := fmt.Sprintf("%s@harakirimail.com", name)

	/* 可选：调用收件箱接口验证地址可用 */
	u := fmt.Sprintf("%s/api/v1/inbox/%s", harakirimailBase, name)
	req, err := http.NewRequest("GET", u, nil)
	if err != nil {
		return nil, err
	}
	harakirimailDefaultHeaders(req)

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	io.ReadAll(resp.Body)

	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("harakirimail: 验证收件箱失败 %d", resp.StatusCode)
	}

	return &CreatedMailbox{Channel: "harakirimail", Email: email, Token: ""}, nil
}

/* HarakirimailGetEmails 获取 harakirimail 邮件列表 */
func HarakirimailGetEmails(email string) ([]NormEmail, error) {
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("harakirimail: 邮箱地址为空")
	}

	/* 从邮箱地址提取收件箱名 */
	name := email
	if at := strings.Index(email, "@"); at > 0 {
		name = email[:at]
	}

	/* 获取收件箱列表 */
	u := fmt.Sprintf("%s/api/v1/inbox/%s", harakirimailBase, name)
	req, err := http.NewRequest("GET", u, nil)
	if err != nil {
		return nil, err
	}
	harakirimailDefaultHeaders(req)

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
		return nil, fmt.Errorf("harakirimail inbox: %d", resp.StatusCode)
	}

	var inboxResp struct {
		Emails []struct {
			ID       string `json:"_id"`
			From     string `json:"from"`
			Subject  string `json:"subject"`
			Received string `json:"received"`
		} `json:"emails"`
	}
	if err := json.Unmarshal(body, &inboxResp); err != nil {
		return nil, err
	}

	emails := make([]NormEmail, 0, len(inboxResp.Emails))
	for _, raw := range inboxResp.Emails {
		/* 获取单封邮件详情以拿到正文 */
		htmlBody, textBody := harakirimailFetchBody(raw.ID)

		flat := map[string]interface{}{
			"id":      raw.ID,
			"from":    raw.From,
			"to":      email,
			"subject": raw.Subject,
			"date":    raw.Received,
			"html":    htmlBody,
			"text":    textBody,
			"isRead":  false,
		}
		emails = append(emails, NormalizeMap(flat, email))
	}
	return emails, nil
}

/* harakirimailFetchBody 获取单封邮件正文 */
func harakirimailFetchBody(id string) (string, string) {
	if id == "" {
		return "", ""
	}
	u := fmt.Sprintf("%s/api/v1/email/%s", harakirimailBase, id)
	req, err := http.NewRequest("GET", u, nil)
	if err != nil {
		return "", ""
	}
	harakirimailDefaultHeaders(req)

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return "", ""
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", ""
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return "", ""
	}

	var detail map[string]interface{}
	if err := json.Unmarshal(body, &detail); err != nil {
		return "", ""
	}
	htmlStr, _ := detail["body_html"].(string)
	if htmlStr == "" {
		htmlStr, _ = detail["html"].(string)
	}
	textStr, _ := detail["body_text"].(string)
	if textStr == "" {
		textStr, _ = detail["text"].(string)
	}
	return htmlStr, textStr
}
