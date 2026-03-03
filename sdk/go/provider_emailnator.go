package tempemail

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strings"
)

/*
 * Emailnator 渠道实现
 * 网站: https://www.emailnator.com
 *
 * @功能 通过 XSRF-TOKEN Session 创建临时邮箱并获取邮件
 * @网站 emailnator.com
 */

const emailnatorBaseURL = "https://www.emailnator.com"

/*
 * emailnatorInitSession 初始化 Session，获取 XSRF-TOKEN 和 Cookie
 */
func emailnatorInitSession() (xsrfToken string, cookies string, err error) {
	req, err := http.NewRequest("GET", emailnatorBaseURL, nil)
	if err != nil {
		return "", "", err
	}
	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/145.0.0.0 Safari/537.36")

	client := HTTPClient()
	resp, err := client.Do(req)
	if err != nil {
		return "", "", err
	}
	defer resp.Body.Close()

	/* 从 Set-Cookie 中提取 XSRF-TOKEN 和所有 cookie */
	var cookieParts []string
	for _, cookie := range resp.Cookies() {
		cookieParts = append(cookieParts, fmt.Sprintf("%s=%s", cookie.Name, cookie.Value))
		if cookie.Name == "XSRF-TOKEN" {
			xsrfToken = cookie.Value
		}
	}

	if xsrfToken == "" {
		return "", "", fmt.Errorf("failed to extract XSRF-TOKEN")
	}

	cookies = strings.Join(cookieParts, "; ")
	return xsrfToken, cookies, nil
}

/*
 * emailnatorGenerate 创建临时邮箱
 * @功能 初始化 Session → POST /generate-email
 */
func emailnatorGenerate() (*EmailInfo, error) {
	xsrfToken, cookies, err := emailnatorInitSession()
	if err != nil {
		return nil, err
	}

	reqBody, _ := json.Marshal(map[string]interface{}{
		"email": []string{"domain"},
	})

	req, err := http.NewRequest("POST", emailnatorBaseURL+"/generate-email", bytes.NewReader(reqBody))
	if err != nil {
		return nil, err
	}
	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/145.0.0.0 Safari/537.36")
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Accept", "application/json")
	req.Header.Set("X-Requested-With", "XMLHttpRequest")
	req.Header.Set("X-XSRF-TOKEN", xsrfToken)
	req.Header.Set("Cookie", cookies)

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
		return nil, fmt.Errorf("failed to generate email: %d", resp.StatusCode)
	}

	var result struct {
		Email []string `json:"email"`
	}
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, err
	}

	if len(result.Email) == 0 {
		return nil, fmt.Errorf("failed to generate email: empty response")
	}

	/*
	 * token 存储 XSRF-TOKEN + cookies 的组合（JSON 格式）
	 * 后续获取邮件时需要这两个值
	 */
	tokenPayload, _ := json.Marshal(map[string]string{
		"xsrfToken": xsrfToken,
		"cookies":   cookies,
	})

	return &EmailInfo{
		Channel: ChannelEmailnator,
		Email:   result.Email[0],
		token:   string(tokenPayload),
	}, nil
}

/*
 * emailnatorGetEmails 获取邮件列表
 * API: POST /message-list
 */
func emailnatorGetEmails(token string, email string) ([]Email, error) {
	var session struct {
		XsrfToken string `json:"xsrfToken"`
		Cookies   string `json:"cookies"`
	}
	if err := json.Unmarshal([]byte(token), &session); err != nil {
		return nil, fmt.Errorf("failed to parse session token: %w", err)
	}

	reqBody, _ := json.Marshal(map[string]string{
		"email": email,
	})

	req, err := http.NewRequest("POST", emailnatorBaseURL+"/message-list", bytes.NewReader(reqBody))
	if err != nil {
		return nil, err
	}
	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/145.0.0.0 Safari/537.36")
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Accept", "application/json")
	req.Header.Set("X-Requested-With", "XMLHttpRequest")
	req.Header.Set("X-XSRF-TOKEN", session.XsrfToken)
	req.Header.Set("Cookie", session.Cookies)

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

	var result struct {
		MessageData []struct {
			MessageID string `json:"messageID"`
			From      string `json:"from"`
			Subject   string `json:"subject"`
			Time      string `json:"time"`
		} `json:"messageData"`
	}
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, err
	}

	/* 过滤广告消息 */
	var emails []Email
	for _, msg := range result.MessageData {
		if strings.HasPrefix(msg.MessageID, "ADS") {
			continue
		}
		emails = append(emails, Email{
			ID:          msg.MessageID,
			From:        msg.From,
			To:          email,
			Subject:     msg.Subject,
			Date:        msg.Time,
			IsRead:      false,
			Attachments: []EmailAttachment{},
		})
	}

	if len(emails) == 0 {
		return []Email{}, nil
	}

	return emails, nil
}
