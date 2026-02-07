package tempemail

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
)

const awamailBaseURL = "https://awamail.com/welcome"

var awamailHeaders = map[string]string{
	"User-Agent":         "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36 Edg/144.0.0.0",
	"Accept":             "application/json, text/javascript, */*; q=0.01",
	"accept-language":    "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
	"cache-control":      "no-cache",
	"dnt":                "1",
	"origin":             "https://awamail.com",
	"pragma":             "no-cache",
	"referer":            "https://awamail.com/?lang=zh",
	"sec-ch-ua":          `"Not(A:Brand";v="8", "Chromium";v="144", "Microsoft Edge";v="144"`,
	"sec-ch-ua-mobile":   "?0",
	"sec-ch-ua-platform": `"Windows"`,
	"sec-fetch-dest":     "empty",
	"sec-fetch-mode":     "cors",
	"sec-fetch-site":     "same-origin",
}

type awamailCreateResponse struct {
	Success bool `json:"success"`
	Data    struct {
		EmailAddress string `json:"email_address"`
		ExpiredAt    string `json:"expired_at"`
		CreatedAt    string `json:"created_at"`
		ID           int    `json:"id"`
	} `json:"data"`
}

type awamailEmailsResponse struct {
	Success bool `json:"success"`
	Data    struct {
		Emails []json.RawMessage `json:"emails"`
	} `json:"data"`
}

func setAwamailHeaders(req *http.Request) {
	for k, v := range awamailHeaders {
		req.Header.Set(k, v)
	}
}

// awamailGenerate 创建临时邮箱
// API: POST /welcome/change_mailbox (空 body)
// 需要保存响应中的 Set-Cookie (awamail_session) 用于后续获取邮件
func awamailGenerate() (*EmailInfo, error) {
	req, err := http.NewRequest("POST", awamailBaseURL+"/change_mailbox", nil)
	if err != nil {
		return nil, err
	}
	setAwamailHeaders(req)
	req.Header.Set("Content-Length", "0")

	// 不自动跟踪重定向，以便捕获 Set-Cookie
	client := &http.Client{
		CheckRedirect: func(req *http.Request, via []*http.Request) error {
			return http.ErrUseLastResponse
		},
	}
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	// 提取 awamail_session cookie
	var sessionCookie string
	for _, cookie := range resp.Cookies() {
		if cookie.Name == "awamail_session" {
			sessionCookie = fmt.Sprintf("awamail_session=%s", cookie.Value)
			break
		}
	}
	if sessionCookie == "" {
		return nil, fmt.Errorf("failed to extract session cookie")
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	var result awamailCreateResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, err
	}

	if !result.Success || result.Data.EmailAddress == "" {
		return nil, fmt.Errorf("failed to generate email")
	}

	return &EmailInfo{
		Channel:   ChannelAwamail,
		Email:     result.Data.EmailAddress,
		Token:     sessionCookie,
		ExpiresAt: result.Data.ExpiredAt,
		CreatedAt: result.Data.CreatedAt,
	}, nil
}

// awamailGetEmails 获取邮件列表
// API: GET /welcome/get_emails
// 需要传入 Cookie (awamail_session) 和 x-requested-with 头
func awamailGetEmails(token string, email string) ([]Email, error) {
	req, err := http.NewRequest("GET", awamailBaseURL+"/get_emails", nil)
	if err != nil {
		return nil, err
	}
	setAwamailHeaders(req)
	req.Header.Set("Cookie", token)
	req.Header.Set("X-Requested-With", "XMLHttpRequest")

	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	var result awamailEmailsResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, err
	}

	if !result.Success {
		return nil, fmt.Errorf("failed to get emails")
	}

	return normalizeRawEmails(result.Data.Emails, email)
}
