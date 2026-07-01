package provider

import (
	crand "crypto/rand"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"io"
	"math/big"
	"net/url"
	"sort"
	"strings"

	http "github.com/bogdanfinn/fhttp"
	tls_client "github.com/bogdanfinn/tls-client"
)

const (
	zhujumpBase         = "https://mail.zhujump.com"
	zhujumpLoginReferer = "https://mail.zhujump.com/zh-CN/login"
	zhujumpTokenPrefix  = "zhj1:"
	zhujumpExpiry1Hour  = 60 * 60 * 1000
)

type zhujumpSession struct {
	Cookie  string `json:"c"`
	EmailID string `json:"i"`
	BaseURL string `json:"b,omitempty"`
}

func zhujumpHTTPClient() tls_client.HttpClient {
	if HTTPClientNoCookieJar != nil {
		return HTTPClientNoCookieJar()
	}
	return HTTPClient()
}

func zhujumpJSONHeaders(req *http.Request, baseURL string, cookie string) {
	req.Header.Set("Accept", "application/json")
	req.Header.Set("Origin", baseURL)
	req.Header.Set("Referer", strings.TrimRight(baseURL, "/")+"/zh-CN/login")
	req.Header.Set("User-Agent", GetCurrentUA())
	if cookie != "" {
		req.Header.Set("Cookie", cookie)
	}
}

func zhujumpRandomString(prefix string, n int) (string, error) {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	var b strings.Builder
	b.Grow(len(prefix) + n)
	b.WriteString(prefix)
	max := big.NewInt(int64(len(chars)))
	for i := 0; i < n; i++ {
		v, err := crand.Int(crand.Reader, max)
		if err != nil {
			return "", err
		}
		b.WriteByte(chars[v.Int64()])
	}
	return b.String(), nil
}

func zhujumpRandomPassword() (string, error) {
	mid, err := zhujumpRandomString("", 12)
	if err != nil {
		return "", err
	}
	return "Sdk!" + mid + "A1", nil
}

func zhujumpCookieMap(hdr string) map[string]string {
	out := make(map[string]string)
	for _, part := range strings.Split(hdr, ";") {
		part = strings.TrimSpace(part)
		if part == "" {
			continue
		}
		i := strings.Index(part, "=")
		if i <= 0 {
			continue
		}
		out[strings.TrimSpace(part[:i])] = strings.TrimSpace(part[i+1:])
	}
	return out
}

func zhujumpMergeCookies(prev string, cookies []*http.Cookie) string {
	m := zhujumpCookieMap(prev)
	for _, c := range cookies {
		if c == nil || c.Name == "" {
			continue
		}
		m[c.Name] = c.Value
	}
	keys := make([]string, 0, len(m))
	for k := range m {
		keys = append(keys, k)
	}
	sort.Strings(keys)
	parts := make([]string, 0, len(keys))
	for _, k := range keys {
		parts = append(parts, k+"="+m[k])
	}
	return strings.Join(parts, "; ")
}

func zhujumpEncodeSession(s *zhujumpSession) (string, error) {
	b, err := json.Marshal(s)
	if err != nil {
		return "", err
	}
	return zhujumpTokenPrefix + base64.StdEncoding.EncodeToString(b), nil
}

func zhujumpDecodeSession(token string) (*zhujumpSession, error) {
	if !strings.HasPrefix(token, zhujumpTokenPrefix) {
		return nil, fmt.Errorf("zhujump: invalid session token")
	}
	raw, err := base64.StdEncoding.DecodeString(token[len(zhujumpTokenPrefix):])
	if err != nil {
		return nil, fmt.Errorf("zhujump: invalid session token")
	}
	var s zhujumpSession
	if err := json.Unmarshal(raw, &s); err != nil {
		return nil, fmt.Errorf("zhujump: invalid session token")
	}
	if strings.TrimSpace(s.Cookie) == "" || strings.TrimSpace(s.EmailID) == "" {
		return nil, fmt.Errorf("zhujump: invalid session token")
	}
	if strings.TrimSpace(s.BaseURL) == "" {
		s.BaseURL = zhujumpBase
	}
	s.BaseURL = strings.TrimRight(s.BaseURL, "/")
	return &s, nil
}

