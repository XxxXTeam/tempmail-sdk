package provider

import (
	"encoding/json"
	"fmt"
	"io"

	http "github.com/bogdanfinn/fhttp"
	tls_client "github.com/bogdanfinn/tls-client"
)

/* 10minutemail.net 临时邮箱服务商
 * 流程：GET /address.api.php 创建邮箱（服务端通过 session cookie 分配）
 *       GET /address.api.php（带 Cookie）获取邮件列表
 *       GET /mail.api.php?mailid={id}（带 Cookie）获取邮件详情
 * token 存储格式: JSON {"cookie":"PHPSESSID=xxx"}
 */

const tenMinuteMailNetBaseURL = "https://10minutemail.net"

/* tenMinuteMailNetToken 内部 token 结构 */
type tenMinuteMailNetToken struct {
	Cookie string `json:"cookie"`
}

/* tenMinuteMailNetAddressResponse 创建/刷新邮箱接口响应 */
type tenMinuteMailNetAddressResponse struct {
	MailGetMail string                         `json:"mail_get_mail"`
	MailList    []tenMinuteMailNetMailListItem `json:"mail_list"`
}

/* tenMinuteMailNetMailListItem 邮件列表项 */
type tenMinuteMailNetMailListItem struct {
	MailID   string `json:"mail_id"`
	From     string `json:"from"`
	Subject  string `json:"subject"`
	Datetime string `json:"datetime"`
}

/* tenMinuteMailNetDetailBody 邮件详情中的 body 条目 */
type tenMinuteMailNetDetailBody struct {
	Content string `json:"content"`
	Body    string `json:"body"`
}

/* tenMinuteMailNetDetailResponse 邮件详情接口响应 */
type tenMinuteMailNetDetailResponse struct {
	From     string                       `json:"from"`
	To       string                       `json:"to"`
	Subject  string                       `json:"subject"`
	Datetime string                       `json:"datetime"`
	Body     []tenMinuteMailNetDetailBody `json:"body"`
	Plain    []string                     `json:"plain"`
}

/* TenMinuteMailNetGenerate 创建 10minutemail.net 临时邮箱
 * API: GET /address.api.php
 * 服务端通过 session cookie 分配邮箱地址，token 存储 PHPSESSID
 */
func TenMinuteMailNetGenerate(duration int, domain string) (*CreatedMailbox, error) {
	client := HTTPClient()

	req, err := http.NewRequest("GET", tenMinuteMailNetBaseURL+"/address.api.php", nil)
	if err != nil {
		return nil, fmt.Errorf("10minutemail-net: 创建请求失败: %w", err)
	}
	req.Header.Set("User-Agent", getCurrentUA())
	req.Header.Set("Accept", "application/json, text/plain, */*")

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("10minutemail-net: 请求失败: %w", err)
	}
	defer resp.Body.Close()

	if err := CheckHTTPStatus(resp, "10minutemail-net address"); err != nil {
		return nil, err
	}

	/* 从响应头提取 PHPSESSID cookie */
	var sessionID string
	for _, cookie := range resp.Cookies() {
		if cookie.Name == "PHPSESSID" {
			sessionID = cookie.Value
			break
		}
	}
	if sessionID == "" {
		return nil, fmt.Errorf("10minutemail-net: 响应中未找到 PHPSESSID cookie")
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("10minutemail-net: 读取响应失败: %w", err)
	}

	var result tenMinuteMailNetAddressResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, fmt.Errorf("10minutemail-net: 解析响应失败: %w", err)
	}

	if result.MailGetMail == "" {
		return nil, fmt.Errorf("10minutemail-net: 响应缺少 mail_get_mail 字段")
	}

	/* 将 PHPSESSID 序列化为 JSON 存储 */
	tokenData, err := json.Marshal(tenMinuteMailNetToken{
		Cookie: "PHPSESSID=" + sessionID,
	})
	if err != nil {
		return nil, fmt.Errorf("10minutemail-net: 序列化 token 失败: %w", err)
	}

	return &CreatedMailbox{
		Channel: "10minutemail-net",
		Email:   result.MailGetMail,
		Token:   string(tokenData),
	}, nil
}

