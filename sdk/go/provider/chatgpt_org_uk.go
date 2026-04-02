package provider

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/url"
	"regexp"
	"strings"

	http "github.com/bogdanfinn/fhttp"
	tls_client "github.com/bogdanfinn/tls-client"
)

const chatgptOrgUkBaseURL = "https://mail.chatgpt.org.uk/api"
const chatgptOrgUkHomeURL = "https://mail.chatgpt.org.uk/"

var chatgptOrgUkHeaders = map[string]string{
	"Accept":  "*/*",
	"Referer": "https://mail.chatgpt.org.uk/",
	"Origin":  "https://mail.chatgpt.org.uk",
	"DNT":     "1",
}

var chatgptOrgUkBrowserAuthRe = regexp.MustCompile(`__BROWSER_AUTH\s*=\s*(\{[\s\S]*?\})\s*;`)

func setChatgptOrgUkHeaders(req *http.Request) {
	for k, v := range chatgptOrgUkHeaders {
		req.Header.Set(k, v)
	}
	req.Header.Set("User-Agent", GetCurrentUA())
}

type ChatgptOrgUkGenerateResponse struct {
	Success bool `json:"success"`
	Data    struct {
		Email string `json:"email"`
	} `json:"data"`
	Auth *struct {
		Token string `json:"token"`
	} `json:"auth"`
}

type chatgptOrgUkInboxTokenResponse struct {
	Auth struct {
		Token string `json:"token"`
	} `json:"auth"`
}

type chatgptOrgUkEmailsResponse struct {
	Success bool `json:"success"`
	Data    struct {
		Emails []json.RawMessage `json:"emails"`
		Count  int               `json:"count"`
	} `json:"data"`
}

type chatgptOrgUkPackedToken struct {
	GmSid string `json:"gmSid"`
	Inbox string `json:"inbox"`
}

func chatgptOrgUkExtractBrowserAuth(html string) string {
	m := chatgptOrgUkBrowserAuthRe.FindStringSubmatch(html)
	if len(m) < 2 {
		return ""
	}
	var o struct {
		Token string `json:"token"`
	}
	if json.Unmarshal([]byte(m[1]), &o) != nil || o.Token == "" {
		return ""
	}
	return o.Token
}

func chatgptOrgUkFetchHomeSession(client tls_client.HttpClient) (gmSid string, browserToken string, err error) {
	req, err := http.NewRequest("GET", chatgptOrgUkHomeURL, nil)
	if err != nil {
		return "", "", err
	}
	setChatgptOrgUkHeaders(req)

	resp, err := client.Do(req)
	if err != nil {
		return "", "", err
	}
	defer resp.Body.Close()

	if err := CheckHTTPStatus(resp, "chatgpt-org-uk home"); err != nil {
		return "", "", err
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", "", err
	}

	for _, cookie := range resp.Cookies() {
		if cookie.Name == "gm_sid" {
			gmSid = cookie.Value
			break
		}
	}
	if gmSid == "" {
		return "", "", fmt.Errorf("failed to extract gm_sid cookie")
	}
	browserToken = chatgptOrgUkExtractBrowserAuth(string(body))
	if browserToken == "" {
		return "", "", fmt.Errorf("failed to extract __BROWSER_AUTH from homepage")
	}
	return gmSid, browserToken, nil
}

func chatgptOrgUkFetchHomeSessionWithRetry(client tls_client.HttpClient) (gmSid string, browserToken string, err error) {
	gmSid, browserToken, err = chatgptOrgUkFetchHomeSession(client)
	if err == nil {
		return gmSid, browserToken, nil
	}
	msg := strings.ToLower(err.Error())
	if strings.Contains(msg, "401") || strings.Contains(msg, "extract") || strings.Contains(msg, "gm_sid") {
		return chatgptOrgUkFetchHomeSession(client)
	}
	return "", "", err
}

