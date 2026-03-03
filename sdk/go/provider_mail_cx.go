package tempemail

import (
	"encoding/json"
	"fmt"
	"io"
	"math/rand"
	"net/http"
	"net/url"
	"strings"
)

/*
 * mail.cx 渠道实现
 * API 文档: https://api.mail.cx/
 *
 * @功能 无需注册，任意邮箱名即可接收邮件
 * @网站 mail.cx
 */

const mailCxBaseURL = "https://api.mail.cx/api/v1"

/* mail.cx 可用域名列表 */
var mailCxDomains = []string{"qabq.com", "nqmo.com"}

/*
 * mailCxGetToken 获取 JWT Token
 * API: POST /auth/authorize_token
 */
func mailCxGetToken() (string, error) {
	req, err := http.NewRequest("POST", mailCxBaseURL+"/auth/authorize_token", strings.NewReader("{}"))
	if err != nil {
		return "", err
	}
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Accept", "application/json")

	client := HTTPClient()
	resp, err := client.Do(req)
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", err
	}

	if resp.StatusCode >= 400 {
		return "", fmt.Errorf("failed to get token: %d", resp.StatusCode)
	}

	/* 响应可能带引号，去除 */
	token := strings.Trim(string(body), "\"")
	return token, nil
}

/*
 * mailCxGenerate 创建临时邮箱
 * @功能 获取 JWT Token + 生成随机邮箱名
 */
func mailCxGenerate() (*EmailInfo, error) {
	token, err := mailCxGetToken()
	if err != nil {
		return nil, err
	}

	domain := mailCxDomains[rand.Intn(len(mailCxDomains))]
	username := randomStr(8)
	email := fmt.Sprintf("%s@%s", username, domain)

	return &EmailInfo{
		Channel: ChannelMailCx,
		Email:   email,
		token:   token,
	}, nil
}

/*
 * mailCxGetEmails 获取邮件列表
 * API: GET /mailbox/{email}
 */
func mailCxGetEmails(token string, email string) ([]Email, error) {
	encodedEmail := url.PathEscape(email)
	req, err := http.NewRequest("GET", mailCxBaseURL+"/mailbox/"+encodedEmail, nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("Accept", "application/json")
	req.Header.Set("Authorization", "Bearer "+token)

	client := HTTPClient()
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	if resp.StatusCode >= 400 {
		return nil, fmt.Errorf("failed to get emails: %d", resp.StatusCode)
	}

	var messages []json.RawMessage
	if err := json.Unmarshal(body, &messages); err != nil {
		return nil, err
	}

	if len(messages) == 0 {
		return []Email{}, nil
	}

	return normalizeRawEmails(messages, email)
}
