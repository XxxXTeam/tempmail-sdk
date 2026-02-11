package tempemail

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
)

const tempmailLaBaseURL = "https://tempmail.la/api"

var tempmailLaHeaders = map[string]string{
	"User-Agent":         "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36 Edg/144.0.0.0",
	"Accept":             "application/json, text/plain, */*",
	"Content-Type":       "application/json",
	"accept-language":    "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
	"cache-control":      "no-cache",
	"dnt":                "1",
	"locale":             "zh-CN",
	"origin":             "https://tempmail.la",
	"platform":           "PC",
	"pragma":             "no-cache",
	"product":            "TEMP_MAIL",
	"referer":            "https://tempmail.la/zh-CN/tempmail",
	"sec-ch-ua":          `"Not(A:Brand";v="8", "Chromium";v="144", "Microsoft Edge";v="144"`,
	"sec-ch-ua-mobile":   "?0",
	"sec-ch-ua-platform": `"Windows"`,
	"sec-fetch-dest":     "empty",
	"sec-fetch-mode":     "cors",
	"sec-fetch-site":     "same-origin",
}

type tempmailLaCreateRequest struct {
	Turnstile string `json:"turnstile"`
}

type tempmailLaCreateResponse struct {
	Code int `json:"code"`
	Data struct {
		MailID  string `json:"mailId"`
		Address string `json:"address"`
		Type    string `json:"type"`
		StartAt string `json:"startAt"`
		EndAt   string `json:"endAt"`
	} `json:"data"`
}

type tempmailLaBoxRequest struct {
	Address string  `json:"address"`
	Cursor  *string `json:"cursor"`
}

type tempmailLaBoxResponse struct {
	Code int `json:"code"`
	Data struct {
		Rows    []json.RawMessage `json:"rows"`
		Cursor  *string           `json:"cursor"`
		HasMore bool              `json:"hasMore"`
	} `json:"data"`
}

func setTempmailLaHeaders(req *http.Request) {
	for k, v := range tempmailLaHeaders {
		req.Header.Set(k, v)
	}
}

// tempmailLaGenerate 创建临时邮箱
// API: POST /api/mail/create
func tempmailLaGenerate() (*EmailInfo, error) {
	reqBody, _ := json.Marshal(tempmailLaCreateRequest{Turnstile: ""})

	req, err := http.NewRequest("POST", tempmailLaBaseURL+"/mail/create", bytes.NewReader(reqBody))
	if err != nil {
		return nil, err
	}
	setTempmailLaHeaders(req)

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

	var result tempmailLaCreateResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, err
	}

	if result.Code != 0 || result.Data.Address == "" {
		return nil, fmt.Errorf("failed to generate email")
	}

	return &EmailInfo{
		Channel:   ChannelTempmailLa,
		Email:     result.Data.Address,
		ExpiresAt: result.Data.EndAt,
		CreatedAt: result.Data.StartAt,
	}, nil
}

// tempmailLaGetEmails 获取邮件列表
// API: POST /api/mail/box
func tempmailLaGetEmails(email string) ([]Email, error) {
	var allRawMessages []json.RawMessage
	var cursor *string

	// 支持分页，循环获取所有邮件
	for {
		reqBody, _ := json.Marshal(tempmailLaBoxRequest{
			Address: email,
			Cursor:  cursor,
		})

		req, err := http.NewRequest("POST", tempmailLaBaseURL+"/mail/box", bytes.NewReader(reqBody))
		if err != nil {
			return nil, err
		}
		setTempmailLaHeaders(req)

		client := HTTPClient()
		resp, err := client.Do(req)
		if err != nil {
			return nil, err
		}

		body, err := io.ReadAll(resp.Body)
		resp.Body.Close()
		if err != nil {
			return nil, err
		}

		var result tempmailLaBoxResponse
		if err := json.Unmarshal(body, &result); err != nil {
			return nil, err
		}

		if result.Code != 0 {
			return nil, fmt.Errorf("failed to get emails")
		}

		allRawMessages = append(allRawMessages, result.Data.Rows...)

		if !result.Data.HasMore || result.Data.Cursor == nil {
			break
		}
		cursor = result.Data.Cursor
	}

	return normalizeRawEmails(allRawMessages, email)
}
