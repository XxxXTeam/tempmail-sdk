package provider

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"regexp"
	"sort"
	"strings"

	http "github.com/bogdanfinn/fhttp"
	tls_client "github.com/bogdanfinn/tls-client"
)

const tempmailoBase = "https://tempmailo.com"

var (
	tempmailoTokenRe    = regexp.MustCompile(`(?i)name="__RequestVerificationToken"[^>]*value="([^"]+)"`)
	tempmailoTokenRevRe = regexp.MustCompile(`(?i)value="([^"]+)"[^>]*name="__RequestVerificationToken"`)
)

type tempmailoSession struct {
	VerificationToken string `json:"verificationToken"`
	Cookie            string `json:"cookie"`
}

func tempmailoHTTPClient() tls_client.HttpClient {
	if HTTPClientNoCookieJar != nil {
		return HTTPClientNoCookieJar()
	}
	return HTTPClient()
}

func tempmailoCookieMap(hdr string) map[string]string {
	out := make(map[string]string)
	for _, part := range strings.Split(hdr, ";") {
		part = strings.TrimSpace(part)
		i := strings.Index(part, "=")
		if i > 0 {
			out[part[:i]] = part[i+1:]
		}
	}
	return out
}

func tempmailoMergeCookies(prev string, cookies []*http.Cookie) string {
	m := tempmailoCookieMap(prev)
	for _, c := range cookies {
		if c != nil && c.Name != "" {
			m[c.Name] = c.Value
		}
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

func tempmailoHeaders(req *http.Request, cookie string) {
	req.Header.Set("User-Agent", GetCurrentUA())
	req.Header.Set("Accept", "text/html,application/json,*/*;q=0.8")
	req.Header.Set("Referer", tempmailoBase+"/")
	if cookie != "" {
		req.Header.Set("Cookie", cookie)
	}
}

func tempmailoParseToken(html string) (string, error) {
	if m := tempmailoTokenRe.FindStringSubmatch(html); len(m) == 2 {
		return m[1], nil
	}
	if m := tempmailoTokenRevRe.FindStringSubmatch(html); len(m) == 2 {
		return m[1], nil
	}
	return "", fmt.Errorf("tempmailo: verification token not found")
}

func tempmailoEncodeSession(s tempmailoSession) (string, error) {
	b, err := json.Marshal(s)
	if err != nil {
		return "", err
	}
	return string(b), nil
}

func tempmailoDecodeSession(token string) (tempmailoSession, error) {
	var s tempmailoSession
	if err := json.Unmarshal([]byte(token), &s); err != nil {
		return s, fmt.Errorf("tempmailo: invalid session token")
	}
	if strings.TrimSpace(s.VerificationToken) == "" || strings.TrimSpace(s.Cookie) == "" {
		return s, fmt.Errorf("tempmailo: invalid session token")
	}
	return s, nil
}

func TempmailoGenerate() (*CreatedMailbox, error) {
	req, err := http.NewRequest("GET", tempmailoBase+"/", nil)
	if err != nil {
		return nil, err
	}
	tempmailoHeaders(req, "")
	resp, err := tempmailoHTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("tempmailo home: %d", resp.StatusCode)
	}
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	verificationToken, err := tempmailoParseToken(string(body))
	if err != nil {
		return nil, err
	}
	cookie := tempmailoMergeCookies("", resp.Cookies())

	req2, err := http.NewRequest("GET", tempmailoBase+"/changemail?_r=1", nil)
	if err != nil {
		return nil, err
	}
	tempmailoHeaders(req2, cookie)
	req2.Header.Set("Accept", "text/plain,*/*;q=0.8")
	req2.Header.Set("RequestVerificationToken", verificationToken)
	resp2, err := tempmailoHTTPClient().Do(req2)
	if err != nil {
		return nil, err
	}
	defer resp2.Body.Close()
	if resp2.StatusCode < 200 || resp2.StatusCode >= 300 {
		return nil, fmt.Errorf("tempmailo changemail: %d", resp2.StatusCode)
	}
	cookie = tempmailoMergeCookies(cookie, resp2.Cookies())
	body2, err := io.ReadAll(resp2.Body)
	if err != nil {
		return nil, err
	}
	email := strings.TrimSpace(string(body2))
	if !strings.Contains(email, "@") {
		return nil, fmt.Errorf("tempmailo: invalid email response")
	}
	token, err := tempmailoEncodeSession(tempmailoSession{
		VerificationToken: verificationToken,
		Cookie:            cookie,
	})
	if err != nil {
		return nil, err
	}
	return &CreatedMailbox{Channel: "tempmailo", Email: email, Token: token}, nil
}

func TempmailoGetEmails(token string, email string) ([]NormEmail, error) {
	session, err := tempmailoDecodeSession(token)
	if err != nil {
		return nil, err
	}
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("tempmailo: empty email")
	}
	reqBody, _ := json.Marshal(map[string]string{"mail": email})
	req, err := http.NewRequest("POST", tempmailoBase+"/", bytes.NewReader(reqBody))
	if err != nil {
		return nil, err
	}
	tempmailoHeaders(req, session.Cookie)
	req.Header.Set("Accept", "application/json,*/*;q=0.8")
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("RequestVerificationToken", session.VerificationToken)
	resp, err := tempmailoHTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("tempmailo messages: %d", resp.StatusCode)
	}
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	var rows []map[string]interface{}
	if err := json.Unmarshal(body, &rows); err != nil {
		return nil, err
	}
	return normEmailsFromMaps(rows, email), nil
}
