package tempemail

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"regexp"
	"sync"
)

const tempMailIOBaseURL = "https://api.internal.temp-mail.io/api/v3"
const tempMailIOPageURL = "https://temp-mail.io/en"

/*
 * 缓存从页面动态获取的 mobileTestingHeader 值（用于 X-CORS-Header）
 */
var (
	cachedTempMailIOCorsHeader string
	tempMailIOCorsOnce         sync.Once
)

/*
 * fetchTempMailIOCorsHeader 从 temp-mail.io 页面的 __NUXT__ 运行时配置中提取 mobileTestingHeader
 * 该值用于 API 请求的 X-CORS-Header 头，缺少此头会导致 400 错误
 */
func fetchTempMailIOCorsHeader() string {
	tempMailIOCorsOnce.Do(func() {
		req, err := http.NewRequest("GET", tempMailIOPageURL, nil)
		if err != nil {
			cachedTempMailIOCorsHeader = "1"
			return
		}
		req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36")

		client := HTTPClient()
		resp, err := client.Do(req)
		if err != nil {
			cachedTempMailIOCorsHeader = "1"
			return
		}
		defer resp.Body.Close()

		body, err := io.ReadAll(resp.Body)
		if err != nil {
			cachedTempMailIOCorsHeader = "1"
			return
		}

		re := regexp.MustCompile(`mobileTestingHeader\s*:\s*"([^"]+)"`)
		matches := re.FindSubmatch(body)
		if len(matches) >= 2 {
			cachedTempMailIOCorsHeader = string(matches[1])
		} else {
			cachedTempMailIOCorsHeader = "1"
		}
	})
	return cachedTempMailIOCorsHeader
}

type tempMailIOCreateRequest struct {
	MinNameLength int `json:"min_name_length"`
	MaxNameLength int `json:"max_name_length"`
}

type tempMailIOCreateResponse struct {
	Email string `json:"email"`
	Token string `json:"token"`
}

/*
 * setTempMailIOHeaders 设置 API 请求头
 * 关键头: Content-Type, Application-Name, Application-Version, X-CORS-Header
 */
func setTempMailIOHeaders(req *http.Request) {
	corsHeader := fetchTempMailIOCorsHeader()
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Application-Name", "web")
	req.Header.Set("Application-Version", "4.0.0")
	req.Header.Set("X-CORS-Header", corsHeader)
	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36 Edg/144.0.0.0")
	req.Header.Set("Origin", "https://temp-mail.io")
	req.Header.Set("Referer", "https://temp-mail.io/")
}

// tempMailIOGenerate 创建临时邮箱
// API: POST /api/v3/email/new
func tempMailIOGenerate() (*EmailInfo, error) {
	reqBody, _ := json.Marshal(tempMailIOCreateRequest{
		MinNameLength: 10,
		MaxNameLength: 10,
	})

	req, err := http.NewRequest("POST", tempMailIOBaseURL+"/email/new", bytes.NewReader(reqBody))
	if err != nil {
		return nil, err
	}
	setTempMailIOHeaders(req)

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
	req, err := http.NewRequest("GET", tempMailIOBaseURL+"/email/"+email+"/messages", nil)
	if err != nil {
		return nil, err
	}
	setTempMailIOHeaders(req)

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

	// API 直接返回邮件数组
	var rawMessages []json.RawMessage
	if err := json.Unmarshal(body, &rawMessages); err != nil {
		return nil, err
	}

	return normalizeRawEmails(rawMessages, email)
}
