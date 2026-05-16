package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"math/rand"
	"net/http"
	"sort"
	"strings"
	"time"

	fhttp "github.com/bogdanfinn/fhttp"
)

const minmailAPIBase = "https://minmail.app/api"

type minmailAddressResp struct {
	Address   string `json:"address"`
	Expire    int    `json:"expire"`
	VisitorID string `json:"visitorId"`
	Ck        string `json:"ck"`
}

type minmailStoredToken struct {
	VisitorID string `json:"visitorId"`
	Ck        string `json:"ck"`
	Cookie    string `json:"cookie"`
}

type minmailListResp struct {
	Message []minmailMessage `json:"message"`
}

type minmailMessage struct {
	ID          string `json:"id"`
	From        string `json:"from"`
	FromAddress string `json:"fromAddress"`
	FromName    string `json:"fromName"`
	To          string `json:"to"`
	Subject     string `json:"subject"`
	Preview     string `json:"preview"`
	Content     string `json:"content"`
	Date        string `json:"date"`
	IsRead      bool   `json:"isRead"`
}

func minmailRandomSeg(n int) string {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	b := make([]byte, n)
	for i := range b {
		b[i] = chars[rand.Intn(len(chars))]
	}
	return string(b)
}

func minmailVisitorID() string {
	return fmt.Sprintf("%s-%s-%s-%s-%s",
		minmailRandomSeg(8), minmailRandomSeg(4), minmailRandomSeg(4), minmailRandomSeg(4), minmailRandomSeg(12))
}

func minmailCookieHeader() string {
	now := time.Now().UnixMilli()
	rnd := rand.Intn(1000000)
	ga := fmt.Sprintf("GA1.1.%d.%d", now, rnd)
	return fmt.Sprintf("_ga=%s; _ga_DFGB8WF1WG=GS2.1.s%d$o1$g0$t%d$j60$l0$h0", ga, now, now)
}