func zhujumpLoginAndCreate(baseURL string, domain string, expiryTime *int) (*CreatedMailbox, error) {
	client := zhujumpHTTPClient()
	baseURL = strings.TrimRight(baseURL, "/")
	loginReferer := baseURL + "/zh-CN/login"

	username, err := zhujumpRandomString("sdk", 10)
	if err != nil {
		return nil, err
	}
	password, err := zhujumpRandomPassword()
	if err != nil {
		return nil, err
	}

	cookieHdr := ""

	registerBody, _ := json.Marshal(map[string]string{
		"username":       username,
		"password":       password,
		"turnstileToken": "",
	})
	req1, err := http.NewRequest("POST", baseURL+"/api/auth/register", strings.NewReader(string(registerBody)))
	if err != nil {
		return nil, err
	}
	zhujumpJSONHeaders(req1, baseURL, "")
	req1.Header.Set("Content-Type", "application/json")
	resp1, err := client.Do(req1)
	if err != nil {
		return nil, err
	}
	defer resp1.Body.Close()
	if err := CheckHTTPStatus(resp1, "zhujump register"); err != nil {
		return nil, err
	}
	_, _ = io.Copy(io.Discard, resp1.Body)
	cookieHdr = zhujumpMergeCookies(cookieHdr, resp1.Cookies())

	req2, err := http.NewRequest("GET", baseURL+"/api/auth/csrf", nil)
	if err != nil {
		return nil, err
	}
	zhujumpJSONHeaders(req2, baseURL, cookieHdr)
	resp2, err := client.Do(req2)
	if err != nil {
		return nil, err
	}
	defer resp2.Body.Close()
	if err := CheckHTTPStatus(resp2, "zhujump csrf"); err != nil {
		return nil, err
	}
	body2, err := io.ReadAll(resp2.Body)
	if err != nil {
		return nil, err
	}
	cookieHdr = zhujumpMergeCookies(cookieHdr, resp2.Cookies())
	var csrfResp struct {
		CSRFToken string `json:"csrfToken"`
	}
	if err := json.Unmarshal(body2, &csrfResp); err != nil {
		return nil, err
	}
	if strings.TrimSpace(csrfResp.CSRFToken) == "" {
		return nil, fmt.Errorf("zhujump: csrf token missing")
	}

	form := url.Values{}
	form.Set("username", username)
	form.Set("password", password)
	form.Set("turnstileToken", "")
	form.Set("redirect", "false")
	form.Set("csrfToken", csrfResp.CSRFToken)
	form.Set("callbackUrl", loginReferer)
	req3, err := http.NewRequest("POST", baseURL+"/api/auth/callback/credentials?", strings.NewReader(form.Encode()))
	if err != nil {
		return nil, err
	}
	zhujumpJSONHeaders(req3, baseURL, cookieHdr)
	req3.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	req3.Header.Set("x-auth-return-redirect", "1")
	resp3, err := client.Do(req3)
	if err != nil {
		return nil, err
	}
	defer resp3.Body.Close()
	if err := CheckHTTPStatus(resp3, "zhujump login"); err != nil {
		return nil, err
	}
	_, _ = io.Copy(io.Discard, resp3.Body)
	cookieHdr = zhujumpMergeCookies(cookieHdr, resp3.Cookies())

	req4, err := http.NewRequest("GET", baseURL+"/api/auth/session", nil)
	if err != nil {
		return nil, err
	}
	zhujumpJSONHeaders(req4, baseURL, cookieHdr)
	resp4, err := client.Do(req4)
	if err != nil {
		return nil, err
	}
	defer resp4.Body.Close()
	if err := CheckHTTPStatus(resp4, "zhujump session"); err != nil {
		return nil, err
	}
	body4, err := io.ReadAll(resp4.Body)
	if err != nil {
		return nil, err
	}
	var sessionResp struct {
		User struct {
			Username string `json:"username"`
		} `json:"user"`
	}
	if err := json.Unmarshal(body4, &sessionResp); err != nil {
		return nil, err
	}
	if sessionResp.User.Username != username {
		return nil, fmt.Errorf("zhujump: login verification failed")
	}

	local, err := zhujumpRandomString("sdk", 10)
	if err != nil {
		return nil, err
	}
	generateReq := map[string]interface{}{
		"name":   local,
		"domain": domain,
	}
	if expiryTime != nil {
		generateReq["expiryTime"] = *expiryTime
	}
	generateBody, _ := json.Marshal(generateReq)
	req5, err := http.NewRequest("POST", baseURL+"/api/emails/generate", strings.NewReader(string(generateBody)))
	if err != nil {
		return nil, err
	}
	zhujumpJSONHeaders(req5, baseURL, cookieHdr)
	req5.Header.Set("Content-Type", "application/json")
	resp5, err := client.Do(req5)
	if err != nil {
		return nil, err
	}
	defer resp5.Body.Close()
	if err := CheckHTTPStatus(resp5, "zhujump generate"); err != nil {
		return nil, err
	}
	body5, err := io.ReadAll(resp5.Body)
	if err != nil {
		return nil, err
	}
	var generateResp struct {
		ID    string `json:"id"`
		Email string `json:"email"`
	}
	if err := json.Unmarshal(body5, &generateResp); err != nil {
		return nil, err
	}
	if strings.TrimSpace(generateResp.ID) == "" || strings.TrimSpace(generateResp.Email) == "" {
		return nil, fmt.Errorf("zhujump: invalid generate response")
	}

	token, err := zhujumpEncodeSession(&zhujumpSession{Cookie: cookieHdr, EmailID: generateResp.ID, BaseURL: baseURL})
	if err != nil {
		return nil, err
	}
	return &CreatedMailbox{
		Email: generateResp.Email,
		Token: token,
	}, nil
}

