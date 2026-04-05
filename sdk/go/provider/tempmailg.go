package provider

import (
	"bytes"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"io"
	"net/url"
	"regexp"
	"sort"
	"strings"

	http "github.com/bogdanfinn/fhttp"
	tls_client "github.com/bogdanfinn/tls-client"
)

// tempmailg.com：邮箱访问凭证来自首次 GET /public/{locale} 响应里的 Set-Cookie（Laravel 加密会话，外形常被误称为 JWT），
// 与 meta csrf-token 配合 POST /public/get_messages 轮询收件。换新邮箱须清空站点 Cookie 后重新 GET（等价于独立 HTTP 会话）；
// 本实现使用 HTTPClientNoCookieJar，避免与全局 Cookie 罐混用导致第二次 Generate 仍挂在旧邮箱上。

const tempmailgOrigin = "https://tempmailg.com"

var tempmailgCsrfMetaRe = regexp.MustCompile(`<meta\s+name="csrf-token"\s+content="([^"]+)"`)

// tempmailgSess：CookieHdr 为首次 GET（及首包 get_messages）合并后的 Cookie 串，即服务端下发的邮箱会话凭证快照。
type tempmailgSess struct {
	Locale    string `json:"l"`
	CookieHdr string `json:"c"`
	CSRF      string `json:"s"`
}

const tempmailgTokPrefix = "tmg1:"

func tempmailgLocale(domain *string) string {
	if domain == nil {
		return "zh"
	}
	s := strings.TrimSpace(*domain)
	if s == "" {
		return "zh"
	}
	// 允许 "zh"、"en" 等路径段，禁止注入
	if strings.ContainsAny(s, "/?#\\") {
		return "zh"
	}
	return s
}

func tempmailgEncodeSess(s *tempmailgSess) (string, error) {
	b, err := json.Marshal(s)
	if err != nil {
		return "", err
	}
	return tempmailgTokPrefix + base64.StdEncoding.EncodeToString(b), nil
}

func tempmailgDecodeSess(tok string) (*tempmailgSess, error) {
	if !strings.HasPrefix(tok, tempmailgTokPrefix) {
		return nil, fmt.Errorf("tempmailg: invalid session token")
	}
	raw, err := base64.StdEncoding.DecodeString(tok[len(tempmailgTokPrefix):])
	if err != nil {
		return nil, fmt.Errorf("tempmailg: invalid session token")
	}
	var s tempmailgSess
	if err := json.Unmarshal(raw, &s); err != nil {
		return nil, fmt.Errorf("tempmailg: invalid session token")
	}
	if s.CookieHdr == "" || s.CSRF == "" {
		return nil, fmt.Errorf("tempmailg: invalid session token")
	}
	return &s, nil
}

func tempmailgCookieMap(hdr string) map[string]string {
	m := make(map[string]string)
	for _, part := range strings.Split(hdr, ";") {
		part = strings.TrimSpace(part)
		if part == "" {
			continue
		}
		i := strings.Index(part, "=")
		if i <= 0 || i >= len(part)-1 {
			continue
		}
		k := strings.TrimSpace(part[:i])
		v := strings.TrimSpace(part[i+1:])
		if k != "" {
			m[k] = v
		}
	}
	return m
}

