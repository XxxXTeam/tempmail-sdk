package provider

import (
	"bytes"
	"crypto/rand"
	"encoding/json"
	"fmt"
	"io"
	"math/big"
	"net/url"
	"strings"

	http "github.com/bogdanfinn/fhttp"
	tls_client "github.com/bogdanfinn/tls-client"
)

const chatgptOrgUkBaseURL = "https://mail.chatgpt.org.uk/api"

// 请求头，参照官网 Web 客户端；Referer 使用 /zh/ 前缀
var chatgptOrgUkHeaders = map[string]string{
	"Accept":         "*/*",
	"Referer":        "https://mail.chatgpt.org.uk/zh/",
	"Origin":         "https://mail.chatgpt.org.uk",
	"DNT":            "1",
	"sec-fetch-dest": "empty",
	"sec-fetch-mode": "cors",
	"sec-fetch-site": "same-origin",
}

// 用户名字符集：小写字母 + 数字
const chatgptOrgUkUsernameChars = "abcdefghijklmnopqrstuvwxyz0123456789"

func setChatgptOrgUkHeaders(req *http.Request) {
	for k, v := range chatgptOrgUkHeaders {
		req.Header.Set(k, v)
	}
	req.Header.Set("User-Agent", GetCurrentUA())
}

// chatgptOrgUkDomainsResponse 对应 GET /api/domains/public 响应
type chatgptOrgUkDomainsResponse struct {
	Success bool `json:"success"`
	Data    struct {
		Domains []struct {
			DomainName string `json:"domain_name"`
			IsActive   int    `json:"is_active"`
		} `json:"domains"`
	} `json:"data"`
}

// chatgptOrgUkInboxTokenResponse 对应 POST /api/inbox-token 响应
type chatgptOrgUkInboxTokenResponse struct {
	Success bool `json:"success"`
	Data    struct {
		Email string `json:"email"`
	} `json:"data"`
	Auth struct {
		Token string `json:"token"`
	} `json:"auth"`
}

// chatgptOrgUkEmailsResponse 对应 GET /api/emails 响应
type chatgptOrgUkEmailsResponse struct {
	Success bool `json:"success"`
	Data    struct {
		Emails []json.RawMessage `json:"emails"`
		Count  int               `json:"count"`
	} `json:"data"`
}

// chatgptOrgUkPackedToken 打包存储 gm_sid 与 inbox JWT
type chatgptOrgUkPackedToken struct {
	GmSid string `json:"gmSid"`
	Inbox string `json:"inbox"`
}

// chatgptOrgUkRandomUsername 本地生成随机用户名
func chatgptOrgUkRandomUsername(length int) (string, error) {
	var b strings.Builder
	n := big.NewInt(int64(len(chatgptOrgUkUsernameChars)))
	for i := 0; i < length; i++ {
		idx, err := rand.Int(rand.Reader, n)
		if err != nil {
			return "", err
		}
		b.WriteByte(chatgptOrgUkUsernameChars[idx.Int64()])
	}
	return b.String(), nil
}

// chatgptOrgUkExtractGmSid 从响应 set-cookie 中提取 gm_sid
func chatgptOrgUkExtractGmSid(resp *http.Response) string {
	for _, cookie := range resp.Cookies() {
		if cookie.Name == "gm_sid" {
			return cookie.Value
		}
	}
	return ""
}

// chatgptOrgUkFetchDomains 获取可用域名列表（过滤 is_active=1）
func chatgptOrgUkFetchDomains(client tls_client.HttpClient) ([]string, error) {
	req, err := http.NewRequest("GET", chatgptOrgUkBaseURL+"/domains/public", nil)
	if err != nil {
		return nil, err
	}
	setChatgptOrgUkHeaders(req)

	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	if err := CheckHTTPStatus(resp, "chatgpt-org-uk domains"); err != nil {
		return nil, err
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	var result chatgptOrgUkDomainsResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, err
	}
	if !result.Success {
		return nil, fmt.Errorf("chatgpt-org-uk: 获取域名失败")
	}

	domains := make([]string, 0, len(result.Data.Domains))
	for _, d := range result.Data.Domains {
		if d.IsActive == 1 && d.DomainName != "" {
			domains = append(domains, d.DomainName)
		}
	}
	if len(domains) == 0 {
		return nil, fmt.Errorf("chatgpt-org-uk: 无可用域名")
	}
	return domains, nil
}

