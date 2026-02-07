package tempemail

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/url"
)

const tempMailIOBaseURL = "https://api.internal.temp-mail.io/api/v3"

var tempMailIOHeaders = map[string]string{
	"User-Agent":          "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36 Edg/144.0.0.0",
	"Content-Type":        "application/json",
	"accept-language":     "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
	"application-name":    "web",
	"application-version": "4.0.0",
	"cache-control":       "no-cache",
	"dnt":                 "1",
	"origin":              "https://temp-mail.io",
	"pragma":              "no-cache",
	"referer":             "https://temp-mail.io/",
	"sec-ch-ua":           `"Not(A:Brand";v="8", "Chromium";v="144", "Microsoft Edge";v="144"`,
	"sec-ch-ua-mobile":    "?0",
	"sec-ch-ua-platform":  `"Windows"`,
	"sec-fetch-dest":      "empty",
	"sec-fetch-mode":      "cors",
	"sec-fetch-site":      "same-site",
}

type tempMailIOCreateRequest struct {
	MinNameLength int `json:"min_name_length"`
	MaxNameLength int `json:"max_name_length"`
}

type tempMailIOCreateResponse struct {
	Email string `json:"email"`
	Token string `json:"token"`
}

func setTempMailIOHeaders(req *http.Request) {
	for k, v := range tempMailIOHeaders {
		req.Header.Set(k, v)
	}
}

// tempMailIOGenerate 创建临时邮箱
// API: POST /api/v3/email/new
func tempMailIOGenerate() (*EmailInfo, error) {
	reqBody, _ := json.Marshal(tempMailIOCreateRequest{
		MinNameLength: 100,
		MaxNameLength: 100,
	})

	req, err := http.NewRequest("POST", tempMailIOBaseURL+"/email/new", bytes.NewReader(reqBody))
	if err != nil {
		return nil, err
	}
	setTempMailIOHeaders(req)

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

	var result tempMailIOCreateResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, err
	}

	if result.Email == "" || result.Token == "" {
		return nil, fmt.Errorf("failed to generate email")
	}

	return &EmailInfo{
		Channel: ChannelTempMailIO,
		Email:   result.Email,
		Token:   result.Token,
	}, nil
}

// tempMailIOGetEmails 获取邮件列表
// API: GET /api/v3/email/{email}/messages
// 返回: 直接返回邮件数组
func tempMailIOGetEmails(email string) ([]Email, error) {
	encodedEmail := url.PathEscape(email)

	req, err := http.NewRequest("GET", tempMailIOBaseURL+"/email/"+encodedEmail+"/messages", nil)
	if err != nil {
		return nil, err
	}
	setTempMailIOHeaders(req)

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

	// API 直接返回邮件数组
	var rawMessages []json.RawMessage
	if err := json.Unmarshal(body, &rawMessages); err != nil {
		return nil, err
	}

	return normalizeRawEmails(rawMessages, email)
}