func tempmailgMergeCookies(prev string, cookies []*http.Cookie) string {
	m := tempmailgCookieMap(prev)
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

func tempmailgXsrfFromCookies(cookieHdr string) string {
	m := tempmailgCookieMap(cookieHdr)
	for _, name := range []string{"XSRF-TOKEN", "xsrf-token"} {
		if v := m[name]; v != "" {
			return v
		}
	}
	for k, v := range m {
		if strings.EqualFold(k, "XSRF-TOKEN") && v != "" {
			return v
		}
	}
	return ""
}

func tempmailgSetPageHeaders(req *http.Request, referer string) {
	req.Header.Set("User-Agent", GetCurrentUA())
	req.Header.Set("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
	req.Header.Set("Cache-Control", "no-cache")
	req.Header.Set("DNT", "1")
	req.Header.Set("Pragma", "no-cache")
	req.Header.Set("Referer", referer)
	req.Header.Set("Upgrade-Insecure-Requests", "1")
}

func tempmailgSetAPIHeaders(req *http.Request, referer, cookieHdr, xsrf string) {
	req.Header.Set("User-Agent", GetCurrentUA())
	req.Header.Set("Accept", "application/json, text/plain, */*")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Origin", tempmailgOrigin)
	req.Header.Set("Referer", referer)
	req.Header.Set("Cache-Control", "no-cache")
	req.Header.Set("Pragma", "no-cache")
	req.Header.Set("DNT", "1")
	req.Header.Set("Cookie", cookieHdr)
	if xsrf != "" {
		req.Header.Set("X-XSRF-TOKEN", xsrf)
	}
}

func tempmailgParseCSRF(html string) (string, error) {
	m := tempmailgCsrfMetaRe.FindStringSubmatch(html)
	if len(m) < 2 {
		return "", fmt.Errorf("tempmailg: csrf-token not found in page")
	}
	t := strings.TrimSpace(m[1])
	if t == "" {
		return "", fmt.Errorf("tempmailg: empty csrf-token")
	}
	return t, nil
}

func tempmailgHTTPClient() tls_client.HttpClient {
	if HTTPClientNoCookieJar != nil {
		return HTTPClientNoCookieJar()
	}
	return HTTPClient()
}

// TempmailgGenerate 无 Cookie 罐 GET /public/{locale} 拿会话凭证，再 POST get_messages 解析 mailbox。
// opts.Domain 可为语言路径，如 zh、en，默认 zh。
func TempmailgGenerate(domain *string) (*CreatedMailbox, error) {
	locale := tempmailgLocale(domain)
	pageURL := tempmailgOrigin + "/public/" + url.PathEscape(locale)
	req, err := http.NewRequest("GET", pageURL, nil)
	if err != nil {
		return nil, err
	}
	tempmailgSetPageHeaders(req, pageURL)

	client := tempmailgHTTPClient()
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if err := CheckHTTPStatus(resp, "tempmailg page"); err != nil {
		return nil, err
	}
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	html := string(body)
	csrf, err := tempmailgParseCSRF(html)
	if err != nil {
		return nil, err
	}
	cookieHdr := tempmailgMergeCookies("", resp.Cookies())
	xsrf := tempmailgXsrfFromCookies(cookieHdr)
	if xsrf == "" {
		return nil, fmt.Errorf("tempmailg: missing XSRF-TOKEN cookie")
	}

	rawBody, err := json.Marshal(map[string]string{"_token": csrf})
	if err != nil {
		return nil, err
	}
	postURL := tempmailgOrigin + "/public/get_messages"
	req2, err := http.NewRequest("POST", postURL, bytes.NewReader(rawBody))
	if err != nil {
		return nil, err
	}
	tempmailgSetAPIHeaders(req2, pageURL, cookieHdr, xsrf)

	resp2, err := client.Do(req2)
	if err != nil {
		return nil, err
	}
	defer resp2.Body.Close()
	if err := CheckHTTPStatus(resp2, "tempmailg get_messages"); err != nil {
		return nil, err
	}
	apiBody, err := io.ReadAll(resp2.Body)
	if err != nil {
		return nil, err
	}
	var wrap struct {
		Status   bool              `json:"status"`
		Mailbox  string            `json:"mailbox"`
		Messages []json.RawMessage `json:"messages"`
	}
	if err := json.Unmarshal(apiBody, &wrap); err != nil {
		return nil, err
	}
	if !wrap.Status || wrap.Mailbox == "" {
		return nil, fmt.Errorf("tempmailg: create mailbox failed")
	}
	cookieHdr = tempmailgMergeCookies(cookieHdr, resp2.Cookies())
	xsrf2 := tempmailgXsrfFromCookies(cookieHdr)
	if xsrf2 != "" {
		xsrf = xsrf2
	}
	sess := &tempmailgSess{
		Locale:    locale,
		CookieHdr: cookieHdr,
		CSRF:      csrf,
	}
	tok, err := tempmailgEncodeSess(sess)
	if err != nil {
		return nil, err
	}
	return &CreatedMailbox{
		Channel: "tempmailg",
		Email:   wrap.Mailbox,
		Token:   tok,
	}, nil
}

// TempmailgGetEmails 使用创建时保存的会话调用 get_messages。
func TempmailgGetEmails(email, token string) ([]NormEmail, error) {
	sess, err := tempmailgDecodeSess(token)
	if err != nil {
		return nil, err
	}
	locale := sess.Locale
	if locale == "" {
		locale = "zh"
	}
	pageURL := tempmailgOrigin + "/public/" + url.PathEscape(locale)

	rawBody, err := json.Marshal(map[string]string{"_token": sess.CSRF})
	if err != nil {
		return nil, err
	}
	postURL := tempmailgOrigin + "/public/get_messages"
	req, err := http.NewRequest("POST", postURL, bytes.NewReader(rawBody))
	if err != nil {
		return nil, err
	}
	xsrf := tempmailgXsrfFromCookies(sess.CookieHdr)
	tempmailgSetAPIHeaders(req, pageURL, sess.CookieHdr, xsrf)

	client := tempmailgHTTPClient()
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if err := CheckHTTPStatus(resp, "tempmailg get_messages"); err != nil {
		return nil, err
	}
	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	var wrap struct {
		Status   bool              `json:"status"`
		Mailbox  string            `json:"mailbox"`
		Messages []json.RawMessage `json:"messages"`
	}
	if err := json.Unmarshal(raw, &wrap); err != nil {
		return nil, err
	}
	if !wrap.Status {
		return nil, fmt.Errorf("tempmailg: get_messages failed")
	}

	if wrap.Mailbox != "" && !strings.EqualFold(strings.TrimSpace(wrap.Mailbox), strings.TrimSpace(email)) {
		return nil, fmt.Errorf("tempmailg: mailbox mismatch")
	}

	out := make([]NormEmail, 0, len(wrap.Messages))
	for _, rm := range wrap.Messages {
		var m map[string]interface{}
		if err := json.Unmarshal(rm, &m); err != nil {
			continue
		}
		out = append(out, NormalizeMap(m, email))
	}
	return out, nil
}