func minmailMergeCookies(prev string, cookies []*http.Cookie) string {
	m := map[string]string{}
	for _, part := range strings.Split(prev, ";") {
		part = strings.TrimSpace(part)
		if part == "" {
			continue
		}
		if i := strings.IndexByte(part, '='); i > 0 {
			m[part[:i]] = part[i+1:]
		}
	}
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

func minmailCookieGet(cookieLine, name string) string {
	for _, part := range strings.Split(cookieLine, ";") {
		part = strings.TrimSpace(part)
		if part == "" {
			continue
		}
		if i := strings.IndexByte(part, '='); i > 0 && part[:i] == name {
			return part[i+1:]
		}
	}
	return ""
}

func minmailBaseHeaders(req *fhttp.Request, cookie string) {
	req.Header.Set("Accept", "*/*")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6")
	req.Header.Set("Origin", "https://minmail.app")
	req.Header.Set("Referer", "https://minmail.app/")
	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/148.0.0.0 Safari/537.36 Edg/148.0.0.0")
	req.Header.Set("cache-control", "no-cache")
	req.Header.Set("dnt", "1")
	req.Header.Set("pragma", "no-cache")
	req.Header.Set("priority", "u=1, i")
	req.Header.Set("sec-ch-ua", `"Chromium";v="148", "Microsoft Edge";v="148", "Not/A)Brand";v="99"`)
	req.Header.Set("sec-ch-ua-mobile", "?0")
	req.Header.Set("sec-ch-ua-platform", `"Windows"`)
	req.Header.Set("sec-fetch-dest", "empty")
	req.Header.Set("sec-fetch-mode", "cors")
	req.Header.Set("sec-fetch-site", "same-origin")
	if strings.TrimSpace(cookie) == "" {
		cookie = minmailCookieHeader()
	}
	req.Header.Set("Cookie", cookie)
}

func minmailListHeaders(req *fhttp.Request, visitorID, ck, cookie string) {
	minmailBaseHeaders(req, cookie)
	req.Header.Set("visitor-id", visitorID)
	if ck != "" {
		req.Header.Set("ck", ck)
	}
}

func minmailEncodeToken(visitorID, ck, cookie string) (string, error) {
	b, err := json.Marshal(minmailStoredToken{VisitorID: visitorID, Ck: ck, Cookie: cookie})
	if err != nil {
		return "", err
	}
	return string(b), nil
}

func minmailParseToken(token string) (visitorID, ck, cookie string) {
	t := strings.TrimSpace(token)
	if strings.HasPrefix(t, "{") {
		var st minmailStoredToken
		if json.Unmarshal([]byte(t), &st) == nil {
			vid := st.VisitorID
			if vid == "" {
				vid = minmailCookieGet(st.Cookie, "visitorId")
			}
			ckVal := st.Ck
			if ckVal == "" {
				ckVal = minmailCookieGet(st.Cookie, "ck")
			}
			return vid, ckVal, st.Cookie
		}
	}
	return t, "", ""
}

func minmailToMatches(header, want string) bool {
	want = strings.TrimSpace(strings.ToLower(want))
	if want == "" {
		return true
	}
	h := strings.TrimSpace(strings.ToLower(header))
	if h == "" {
		return true
	}
	if h == want {
		return true
	}
	if i := strings.Index(h, "<"); i >= 0 {
		if j := strings.Index(h, ">"); j > i {
			inner := strings.TrimSpace(h[i+1 : j])
			if inner == want {
				return true
			}
		}
	}
	return strings.Contains(h, want)
}

func MinmailGenerate() (*CreatedMailbox, error) {
	vid := minmailVisitorID()
	cook := minmailCookieHeader()
	req, err := fhttp.NewRequest("GET", minmailAPIBase+"/mail/address?refresh=true&expire=1440&part=main", nil)
	if err != nil {
		return nil, err
	}
	minmailBaseHeaders(req, cook)
	req.Header.Set("visitor-id", vid)

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("minmail address: %d", resp.StatusCode)
	}

	cook = minmailMergeCookies(cook, resp.Cookies())

	var data minmailAddressResp
	if err := json.Unmarshal(body, &data); err != nil {
		return nil, err
	}
	if data.Address == "" {
		return nil, fmt.Errorf("minmail: empty address")
	}

	if data.VisitorID != "" {
		vid = data.VisitorID
	} else if v := minmailCookieGet(cook, "visitorId"); v != "" {
		vid = v
	}
	ck := data.Ck
	if ck == "" {
		ck = resp.Header.Get("ck")
	}
	if ck == "" {
		ck = minmailCookieGet(cook, "ck")
	}
	if vid != "" && minmailCookieGet(cook, "visitorId") == "" {
		cook = minmailMergeCookies(cook+"; visitorId="+vid, nil)
	}
	tok, err := minmailEncodeToken(vid, ck, cook)
	if err != nil {
		return nil, err
	}

	expiresAt := time.Now().Add(time.Duration(data.Expire) * time.Minute).UnixMilli()
	info := &CreatedMailbox{Channel: "minmail", Email: data.Address, Token: tok}
	info.ExpiresAt = expiresAt
	return info, nil
}

func MinmailGetEmails(email, token string) ([]NormEmail, error) {
	vid, ck, cook := minmailParseToken(token)
	if vid == "" {
		return nil, fmt.Errorf("minmail: token must include visitorId")
	}
	req, err := fhttp.NewRequest("GET", minmailAPIBase+"/mail/list?part=main", nil)
	if err != nil {
		return nil, err
	}
	minmailListHeaders(req, vid, ck, cook)

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("minmail list: %d", resp.StatusCode)
	}

	var data minmailListResp
	if err := json.Unmarshal(body, &data); err != nil {
		return nil, err
	}

	var emails []NormEmail
	for _, raw := range data.Message {
		if raw.To != "" && !minmailToMatches(raw.To, email) {
			continue
		}
		flat := map[string]interface{}{
			"id":          raw.ID,
			"from":        raw.From,
			"fromAddress": raw.FromAddress,
			"fromName":    raw.FromName,
			"to":          raw.To,
			"subject": raw.Subject,
			"text":    raw.Preview,
			"html":    raw.Content,
			"date":    raw.Date,
			"isRead":  raw.IsRead,
		}
		emails = append(emails, NormalizeMap(flat, email))
	}
	return emails, nil
}
