package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"regexp"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

/* tempemail.co 临时邮箱服务商
 * 流程：GET /mail/random 创建邮箱（需保存 Cookie）
 *       GET /get-mails 获取邮件列表（返回 HTML 包裹在 JSON 中）
 *       GET /mail/info 获取邮件详情
 * token 存储 address 字符串
 */

const tempemailCoBaseURL = "https://tempemail.co"

/* tempemailCoMailIDRegex 从邮件列表 HTML 中提取邮件 ID */
var tempemailCoMailIDRegex = regexp.MustCompile(`data-id="(\d+)"`)

/* tempemailCoSetHeaders 设置通用请求头 */
func tempemailCoSetHeaders(req *http.Request) {
	req.Header.Set("User-Agent", getCurrentUA())
	req.Header.Set("Accept", "application/json, text/plain, */*")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
	req.Header.Set("Referer", tempemailCoBaseURL+"/")
}

/* TempemailCoGenerate 创建 tempemail.co 临时邮箱
 * GET /mail/random 获取随机邮箱地址，服务端通过 Cookie 维持会话
 * token 存储返回的 address 值，用于后续获取邮件
 */
func TempemailCoGenerate() (*CreatedMailbox, error) {
	client := HTTPClient()

	req, err := http.NewRequest("GET", tempemailCoBaseURL+"/mail/random", nil)
	if err != nil {
		return nil, fmt.Errorf("tempemail-co: 创建请求失败: %w", err)
	}
	tempemailCoSetHeaders(req)

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("tempemail-co: 请求失败: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("tempemail-co: 读取响应失败: %w", err)
	}

	if err := CheckHTTPStatus(resp, "tempemail-co random"); err != nil {
		return nil, err
	}

	var randomResp struct {
		Result  bool   `json:"result"`
		ID      string `json:"id"`
		Address string `json:"address"`
	}
	if err := json.Unmarshal(body, &randomResp); err != nil {
		return nil, fmt.Errorf("tempemail-co: 解析响应失败: %w", err)
	}

	if !randomResp.Result {
		return nil, fmt.Errorf("tempemail-co: 创建邮箱失败, result=false")
	}

	address := strings.TrimSpace(randomResp.Address)
	if address == "" {
		address = strings.TrimSpace(randomResp.ID)
	}
	if address == "" {
		return nil, fmt.Errorf("tempemail-co: 返回的邮箱地址为空")
	}

	return &CreatedMailbox{
		Channel: "tempemail-co",
		Email:   address,
		Token:   address,
	}, nil
}

/* TempemailCoGetEmails 获取 tempemail.co 邮件列表
 * 1. GET /get-mails 获取邮件列表（mails 字段为 HTML 字符串）
 * 2. 用正则从 HTML 中提取邮件 ID（data-id 属性）
 * 3. 对每个 ID 调用 GET /mail/info 获取邮件详情
 */
func TempemailCoGetEmails(email, token string) ([]NormEmail, error) {
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("tempemail-co: 邮箱地址为空")
	}

	address := strings.TrimSpace(token)
	if address == "" {
		address = email
	}

	client := HTTPClient()

	/* 步骤 1：GET /get-mails 获取邮件列表 */
	mailsURL := fmt.Sprintf("%s/get-mails?mail_id=%s&unseen=0&is_new=1", tempemailCoBaseURL, address)
	req, err := http.NewRequest("GET", mailsURL, nil)
	if err != nil {
		return nil, fmt.Errorf("tempemail-co: 创建邮件列表请求失败: %w", err)
	}
	tempemailCoSetHeaders(req)

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("tempemail-co: 获取邮件列表失败: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("tempemail-co: 读取邮件列表响应失败: %w", err)
	}

	if err := CheckHTTPStatus(resp, "tempemail-co get-mails"); err != nil {
		return nil, err
	}

	var mailsResp struct {
		Result bool            `json:"result"`
		Mails  json.RawMessage `json:"mails"`
		Count  int             `json:"count"`
	}
	if err := json.Unmarshal(body, &mailsResp); err != nil {
		return nil, fmt.Errorf("tempemail-co: 解析邮件列表失败: %w", err)
	}

	if !mailsResp.Result {
		return nil, fmt.Errorf("tempemail-co: 获取邮件列表失败, result=false")
	}

	if mailsResp.Count <= 0 {
		return []NormEmail{}, nil
	}

	/* 步骤 2：从 HTML 中提取邮件 ID */
	var mailsHTML string
	if err := json.Unmarshal(mailsResp.Mails, &mailsHTML); err != nil {
		/* mails 字段可能不是字符串，尝试直接使用原始值 */
		mailsHTML = string(mailsResp.Mails)
	}

	matches := tempemailCoMailIDRegex.FindAllStringSubmatch(mailsHTML, -1)
	if len(matches) == 0 {
		return []NormEmail{}, nil
	}

	/* 步骤 3：对每个 ID 获取邮件详情 */
	emails := make([]NormEmail, 0, len(matches))
	for _, match := range matches {
		if len(match) < 2 {
			continue
		}
		mailID := match[1]
		detail := tempemailCoFetchMailDetail(client, mailID, email)
		if detail != nil {
			emails = append(emails, *detail)
		}
	}

	return emails, nil
}

/* tempemailCoFetchMailDetail 获取单封邮件的详情
 * GET /mail/info?id={id} → {result, mail: {fromName, fromAddress, subject, textHtml, displayDate}}
 */
func tempemailCoFetchMailDetail(client interface {
	Do(*http.Request) (*http.Response, error)
}, mailID, recipientEmail string) *NormEmail {
	if mailID == "" {
		return nil
	}

	infoURL := fmt.Sprintf("%s/mail/info?id=%s", tempemailCoBaseURL, mailID)
	req, err := http.NewRequest("GET", infoURL, nil)
	if err != nil {
		return nil
	}
	tempemailCoSetHeaders(req)

	resp, err := client.Do(req)
	if err != nil {
		return nil
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil
	}

	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil
	}

	var infoResp struct {
		Result bool `json:"result"`
		Mail   struct {
			FromName    string `json:"fromName"`
			FromAddress string `json:"fromAddress"`
			DisplayDate string `json:"displayDate"`
			Subject     string `json:"subject"`
			TextHtml    string `json:"textHtml"`
		} `json:"mail"`
	}
	if err := json.Unmarshal(body, &infoResp); err != nil {
		return nil
	}

	if !infoResp.Result {
		return nil
	}

	/* 构造发件人地址：如果有 fromName 则格式化为 "name <email>" */
	fromAddr := infoResp.Mail.FromAddress
	if infoResp.Mail.FromName != "" && infoResp.Mail.FromName != fromAddr {
		fromAddr = fmt.Sprintf("%s <%s>", infoResp.Mail.FromName, infoResp.Mail.FromAddress)
	}

	flat := map[string]interface{}{
		"id":      mailID,
		"from":    fromAddr,
		"to":      recipientEmail,
		"subject": infoResp.Mail.Subject,
		"date":    infoResp.Mail.DisplayDate,
		"html":    infoResp.Mail.TextHtml,
	}
	result := NormalizeMap(flat, recipientEmail)
	return &result
}
