package provider

import (
	"encoding/base64"
	"encoding/json"
	"fmt"
	"html"
	"io"
	"net/url"
	"regexp"
	"sort"
	"strings"

	http "github.com/bogdanfinn/fhttp"
	tls_client "github.com/bogdanfinn/tls-client"
)

// moakt.com：凭证为 tm_session 等 Cookie；先 GET 语言首页再 GET 收件箱解析 #email-address；
// 邮件列表解析 /{locale}/email/{uuid} 链接，详情 GET .../html 解析 .message-container。

const moaktOrigin = "https://www.moakt.com"

type moaktSess struct {
	Locale    string `json:"l"`
	CookieHdr string `json:"c"`
}

const moaktTokPrefix = "mok1:"

var (
	moaktEmailDivRe  = regexp.MustCompile(`(?is)<div\s+id="email-address"\s*>([^<]+)</div>`)
	moaktHrefEmailRe = regexp.MustCompile(
		`href="(/[^"]+/email/[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})"`,
	)
	moaktTitleRe    = regexp.MustCompile(`(?is)<li\s+class="title"\s*>([^<]*)</li>`)
	moaktDateRe     = regexp.MustCompile(`(?is)<li\s+class="date"[^>]*>[\s\S]*?<span[^>]*>([^<]+)</span>`)
	moaktSenderRe   = regexp.MustCompile(`(?is)<li\s+class="sender"[^>]*>[\s\S]*?<span[^>]*>([\s\S]*?)</span>\s*</li>`)
	moaktBodyRe     = regexp.MustCompile(`(?is)<div\s+class="email-body"\s*>([\s\S]*?)</div>`)
	moaktFromAddrRe = regexp.MustCompile(`<([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})>`)
)

func moaktLocale(domain *string) string {
	if domain == nil {
		return "zh"
	}
	s := strings.TrimSpace(*domain)
	if s == "" {
		return "zh"
	}
	if strings.ContainsAny(s, "/?#\\") {
		return "zh"
	}
	return s
}

