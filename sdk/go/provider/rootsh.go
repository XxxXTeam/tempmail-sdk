package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"math/rand"
	"net/url"
	"strings"
	"time"

	http "github.com/bogdanfinn/fhttp"
)

/* rootsh.com（BccTo.CC）临时邮箱服务商
 * 流程：GET / 获取 session cookie → POST /applymail 创建邮箱
 *       POST /getmail 获取邮件列表 → POST /viewmail 获取邮件正文
 * token 存储 time 值，用于增量获取邮件
 */

const rootshBaseURL = "https://rootsh.com"
const rootshDefaultDomain = "bccto.cc"

/* rootshSetXHRHeaders 设置 AJAX 请求所需的通用请求头 */
func rootshSetXHRHeaders(req *http.Request) {
	req.Header.Set("User-Agent", getCurrentUA())
	req.Header.Set("Accept", "application/json, text/javascript, */*; q=0.01")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
	req.Header.Set("X-Requested-With", "XMLHttpRequest")
	req.Header.Set("Referer", rootshBaseURL+"/")
	req.Header.Set("Origin", rootshBaseURL)
}

/* rootshRandomLocal 随机生成 10 位字母数字字符串作为邮箱本地部分 */
func rootshRandomLocal() string {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	b := make([]byte, 10)
	for i := range b {
		b[i] = chars[rand.Intn(len(chars))]
	}
	return string(b)
}

/* RootshGenerate 创建 rootsh 临时邮箱
 * 1. GET / 获取 session cookie
 * 2. POST /applymail 申请邮箱地址
 * token 存储服务端返回的 time 值，用于后续增量获取邮件
 */
func RootshGenerate() (*CreatedMailbox, error) {
	client := HTTPClient()

	/* 步骤 1：GET / 获取 session cookie */
	req, err := http.NewRequest("GET", rootshBaseURL+"/", nil)
	if err != nil {
		return nil, fmt.Errorf("rootsh: 创建首页请求失败: %w", err)
	}
	req.Header.Set("User-Agent", getCurrentUA())
	req.Header.Set("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("rootsh: 获取首页失败: %w", err)
	}
	defer resp.Body.Close()
	io.ReadAll(resp.Body)

	if err := CheckHTTPStatus(resp, "rootsh home"); err != nil {
		return nil, err
	}

	/* 步骤 2：POST /applymail 创建邮箱 */
	local := rootshRandomLocal()
	emailAddr := fmt.Sprintf("%s@%s", local, rootshDefaultDomain)

	form := url.Values{}
	form.Set("mail", emailAddr)

	req2, err := http.NewRequest("POST", rootshBaseURL+"/applymail", strings.NewReader(form.Encode()))
	if err != nil {
		return nil, fmt.Errorf("rootsh: 创建申请请求失败: %w", err)
	}
	rootshSetXHRHeaders(req2)
	req2.Header.Set("Content-Type", "application/x-www-form-urlencoded")

	resp2, err := client.Do(req2)
	if err != nil {
		return nil, fmt.Errorf("rootsh: 申请邮箱失败: %w", err)
	}
	defer resp2.Body.Close()

	body, err := io.ReadAll(resp2.Body)
	if err != nil {
		return nil, fmt.Errorf("rootsh: 读取申请响应失败: %w", err)
	}

	if err := CheckHTTPStatus(resp2, "rootsh applymail"); err != nil {
		return nil, err
	}

	var applyResp struct {
		Success string      `json:"success"`
		User    string      `json:"user"`
		Time    json.Number `json:"time"`
		Tips    string      `json:"tips"`
	}
	if err := json.Unmarshal(body, &applyResp); err != nil {
		return nil, fmt.Errorf("rootsh: 解析申请响应失败: %w", err)
	}

	if applyResp.Success != "true" {
		return nil, fmt.Errorf("rootsh: 申请邮箱失败, tips=%s", applyResp.Tips)
	}

	/* 使用服务端返回的邮箱地址（可能与请求的不同） */
	actualEmail := strings.TrimSpace(applyResp.User)
	if actualEmail == "" {
		actualEmail = emailAddr
	}

	/* token 存储 time 值，用于后续增量获取邮件 */
	tokenTime := applyResp.Time.String()
	if tokenTime == "" {
		tokenTime = "0"
	}

	return &CreatedMailbox{
		Channel: "rootsh",
		Email:   actualEmail,
		Token:   tokenTime,
	}, nil
}