/* TenMinuteMailNetGetEmails 获取 10minutemail.net 邮件列表
 * 1. GET /address.api.php（带 Cookie）获取 mail_list
 * 2. 逐封调用 GET /mail.api.php?mailid={id} 获取详情
 */
func TenMinuteMailNetGetEmails(token, email string) ([]NormEmail, error) {
	if token == "" {
		return nil, fmt.Errorf("10minutemail-net: token 为空")
	}

	var tkn tenMinuteMailNetToken
	if err := json.Unmarshal([]byte(token), &tkn); err != nil {
		return nil, fmt.Errorf("10minutemail-net: 解析 token 失败: %w", err)
	}
	if tkn.Cookie == "" {
		return nil, fmt.Errorf("10minutemail-net: token 缺少 cookie 字段")
	}

	client := HTTPClient()

	/* 步骤 1：获取邮件列表 */
	req, err := http.NewRequest("GET", tenMinuteMailNetBaseURL+"/address.api.php", nil)
	if err != nil {
		return nil, fmt.Errorf("10minutemail-net: 创建列表请求失败: %w", err)
	}
	req.Header.Set("User-Agent", getCurrentUA())
	req.Header.Set("Accept", "application/json, text/plain, */*")
	req.Header.Set("Cookie", tkn.Cookie)

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("10minutemail-net: 获取列表失败: %w", err)
	}
	defer resp.Body.Close()

	if err := CheckHTTPStatus(resp, "10minutemail-net list"); err != nil {
		return nil, err
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("10minutemail-net: 读取列表响应失败: %w", err)
	}

	var addrResp tenMinuteMailNetAddressResponse
	if err := json.Unmarshal(body, &addrResp); err != nil {
		return nil, fmt.Errorf("10minutemail-net: 解析列表响应失败: %w", err)
	}

	if len(addrResp.MailList) == 0 {
		return []NormEmail{}, nil
	}

	/* 步骤 2：逐封获取邮件详情 */
	emails := make([]NormEmail, 0, len(addrResp.MailList))
	for _, item := range addrResp.MailList {
		detail, err := tenMinuteMailNetFetchDetail(client, tkn.Cookie, item.MailID, email)
		if err != nil {
			continue
		}
		emails = append(emails, detail)
	}

	return emails, nil
}

/* tenMinuteMailNetFetchDetail 获取单封邮件详情并转为 NormEmail */
func tenMinuteMailNetFetchDetail(client tls_client.HttpClient, cookie, mailID, recipientEmail string) (NormEmail, error) {
	url := fmt.Sprintf("%s/mail.api.php?mailid=%s", tenMinuteMailNetBaseURL, mailID)
	req, err := http.NewRequest("GET", url, nil)
	if err != nil {
		return NormEmail{}, err
	}
	req.Header.Set("User-Agent", getCurrentUA())
	req.Header.Set("Accept", "application/json, text/plain, */*")
	req.Header.Set("Cookie", cookie)

	resp, err := client.Do(req)
	if err != nil {
		return NormEmail{}, err
	}
	defer resp.Body.Close()

	if err := CheckHTTPStatus(resp, "10minutemail-net detail"); err != nil {
		return NormEmail{}, err
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return NormEmail{}, err
	}

	var detail tenMinuteMailNetDetailResponse
	if err := json.Unmarshal(body, &detail); err != nil {
		return NormEmail{}, err
	}

	/* 提取 text 和 html 内容 */
	var textBody, htmlBody string
	for _, b := range detail.Body {
		switch b.Content {
		case "text/plain":
			textBody = b.Body
		case "text/html":
			htmlBody = b.Body
		}
	}
	if textBody == "" && len(detail.Plain) > 0 {
		textBody = detail.Plain[0]
	}

	return NormEmail{
		ID:      mailID,
		From:    detail.From,
		To:      recipientEmail,
		Subject: detail.Subject,
		Text:    textBody,
		HTML:    htmlBody,
		Date:    detail.Datetime,
	}, nil
}
