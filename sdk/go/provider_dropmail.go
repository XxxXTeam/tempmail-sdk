package tempemail

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/url"
	"os"
	"strings"
	"sync"
	"time"

	http "github.com/bogdanfinn/fhttp"
)

const (
	dropmailTokenGenerateURL = "https://dropmail.me/api/token/generate"
	dropmailTokenRenewURL    = "https://dropmail.me/api/token/renew"
	dropmailAutoTokenCache   = 50 * time.Minute
	dropmailRenewBefore      = 10 * time.Minute
)

const dropmailCreateSessionQuery = `mutation {introduceSession {id, expiresAt, addresses{id, address}}}`

const dropmailGetMailsQuery = `query ($id: ID!) {
  session(id:$id) {
    mails {
      id, rawSize, fromAddr, toAddr, receivedAt,
      text, headerFrom, headerSubject, html
    }
  }
}`

type dropmailGraphQLResponse struct {
	Data   json.RawMessage `json:"data"`
	Errors []struct {
		Message string `json:"message"`
	} `json:"errors"`
}

type dropmailSession struct {
	ID        string `json:"id"`
	ExpiresAt string `json:"expiresAt"`
	Addresses []struct {
		ID      string `json:"id"`
		Address string `json:"address"`
	} `json:"addresses"`
}

type dropmailIntroduceSessionResponse struct {
	IntroduceSession dropmailSession `json:"introduceSession"`
}

type dropmailSessionQueryResponse struct {
	Session struct {
		Mails []map[string]interface{} `json:"mails"`
	} `json:"session"`
}

type dropmailTokenAPIResponse struct {
	Token string `json:"token"`
	Error string `json:"error"`
}

var (
	dropmailAfMu     sync.Mutex
	dropmailAfCached *struct {
		value     string
		expiresAt time.Time
	}
	dropmailTokenJSON = map[string]string{
		"Accept":       "application/json",
		"Content-Type": "application/json",
		"Origin":       "https://dropmail.me",
		"Referer":      "https://dropmail.me/api/",
	}
)

func explicitDropmailAuthToken() string {
	cfg := GetConfig()
	if s := strings.TrimSpace(cfg.DropmailAuthToken); s != "" {
		return s
	}
	if s := strings.TrimSpace(os.Getenv("DROPMAIL_AUTH_TOKEN")); s != "" {
		return s
	}
	if s := strings.TrimSpace(os.Getenv("DROPMAIL_API_TOKEN")); s != "" {
		return s
	}
	return ""
}

func dropmailAutoTokenDisabled() bool {
	if GetConfig().DropmailDisableAutoToken {
		return true
	}
	v := strings.ToLower(strings.TrimSpace(os.Getenv("DROPMAIL_NO_AUTO_TOKEN")))
	return v == "1" || v == "true" || v == "yes"
}

func dropmailRenewLifetimeStr() string {
	if s := strings.TrimSpace(GetConfig().DropmailRenewLifetime); s != "" {
		return s
	}
	if s := strings.TrimSpace(os.Getenv("DROPMAIL_RENEW_LIFETIME")); s != "" {
		return s
	}
	return "1d"
}

func dropmailCacheDurationForLifetime(lifetime string) time.Duration {
	s := strings.ToLower(strings.TrimSpace(lifetime))
	switch s {
	case "1h":
		return 50 * time.Minute
	case "1d":
		return 23 * time.Hour
	case "7d", "30d", "90d":
		var days int
		if _, err := fmt.Sscanf(s, "%dd", &days); err == nil && days > 0 {
			return time.Duration(days-1) * 24 * time.Hour
		}
	}
	return dropmailAutoTokenCache
}

func dropmailPostJSON(u string, payload any) ([]byte, error) {
	body, err := json.Marshal(payload)
	if err != nil {
		return nil, err
	}
	req, err := http.NewRequest(http.MethodPost, u, bytes.NewReader(body))
	if err != nil {
		return nil, err
	}
	for k, v := range dropmailTokenJSON {
		req.Header.Set(k, v)
	}
	req.Header.Set("User-Agent", GetCurrentUA())

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	if resp.StatusCode >= 400 {
		return nil, fmt.Errorf("DropMail token HTTP %d", resp.StatusCode)
	}
	return raw, nil
}

func dropmailFetchAfToken() (string, error) {
	raw, err := dropmailPostJSON(dropmailTokenGenerateURL, map[string]string{"type": "af", "lifetime": "1h"})
	if err != nil {
		return "", err
	}
	var tr dropmailTokenAPIResponse
	if err := json.Unmarshal(raw, &tr); err != nil {
		return "", err
	}
	tok := strings.TrimSpace(tr.Token)
	if tok == "" || !strings.HasPrefix(tok, "af_") {
		if tr.Error != "" {
			return "", fmt.Errorf("DropMail token/generate: %s", tr.Error)
		}
		return "", fmt.Errorf("DropMail token/generate 未返回有效 af_ 令牌")
	}
	return tok, nil
}

