package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"math/rand"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

const tempmailPlusBase = "https://tempmail.plus/api/mails"
const tempmailPlusDomain = "mailto.plus"

/* tempmailPlusDefaultHeaders 设置通用请求头 */
func tempmailPlusDefaultHeaders(req *http.Request) {
	req.Header.Set("Accept", "application/json")
	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36")
	req.Header.Set("Referer", "https://tempmail.plus/")
	req.Header.Set("Origin", "https://tempmail.plus")
}

/* 随机生成 12 位字母数字字符串作为本地部分 */
func tempmailPlusRandomLocal() string {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	b := make([]byte, 12)
	for i := range b {
		b[i] = chars[rand.Intn(len(chars))]
	}
	return string(b)
}

/* TempmailPlusGenerate 创建 tempmail-plus 临时邮箱 */
func TempmailPlusGenerate(domain *string, channel ...string) (*CreatedMailbox, error) {
	selectedDomain := tempmailPlusDomain
	if domain != nil {
		d := strings.TrimSpace(*domain)
		if d != "" {
			selectedDomain = d
		}
	}
	selectedChannel := "tempmail-plus"
	if len(channel) > 0 && strings.TrimSpace(channel[0]) != "" {
		selectedChannel = strings.TrimSpace(channel[0])
	}

	local := tempmailPlusRandomLocal()
	email := fmt.Sprintf("%s@%s", local, selectedDomain)

	/* 调用列表接口验证地址可用 */
	u := fmt.Sprintf("%s/?email=%s&epin=", tempmailPlusBase, email)
	req, err := http.NewRequest("GET", u, nil)
	if err != nil {
		return nil, err
	}
	tempmailPlusDefaultHeaders(req)

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	io.ReadAll(resp.Body)

	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("tempmail-plus: 验证邮箱失败 %d", resp.StatusCode)
	}

	return &CreatedMailbox{Channel: selectedChannel, Email: email, Token: ""}, nil
}

/* TempmailPlusGetEmails 获取 tempmail-plus 邮件列表 */
func TempmailPlusGetEmails(email string) ([]NormEmail, error) {
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("tempmail-plus: 邮箱地址为空")
	}

	/* 获取邮件列表 */
	u := fmt.Sprintf("%s/?email=%s&epin=", tempmailPlusBase, email)
	req, err := http.NewRequest("GET", u, nil)
	if err != nil {
		return nil, err
	}
	tempmailPlusDefaultHeaders(req)

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
		return nil, fmt.Errorf("tempmail-plus list: %d", resp.StatusCode)
	}

	var listResp struct {
		MailList []struct {
			MailID   int    `json:"mail_id"`
			FromMail string `json:"from_mail"`
			Subject  string `json:"subject"`
			Time     string `json:"time"`
			IsNew    bool   `json:"is_new"`
		} `json:"mail_list"`
	}
	if err := json.Unmarshal(body, &listResp); err != nil {
		return nil, err
	}

	emails := make([]NormEmail, 0, len(listResp.MailList))
	for _, item := range listResp.MailList {
		/* 获取单封邮件详情以拿到正文 */
		htmlBody, textBody := tempmailPlusFetchBody(item.MailID, email)

		flat := map[string]interface{}{
			"id":      fmt.Sprintf("%d", item.MailID),
			"from":    item.FromMail,
			"to":      email,
			"subject": item.Subject,
			"date":    item.Time,
			"html":    htmlBody,
			"text":    textBody,
			"isRead":  !item.IsNew,
		}
		emails = append(emails, NormalizeMap(flat, email))
	}
	return emails, nil
}

/* tempmailPlusFetchBody 获取单封邮件正文 */
func tempmailPlusFetchBody(mailID int, email string) (string, string) {
	if mailID == 0 {
		return "", ""
	}
	u := fmt.Sprintf("%s/%d?email=%s&epin=", tempmailPlusBase, mailID, email)
	req, err := http.NewRequest("GET", u, nil)
	if err != nil {
		return "", ""
	}
	tempmailPlusDefaultHeaders(req)

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
	htmlStr, _ := detail["html"].(string)
	textStr, _ := detail["text"].(string)
	return htmlStr, textStr
}