func moaktCookieMap(hdr string) map[string]string {
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

func moaktMergeCookies(prev string, cookies []*http.Cookie) string {
	m := moaktCookieMap(prev)
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

func moaktEncodeSess(s *moaktSess) (string, error) {
	b, err := json.Marshal(s)
	if err != nil {
		return "", err
	}
	return moaktTokPrefix + base64.StdEncoding.EncodeToString(b), nil
}

func moaktDecodeSess(tok string) (*moaktSess, error) {
	if !strings.HasPrefix(tok, moaktTokPrefix) {
		return nil, fmt.Errorf("moakt: invalid session token")
	}
	raw, err := base64.StdEncoding.DecodeString(tok[len(moaktTokPrefix):])
	if err != nil {
		return nil, fmt.Errorf("moakt: invalid session token")
	}
	var s moaktSess
	if err := json.Unmarshal(raw, &s); err != nil {
		return nil, fmt.Errorf("moakt: invalid session token")
	}
	if s.CookieHdr == "" || s.Locale == "" {
		return nil, fmt.Errorf("moakt: invalid session token")
	}
	return &s, nil
}

func moaktHTTPClient() tls_client.HttpClient {
	if HTTPClientNoCookieJar != nil {
		return HTTPClientNoCookieJar()
	}
	return HTTPClient()
}

func moaktSetPageHeaders(req *http.Request, referer string) {
	req.Header.Set("User-Agent", GetCurrentUA())
	req.Header.Set("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6")
	req.Header.Set("Cache-Control", "no-cache")
	req.Header.Set("DNT", "1")
	req.Header.Set("Pragma", "no-cache")
	req.Header.Set("Referer", referer)
	req.Header.Set("Upgrade-Insecure-Requests", "1")
}

func moaktParseInboxEmail(htmlStr string) (string, error) {
	m := moaktEmailDivRe.FindStringSubmatch(htmlStr)
	if len(m) < 2 {
		return "", fmt.Errorf("moakt: email-address not found")
	}
	addr := strings.TrimSpace(html.UnescapeString(m[1]))
	if addr == "" {
		return "", fmt.Errorf("moakt: empty email-address")
	}
	return addr, nil
}

func moaktStripTags(s string) string {
	re := regexp.MustCompile(`<[^>]+>`)
	return strings.TrimSpace(re.ReplaceAllString(s, " "))
}

func moaktListMailIDs(htmlStr string) []string {
	seen := make(map[string]struct{})
	var out []string
	for _, m := range moaktHrefEmailRe.FindAllStringSubmatch(htmlStr, -1) {
		if len(m) < 2 {
			continue
		}
		path := m[1]
		if strings.Contains(path, "/delete") {
			continue
		}
		id := path[strings.LastIndex(path, "/")+1:]
		if _, ok := seen[id]; ok {
			continue
		}
		seen[id] = struct{}{}
		out = append(out, id)
	}
	return out
}

func moaktParseMessageHTML(page string, id string, recipient string) map[string]interface{} {
	raw := map[string]interface{}{"id": id, "to": recipient}
	if sm := moaktTitleRe.FindStringSubmatch(page); len(sm) >= 2 {
		raw["subject"] = strings.TrimSpace(html.UnescapeString(sm[1]))
	}
	if sm := moaktDateRe.FindStringSubmatch(page); len(sm) >= 2 {
		raw["date"] = strings.TrimSpace(html.UnescapeString(sm[1]))
	}
	if sm := moaktSenderRe.FindStringSubmatch(page); len(sm) >= 2 {
		inner := html.UnescapeString(sm[1])
		raw["from"] = strings.TrimSpace(moaktStripTags(inner))
		if em := moaktFromAddrRe.FindStringSubmatch(inner); len(em) >= 2 {
			raw["from"] = strings.TrimSpace(em[1])
		}
	}
	if sm := moaktBodyRe.FindStringSubmatch(page); len(sm) >= 2 {
		body := strings.TrimSpace(sm[1])
		raw["html"] = body
	}
	return raw
}

// MoaktGenerate GET /{locale} 再 GET /{locale}/inbox，解析 #email-address；token 内为 Cookie 快照。
// opts.Domain 为语言路径（如 zh、en），默认 zh。
func MoaktGenerate(domain *string) (*CreatedMailbox, error) {
	loc := moaktLocale(domain)
	base := moaktOrigin + "/" + url.PathEscape(loc)
	inbox := base + "/inbox"
	client := moaktHTTPClient()

	req, err := http.NewRequest("GET", base, nil)
	if err != nil {
		return nil, err
	}
	moaktSetPageHeaders(req, base)
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if err := CheckHTTPStatus(resp, "moakt home"); err != nil {
		return nil, err
	}
	_, _ = io.Copy(io.Discard, resp.Body)
	cookieHdr := moaktMergeCookies("", resp.Cookies())

	req2, err := http.NewRequest("GET", inbox, nil)
	if err != nil {
		return nil, err
	}
	moaktSetPageHeaders(req2, base)
	req2.Header.Set("Cookie", cookieHdr)
	resp2, err := client.Do(req2)
	if err != nil {
		return nil, err
	}
	defer resp2.Body.Close()
	if err := CheckHTTPStatus(resp2, "moakt inbox"); err != nil {
		return nil, err
	}
	body, err := io.ReadAll(resp2.Body)
	if err != nil {
		return nil, err
	}
	cookieHdr = moaktMergeCookies(cookieHdr, resp2.Cookies())
	htmlStr := string(body)
	emailAddr, err := moaktParseInboxEmail(htmlStr)
	if err != nil {
		return nil, err
	}
	if moaktCookieMap(cookieHdr)["tm_session"] == "" {
		return nil, fmt.Errorf("moakt: missing tm_session cookie")
	}
	tok, err := moaktEncodeSess(&moaktSess{Locale: loc, CookieHdr: cookieHdr})
	if err != nil {
		return nil, err
	}
	return &CreatedMailbox{
		Channel: "moakt",
		Email:   emailAddr,
		Token:   tok,
	}, nil
}

// MoaktGetEmails 拉取收件箱链接后逐封 GET .../email/{id}/html 解析正文。
func MoaktGetEmails(email, token string) ([]NormEmail, error) {
	sess, err := moaktDecodeSess(token)
	if err != nil {
		return nil, err
	}
	loc := sess.Locale
	inbox := moaktOrigin + "/" + url.PathEscape(loc) + "/inbox"
	client := moaktHTTPClient()

	req, err := http.NewRequest("GET", inbox, nil)
	if err != nil {
		return nil, err
	}
	moaktSetPageHeaders(req, moaktOrigin+"/"+url.PathEscape(loc))
	req.Header.Set("Cookie", sess.CookieHdr)
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if err := CheckHTTPStatus(resp, "moakt inbox"); err != nil {
		return nil, err
	}
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	ids := moaktListMailIDs(string(body))
	out := make([]NormEmail, 0, len(ids))
	for _, id := range ids {
		detailURL := moaktOrigin + "/" + url.PathEscape(loc) + "/email/" + url.PathEscape(id) + "/html"
		reqd, err := http.NewRequest("GET", detailURL, nil)
		if err != nil {
			continue
		}
		moaktSetPageHeaders(reqd, moaktOrigin+"/"+url.PathEscape(loc)+"/email/"+url.PathEscape(id))
		reqd.Header.Set("Cookie", sess.CookieHdr)
		respd, err := client.Do(reqd)
		if err != nil {
			continue
		}
		if err := CheckHTTPStatus(respd, "moakt mail html"); err != nil {
			respd.Body.Close()
			continue
		}
		b, err := io.ReadAll(respd.Body)
		respd.Body.Close()
		if err != nil {
			continue
		}
		raw := moaktParseMessageHTML(string(b), id, email)
		out = append(out, NormalizeMap(raw, email))
	}
	return out, nil
}