func chatgptOrgUkFetchInboxToken(client tls_client.HttpClient, email string, gmSid string) (string, error) {
	payload, err := json.Marshal(map[string]string{"email": email})
	if err != nil {
		return "", err
	}

	req, err := http.NewRequest("POST", chatgptOrgUkBaseURL+"/inbox-token", bytes.NewReader(payload))
	if err != nil {
		return "", err
	}
	setChatgptOrgUkHeaders(req)
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Cookie", fmt.Sprintf("gm_sid=%s", gmSid))

	resp, err := client.Do(req)
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()

	if err := CheckHTTPStatus(resp, "chatgpt-org-uk inbox-token"); err != nil {
		return "", err
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", err
	}

	var result chatgptOrgUkInboxTokenResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return "", err
	}
	if result.Auth.Token == "" {
		return "", fmt.Errorf("failed to get inbox token")
	}
	return result.Auth.Token, nil
}

func chatgptOrgUkFetchInboxTokenWithRetry(client tls_client.HttpClient, email string) (inbox string, gmSid string, err error) {
	gmSid, _, err = chatgptOrgUkFetchHomeSessionWithRetry(client)
	if err != nil {
		return "", "", err
	}

	token, err := chatgptOrgUkFetchInboxToken(client, email, gmSid)
	if err == nil {
		return token, gmSid, nil
	}
	if strings.Contains(err.Error(), ": 401") {
		gmSid, _, err = chatgptOrgUkFetchHomeSessionWithRetry(client)
		if err != nil {
			return "", "", err
		}
		token, err = chatgptOrgUkFetchInboxToken(client, email, gmSid)
		if err != nil {
			return "", "", err
		}
		return token, gmSid, nil
	}
	return "", "", err
}

func chatgptOrgUkParsePackedToken(packed string) (gmSid string, inbox string) {
	t := strings.TrimSpace(packed)
	if strings.HasPrefix(t, "{") {
		var p chatgptOrgUkPackedToken
		if json.Unmarshal([]byte(t), &p) == nil && p.GmSid != "" && p.Inbox != "" {
			return p.GmSid, p.Inbox
		}
	}
	return "", packed
}

func ChatgptOrgUkGenerate() (*CreatedMailbox, error) {
	client := HTTPClient()

	gmSid, browserToken, err := chatgptOrgUkFetchHomeSessionWithRetry(client)
	if err != nil {
		return nil, err
	}

	req, err := http.NewRequest("GET", chatgptOrgUkBaseURL+"/generate-email", nil)
	if err != nil {
		return nil, err
	}
	setChatgptOrgUkHeaders(req)
	req.Header.Set("Cookie", fmt.Sprintf("gm_sid=%s", gmSid))
	req.Header.Set("X-Inbox-Token", browserToken)

	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	if err := CheckHTTPStatus(resp, "chatgpt-org-uk generate"); err != nil {
		return nil, err
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	var result ChatgptOrgUkGenerateResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, err
	}

	if !result.Success {
		return nil, fmt.Errorf("failed to generate email")
	}

	email := result.Data.Email
	if email == "" {
		return nil, fmt.Errorf("failed to generate email")
	}

	var inboxJwt string
	if result.Auth != nil && result.Auth.Token != "" {
		inboxJwt = result.Auth.Token
	} else {
		inboxJwt, err = chatgptOrgUkFetchInboxToken(client, email, gmSid)
		if err != nil {
			return nil, err
		}
	}

	packed, err := json.Marshal(chatgptOrgUkPackedToken{GmSid: gmSid, Inbox: inboxJwt})
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
	client := HTTPClient()
	if gmSid == "" {
		var err error
		gmSid, _, err = chatgptOrgUkFetchHomeSessionWithRetry(client)
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
	if strings.Contains(err.Error(), ": 401") {
		refreshed, sid, refreshErr := chatgptOrgUkFetchInboxTokenWithRetry(client, email)
		if refreshErr != nil {
			return nil, err
		}
		return fetchEmails(refreshed, sid)
	}
	return nil, err
}