func ZhujumpGenerate(domain string, channel string) (*CreatedMailbox, error) {
	mailbox, err := zhujumpLoginAndCreate(zhujumpBase, domain, nil)
	if err != nil {
		return nil, err
	}
	mailbox.Channel = channel
	return mailbox, nil
}

func MoemailGenerate(baseURL string, domain string, channel string, expiryTime int) (*CreatedMailbox, error) {
	mailbox, err := zhujumpLoginAndCreate(baseURL, domain, &expiryTime)
	if err != nil {
		return nil, err
	}
	mailbox.Channel = channel
	return mailbox, nil
}

func zhujumpMessageHasBody(msg map[string]interface{}) bool {
	if msg == nil {
		return false
	}
	content, _ := msg["content"].(string)
	html, _ := msg["html"].(string)
	return content != "" || html != ""
}

func zhujumpFetchMessageDetail(baseURL string, cookie string, emailID string, messageID string) (map[string]interface{}, error) {
	req, err := http.NewRequest("GET", baseURL+"/api/emails/"+url.PathEscape(emailID)+"/"+url.PathEscape(messageID), nil)
	if err != nil {
		return nil, err
	}
	zhujumpJSONHeaders(req, baseURL, cookie)
	resp, err := zhujumpHTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if err := CheckHTTPStatus(resp, "zhujump get message"); err != nil {
		return nil, err
	}
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	var detailResp struct {
		Message map[string]interface{} `json:"message"`
	}
	if err := json.Unmarshal(body, &detailResp); err != nil {
		return nil, err
	}
	return detailResp.Message, nil
}

func ZhujumpGetEmails(email, token string) ([]NormEmail, error) {
	session, err := zhujumpDecodeSession(token)
	if err != nil {
		return nil, err
	}

	req, err := http.NewRequest("GET", session.BaseURL+"/api/emails/"+url.PathEscape(session.EmailID), nil)
	if err != nil {
		return nil, err
	}
	zhujumpJSONHeaders(req, session.BaseURL, session.Cookie)
	resp, err := zhujumpHTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if err := CheckHTTPStatus(resp, "zhujump get emails"); err != nil {
		return nil, err
	}
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	var detailResp struct {
		Messages []map[string]interface{} `json:"messages"`
	}
	if err := json.Unmarshal(body, &detailResp); err != nil {
		return nil, err
	}
	out := make([]NormEmail, 0, len(detailResp.Messages))
	for _, msg := range detailResp.Messages {
		if id := strings.TrimSpace(fmt.Sprint(msg["id"])); id != "" && !zhujumpMessageHasBody(msg) {
			if detail, err := zhujumpFetchMessageDetail(session.BaseURL, session.Cookie, session.EmailID, id); err == nil && detail != nil {
				for k, v := range detail {
					msg[k] = v
				}
			}
		}
		if _, ok := msg["to_address"]; !ok || msg["to_address"] == nil {
			msg["to_address"] = email
		}
		out = append(out, NormalizeMap(msg, email))
	}
	return out, nil
}
