package provider

import (
	"fmt"
	"io"
	"math/rand"
	"regexp"
	"strings"
	"time"

	http "github.com/bogdanfinn/fhttp"
)

/**
 * MailCatch — https://mailcatch.com
 * 公开收件箱，无需认证/Session
 * 创建邮箱: 生成随机用户名 + "@mailcatch.com"
 * 获取邮件: GET https://mailcatch.com/api/list/{inboxName} → HTML
 * 获取邮件内容: GET https://mailcatch.com/api/data/{inboxName}/{emailId} → HTML
 */

const mailcatchDomain = "mailcatch.com"
const mailcatchAPIBase = "https://mailcatch.com"

/* mailcatchRandomLocal 生成随机用户名 */
func mailcatchRandomLocal(n int) string {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	b := make([]byte, n)
	for i := range b {
		b[i] = chars[rand.Intn(len(chars))]
	}
	return string(b)
}

/* mailcatchDefaultHeaders 设置请求头 */
func mailcatchDefaultHeaders(req *http.Request) {
	req.Header.Set("Accept", "text/html, */*")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
	req.Header.Set("Cache-Control", "no-cache")
	req.Header.Set("DNT", "1")
	req.Header.Set("Pragma", "no-cache")
	req.Header.Set("Referer", "https://mailcatch.com/")
	req.Header.Set("User-Agent", getCurrentUA())
}

/* 邮件项正则：提取 data-email-id, data-subject, data-timestamp, data-sender */
var mailcatchItemRe = regexp.MustCompile(`data-email-id="([^"]*?)"\s+data-subject="([^"]*?)"\s+data-timestamp="([^"]*?)"\s+data-sender="([^"]*?)"`)

/*
 * MailcatchGenerate 创建 mailcatch.com 临时邮箱
 * 无需 API 调用，直接生成随机用户名 + "@mailcatch.com"
 */
func MailcatchGenerate(channel ...string) (*CreatedMailbox, error) {
	user := mailcatchRandomLocal(10)
	email := user + "@" + mailcatchDomain
	ch := "mailcatch"
	if len(channel) > 0 && channel[0] != "" {
		ch = channel[0]
	}
	return &CreatedMailbox{Channel: ch, Email: email, Token: ""}, nil
}

/*
 * MailcatchGetEmails 获取 mailcatch.com 邮件列表
 * 1. GET /api/list/{username} → HTML 片段
 * 2. 正则提取邮件项属性
 * 3. 对每封邮件 GET /api/data/{username}/{emailId} → HTML 正文
 * 4. 标准化为 NormEmail
 */
func MailcatchGetEmails(email string) ([]NormEmail, error) {
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("mailcatch: empty email")
	}
	/* 提取 @ 前面的用户名部分 */
	parts := strings.SplitN(email, "@", 2)
	username := parts[0]

	/* 获取邮件列表 */
	listURL := fmt.Sprintf("%s/api/list/%s", mailcatchAPIBase, username)
	req, err := http.NewRequest("GET", listURL, nil)
	if err != nil {
		return nil, err
	}
	mailcatchDefaultHeaders(req)

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
		return nil, fmt.Errorf("mailcatch list: %d", resp.StatusCode)
	}

	listHTML := string(body)

	/* 正则提取所有邮件项 */
	matches := mailcatchItemRe.FindAllStringSubmatch(listHTML, -1)
	if len(matches) == 0 {
		return []NormEmail{}, nil
	}

	var result []NormEmail
	for _, m := range matches {
		emailID := m[1]
		subject := m[2]
		timestamp := m[3]
		sender := m[4]

		/* 获取邮件正文 */
		dataURL := fmt.Sprintf("%s/api/data/%s/%s", mailcatchAPIBase, username, emailID)
		dataReq, err := http.NewRequest("GET", dataURL, nil)
		if err != nil {
			continue
		}
		mailcatchDefaultHeaders(dataReq)

		dataResp, err := HTTPClient().Do(dataReq)
		if err != nil {
			continue
		}

		dataBody, err := io.ReadAll(dataResp.Body)
		dataResp.Body.Close()
		if err != nil {
			continue
		}

		htmlContent := ""
		if dataResp.StatusCode >= 200 && dataResp.StatusCode < 300 {
			htmlContent = string(dataBody)
		}

		/* 解析时间戳 */
		dateStr := timestamp
		if t, parseErr := time.Parse(time.RFC3339, timestamp); parseErr == nil {
			dateStr = t.UTC().Format(time.RFC3339)
		}

		result = append(result, NormEmail{
			ID:      emailID,
			From:    sender,
			To:      email,
			Subject: subject,
			HTML:    htmlContent,
			Date:    dateStr,
		})
	}

	if result == nil {
		result = []NormEmail{}
	}
	return result, nil
}