/* RootshGetEmails 获取 rootsh 邮件列表
 * 1. POST /getmail 获取邮件列表（增量查询，基于 lastCheckTime）
 * 2. 对每封邮件 POST /viewmail 获取 HTML 正文
 */
func RootshGetEmails(email, token string) ([]NormEmail, error) {
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("rootsh: 邮箱地址为空")
	}

	lastCheckTime := strings.TrimSpace(token)
	if lastCheckTime == "" {
		lastCheckTime = "0"
	}

	client := HTTPClient()

	/* POST /getmail 获取邮件列表 */
	now := fmt.Sprintf("%d", time.Now().UnixMilli())
	form := url.Values{}
	form.Set("mail", email)
	form.Set("time", lastCheckTime)
	form.Set("_", now)

	req, err := http.NewRequest("POST", rootshBaseURL+"/getmail", strings.NewReader(form.Encode()))
	if err != nil {
		return nil, fmt.Errorf("rootsh: 创建获取邮件请求失败: %w", err)
	}
	rootshSetXHRHeaders(req)
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("rootsh: 获取邮件列表失败: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("rootsh: 读取邮件列表响应失败: %w", err)
	}

	if err := CheckHTTPStatus(resp, "rootsh getmail"); err != nil {
		return nil, err
	}

	/* 解析邮件列表响应
	 * mail 数组中每个元素: [displayName, fromEmail, subject, dateStr, fid, receivedTime]
	 */
	var mailResp struct {
		Success string          `json:"success"`
		To      string          `json:"to"`
		Mail    [][]interface{} `json:"mail"`
	}
	if err := json.Unmarshal(body, &mailResp); err != nil {
		return nil, fmt.Errorf("rootsh: 解析邮件列表失败: %w", err)
	}

	if mailResp.Success != "true" {
		return nil, fmt.Errorf("rootsh: 获取邮件列表失败")
	}

	if len(mailResp.Mail) == 0 {
		return []NormEmail{}, nil
	}

	emails := make([]NormEmail, 0, len(mailResp.Mail))
	for _, item := range mailResp.Mail {
		if len(item) < 6 {
			continue
		}

		/* 提取字段：[displayName, fromEmail, subject, dateStr, fid, receivedTime] */
		fromEmail := rootshToString(item[1])
		displayName := rootshToString(item[0])
		subject := rootshToString(item[2])
		dateStr := rootshToString(item[3])
		fid := rootshToString(item[4])

		/* 构造发件人地址：如果有 displayName 则格式化为 "name <email>" */
		fromAddr := fromEmail
		if displayName != "" && displayName != fromEmail {
			fromAddr = fmt.Sprintf("%s <%s>", displayName, fromEmail)
		}

		/* POST /viewmail 获取邮件正文 */
		htmlBody := rootshFetchMailBody(client, fid, email)

		flat := map[string]interface{}{
			"id":      fid,
			"from":    fromAddr,
			"to":      email,
			"subject": subject,
			"date":    dateStr,
			"html":    htmlBody,
		}
		emails = append(emails, NormalizeMap(flat, email))
	}

	return emails, nil
}

/* rootshFetchMailBody 获取单封邮件的 HTML 正文 */
func rootshFetchMailBody(client interface{ Do(*http.Request) (*http.Response, error) }, fid, email string) string {
	if fid == "" {
		return ""
	}

	form := url.Values{}
	form.Set("mail", fid)
	form.Set("to", email)

	req, err := http.NewRequest("POST", rootshBaseURL+"/viewmail", strings.NewReader(form.Encode()))
	if err != nil {
		return ""
	}
	rootshSetXHRHeaders(req)
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")

	resp, err := client.Do(req)
	if err != nil {
		return ""
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return ""
	}

	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return ""
	}

	var viewResp struct {
		Success    string `json:"success"`
		Mail       string `json:"mail"`
		Attachment string `json:"attachment"`
	}
	if err := json.Unmarshal(body, &viewResp); err != nil {
		return ""
	}

	if viewResp.Success != "true" {
		return ""
	}

	return viewResp.Mail
}

/* rootshToString 将 interface{} 安全转换为字符串 */
func rootshToString(v interface{}) string {
	if v == nil {
		return ""
	}
	switch val := v.(type) {
	case string:
		return val
	case float64:
		return fmt.Sprintf("%.0f", val)
	case json.Number:
		return val.String()
	default:
		return fmt.Sprintf("%v", val)
	}
}