// chatgptOrgUkCreateInbox 调用 inbox-token 创建收件箱，返回 inbox JWT 与 gm_sid
func chatgptOrgUkCreateInbox(client tls_client.HttpClient, email string) (inbox string, gmSid string, err error) {
	payload, err := json.Marshal(map[string]string{"email": email})
	if err != nil {
		return "", "", err
	}

	req, err := http.NewRequest("POST", chatgptOrgUkBaseURL+"/inbox-token", bytes.NewReader(payload))
	if err != nil {
		return "", "", err
	}
	setChatgptOrgUkHeaders(req)
	req.Header.Set("Content-Type", "application/json")

	resp, err := client.Do(req)
	if err != nil {
		return "", "", err
	}
	defer resp.Body.Close()

	if err := CheckHTTPStatus(resp, "chatgpt-org-uk inbox-token"); err != nil {
		return "", "", err
	}

	gmSid = chatgptOrgUkExtractGmSid(resp)

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", "", err
	}

	var result chatgptOrgUkInboxTokenResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return "", "", err
	}
	if !result.Success || result.Auth.Token == "" {
		return "", "", fmt.Errorf("chatgpt-org-uk: inbox-token 响应缺少 token")
	}
	return result.Auth.Token, gmSid, nil
}

// chatgptOrgUkParsePackedToken 解析打包的 token
func chatgptOrgUkParsePackedToken(packed string) (gmSid string, inbox string) {
	t := strings.TrimSpace(packed)
	if strings.HasPrefix(t, "{") {
		var p chatgptOrgUkPackedToken
		if json.Unmarshal([]byte(t), &p) == nil && p.Inbox != "" {
			return p.GmSid, p.Inbox
		}
	}
	return "", packed
}

func ChatgptOrgUkGenerate() (*CreatedMailbox, error) {
	client := HTTPClient()

	domains, err := chatgptOrgUkFetchDomains(client)
	if err != nil {
		return nil, err
	}

	idx, err := rand.Int(rand.Reader, big.NewInt(int64(len(domains))))
	if err != nil {
		return nil, err
	}
	domain := domains[idx.Int64()]

	username, err := chatgptOrgUkRandomUsername(10)
	if err != nil {
		return nil, err
	}
	email := username + "@" + domain

	inbox, gmSid, err := chatgptOrgUkCreateInbox(client, email)
	if err != nil {
		return nil, err
	}

	packed, err := json.Marshal(chatgptOrgUkPackedToken{GmSid: gmSid, Inbox: inbox})
	if err != nil {
		return nil, err
	}

	return &CreatedMailbox{Channel: "chatgpt-org-uk", Email: email, Token: string(packed)}, nil
}

func ChatgptOrgUkGetEmails(email string, token string) ([]NormEmail, error) {
	if token == "" {
		return nil, fmt.Errorf("missing inbox token")
	}
	encodedEmail := url.QueryEscape(email)

	gmSid, inbox := chatgptOrgUkParsePackedToken(token)
	if inbox == "" {
		return nil, fmt.Errorf("chatgpt-org-uk: inbox token 缺失")
	}
	client := HTTPClient()

	// gm_sid 丢失时重新创建 session
	if gmSid == "" {
		var err error
		inbox, gmSid, err = chatgptOrgUkCreateInbox(client, email)
		if err != nil {
			return nil, err
		}
	}

	fetchEmails := func(inboxToken string, sid string) ([]NormEmail, error) {
		req, err := http.NewRequest("GET", chatgptOrgUkBaseURL+"/emails?email="+encodedEmail, nil)
		if err != nil {
			return nil, err
		}
		setChatgptOrgUkHeaders(req)
		req.Header.Set("Cookie", fmt.Sprintf("gm_sid=%s", sid))
		req.Header.Set("x-inbox-token", inboxToken)

		resp, err := client.Do(req)
		if err != nil {
			return nil, err
		}
		defer resp.Body.Close()

		if err := CheckHTTPStatus(resp, "chatgpt-org-uk get emails"); err != nil {
			return nil, err
		}

		body, err := io.ReadAll(resp.Body)
		if err != nil {
			return nil, err
		}

		var result chatgptOrgUkEmailsResponse
		if err := json.Unmarshal(body, &result); err != nil {
			return nil, err
		}
		if !result.Success {
			return nil, fmt.Errorf("failed to get emails")
		}

		return NormalizeRawMessages(result.Data.Emails, email)
	}

	emails, err := fetchEmails(inbox, gmSid)
	if err == nil {
		return emails, nil
	}
	// 401/403 时重新创建 session 后重试一次
	if strings.Contains(err.Error(), ": 401") || strings.Contains(err.Error(), ": 403") {
		refreshed, sid, refreshErr := chatgptOrgUkCreateInbox(client, email)
		if refreshErr != nil {
			return nil, err
		}
		return fetchEmails(refreshed, sid)
	}
	return nil, err
}