func dropmailRenewAfToken(current, lifetime string) (string, error) {
	raw, err := dropmailPostJSON(dropmailTokenRenewURL, map[string]string{"token": current, "lifetime": lifetime})
	if err != nil {
		return "", err
	}
	var tr dropmailTokenAPIResponse
	if err := json.Unmarshal(raw, &tr); err != nil {
		return "", err
	}
	tok := strings.TrimSpace(tr.Token)
	if tok == "" || !strings.HasPrefix(tok, "af_") {
		if tr.Error != "" {
			return "", fmt.Errorf("DropMail token/renew: %s", tr.Error)
		}
		return "", fmt.Errorf("DropMail token/renew 未返回有效 af_ 令牌")
	}
	return tok, nil
}

func resolveDropmailAuthToken() (string, error) {
	if t := explicitDropmailAuthToken(); t != "" {
		return t, nil
	}
	if dropmailAutoTokenDisabled() {
		return "", fmt.Errorf("DropMail 已禁用自动令牌：请设置 DROPMAIL_AUTH_TOKEN 或 SetConfig(SDKConfig{DropmailAuthToken: \"af_...\"})，见 https://dropmail.me/api/")
	}

	dropmailAfMu.Lock()
	defer dropmailAfMu.Unlock()

	now := time.Now()
	if dropmailAfCached != nil && now.Before(dropmailAfCached.expiresAt.Add(-dropmailRenewBefore)) {
		return dropmailAfCached.value, nil
	}

	renewLife := dropmailRenewLifetimeStr()
	if dropmailAfCached != nil && dropmailAfCached.value != "" {
		if renewed, err := dropmailRenewAfToken(dropmailAfCached.value, renewLife); err == nil {
			dropmailAfCached = &struct {
				value     string
				expiresAt time.Time
			}{value: renewed, expiresAt: now.Add(dropmailCacheDurationForLifetime(renewLife))}
			return renewed, nil
		}
		dropmailAfCached = nil
	}

	tok, err := dropmailFetchAfToken()
	if err != nil {
		return "", err
	}
	dropmailAfCached = &struct {
		value     string
		expiresAt time.Time
	}{value: tok, expiresAt: now.Add(dropmailAutoTokenCache)}
	return tok, nil
}

// dropmailGraphQLRequest 执行 GraphQL 请求
func dropmailGraphQLRequest(query string, variables map[string]interface{}) (json.RawMessage, error) {
	af, err := resolveDropmailAuthToken()
	if err != nil {
		return nil, err
	}
	base := fmt.Sprintf("https://dropmail.me/api/graphql/%s", url.PathEscape(af))

	form := url.Values{}
	form.Set("query", query)
	if variables != nil {
		varsJSON, _ := json.Marshal(variables)
		form.Set("variables", string(varsJSON))
	}

	req, err := http.NewRequest(http.MethodPost, base, strings.NewReader(form.Encode()))
	if err != nil {
		return nil, err
	}
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	req.Header.Set("User-Agent", GetCurrentUA())

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
		return nil, fmt.Errorf("GraphQL request failed: %d", resp.StatusCode)
	}

	var result dropmailGraphQLResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, err
	}

	if len(result.Errors) > 0 {
		return nil, fmt.Errorf("GraphQL error: %s", result.Errors[0].Message)
	}

	return result.Data, nil
}

// dropmailGenerate 创建临时邮箱
// GraphQL mutation: introduceSession
func dropmailGenerate() (*EmailInfo, error) {
	data, err := dropmailGraphQLRequest(dropmailCreateSessionQuery, nil)
	if err != nil {
		return nil, err
	}

	var resp dropmailIntroduceSessionResponse
	if err := json.Unmarshal(data, &resp); err != nil {
		return nil, err
	}

	session := resp.IntroduceSession
	if session.ID == "" || len(session.Addresses) == 0 {
		return nil, fmt.Errorf("failed to generate email")
	}

	return &EmailInfo{
		Channel:   ChannelDropmail,
		Email:     session.Addresses[0].Address,
		token:     session.ID,
		ExpiresAt: session.ExpiresAt,
	}, nil
}

// dropmailFlattenMessage 将 dropmail 的邮件格式扁平化
func dropmailFlattenMessage(mail map[string]interface{}, recipientEmail string) map[string]interface{} {
	flat := map[string]interface{}{
		"id":          mail["id"],
		"from":        mail["fromAddr"],
		"to":          mail["toAddr"],
		"subject":     mail["headerSubject"],
		"text":        mail["text"],
		"html":        mail["html"],
		"received_at": mail["receivedAt"],
		"attachments": []interface{}{},
	}

	if flat["to"] == nil || flat["to"] == "" {
		flat["to"] = recipientEmail
	}

	return flat
}

// dropmailGetEmails 获取邮件列表
// GraphQL query: session(id) { mails {...} }
func dropmailGetEmails(token string, email string) ([]Email, error) {
	data, err := dropmailGraphQLRequest(dropmailGetMailsQuery, map[string]interface{}{
		"id": token,
	})
	if err != nil {
		return nil, err
	}

	var resp dropmailSessionQueryResponse
	if err := json.Unmarshal(data, &resp); err != nil {
		return nil, err
	}

	mails := resp.Session.Mails
	if len(mails) == 0 {
		return []Email{}, nil
	}

	emails := make([]Email, 0, len(mails))
	for _, mail := range mails {
		flat := dropmailFlattenMessage(mail, email)
		emails = append(emails, normalizeRawEmail(flat, email))
	}
	return emails, nil
}
