package provider

import (
	"encoding/json"
	"fmt"
	"io"
	stdhttp "net/http"
	"net/url"
	"regexp"
	"strings"
	"time"
)

const fakemailBase = "https://www.fakemail.net"

var fakemailCSRFRe = regexp.MustCompile(`const\s+CSRF\s*=\s*"([^"]+)"`)

type fakemailGenerateResponse struct {
	Email string `json:"email"`
	Heslo string `json:"heslo"`
}

type fakemailListRow struct {
	ID       any    `json:"id"`
	Subject  string `json:"predmet"`
	From     string `json:"od"`
	When     string `json:"kdy"`
	ReadFlag string `json:"precteno"`
}

type fakemailDetailResponse struct {
	ID      any    `json:"id"`
	Subject string `json:"predmet"`
	From    string `json:"od"`
	HTML    string `json:"telo"`
	To      string `json:"komu"`
}

func fakemailClient() *stdhttp.Client {
	return &stdhttp.Client{Timeout: 15 * time.Second}
}

func fakemailMergeCookie(prev string, headers stdhttp.Header) string {
	jar := map[string]string{}
	for _, part := range strings.Split(prev, ";") {
		p := strings.TrimSpace(part)
		if i := strings.Index(p, "="); i > 0 {
			jar[p[:i]] = p[i+1:]
		}
	}
	for _, line := range headers.Values("Set-Cookie") {
		first := strings.TrimSpace(strings.Split(line, ";")[0])
		if i := strings.Index(first, "="); i > 0 {
			jar[first[:i]] = first[i+1:]
		}
	}
	parts := make([]string, 0, len(jar))
	for k, v := range jar {
		parts = append(parts, k+"="+v)
	}
	return strings.Join(parts, "; ")
}

func fakemailRequest(method, u, cookie string, body io.Reader) (int, []byte, string, error) {
	req, err := stdhttp.NewRequest(method, u, body)
	if err != nil {
		return 0, nil, "", err
	}
	req.Header.Set("User-Agent", "Mozilla/5.0")
	req.Header.Set("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8")
	if strings.Contains(u, "/index/") {
		req.Header.Set("Accept", "application/json, text/javascript, */*; q=0.01")
		req.Header.Set("X-Requested-With", "XMLHttpRequest")
		req.Header.Set("Referer", fakemailBase+"/")
	}
	if method == stdhttp.MethodPost {
		req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	}
	if cookie != "" {
		req.Header.Set("Cookie", cookie)
	}
	resp, err := fakemailClient().Do(req)
	if err != nil {
		return 0, nil, "", err
	}
	defer resp.Body.Close()
	data, err := io.ReadAll(resp.Body)
	if err != nil {
		return resp.StatusCode, nil, "", err
	}
	return resp.StatusCode, data, fakemailMergeCookie(cookie, resp.Header), nil
}

func fakemailCleanJSON(data []byte) []byte {
	if len(data) >= 3 && data[0] == 0xef && data[1] == 0xbb && data[2] == 0xbf {
		return data[3:]
	}
	return data
}

func FakemailGenerate() (*CreatedMailbox, error) {
	status, body, cookie, err := fakemailRequest(stdhttp.MethodGet, fakemailBase+"/", "", nil)
	if err != nil {
		return nil, err
	}
	if status < 200 || status >= 300 {
		return nil, fmt.Errorf("fakemail home: %d", status)
	}
	m := fakemailCSRFRe.FindStringSubmatch(string(body))
	if len(m) < 2 {
		return nil, fmt.Errorf("fakemail: csrf token not found")
	}
	u := fakemailBase + "/index/index?csrf_token=" + url.QueryEscape(m[1])
	status, body, cookie, err = fakemailRequest(stdhttp.MethodGet, u, cookie, nil)
	if err != nil {
		return nil, err
	}
	if status < 200 || status >= 300 {
		return nil, fmt.Errorf("fakemail generate: %d", status)
	}
	var data fakemailGenerateResponse
	if err := json.Unmarshal(fakemailCleanJSON(body), &data); err != nil {
		return nil, err
	}
	email := strings.TrimSpace(data.Email)
	if email == "" || !strings.Contains(email, "@") {
		return nil, fmt.Errorf("fakemail: invalid mailbox response")
	}
	return &CreatedMailbox{Channel: "fakemail", Email: email, Token: cookie}, nil
}

func fakemailFetchDetail(cookie, id string) (*fakemailDetailResponse, error) {
	form := url.Values{"id": {id}}
	status, body, _, err := fakemailRequest(stdhttp.MethodPost, fakemailBase+"/index/email", cookie, strings.NewReader(form.Encode()))
	if err != nil {
		return nil, err
	}
	if status < 200 || status >= 300 {
		return nil, fmt.Errorf("fakemail detail: %d", status)
	}
	var detail fakemailDetailResponse
	if err := json.Unmarshal(fakemailCleanJSON(body), &detail); err != nil {
		return nil, err
	}
	return &detail, nil
}

func fakemailFlatten(row fakemailListRow, detail *fakemailDetailResponse, recipient string) map[string]any {
	subject := row.Subject
	from := row.From
	html := ""
	id := row.ID
	if detail != nil {
		if detail.Subject != "" {
			subject = detail.Subject
		}
		if detail.From != "" {
			from = detail.From
		}
		if detail.HTML != "" {
			html = detail.HTML
		}
		if detail.ID != nil {
			id = detail.ID
		}
	}
	return map[string]any{
		"id":      id,
		"from":    from,
		"to":      recipient,
		"subject": subject,
		"text":    "",
		"html":    html,
		"date":    row.When,
		"isRead":  row.ReadFlag == "precteno",
	}
}

func FakemailGetEmails(token, email string) ([]NormEmail, error) {
	cookie := strings.TrimSpace(token)
	address := strings.TrimSpace(email)
	if cookie == "" {
		return nil, fmt.Errorf("fakemail: empty session token")
	}
	if address == "" {
		return nil, fmt.Errorf("fakemail: empty email")
	}
	status, body, _, err := fakemailRequest(stdhttp.MethodGet, fakemailBase+"/index/refresh", cookie, nil)
	if err != nil {
		return nil, err
	}
	if status < 200 || status >= 300 {
		return nil, fmt.Errorf("fakemail refresh: %d", status)
	}
	var rows []fakemailListRow
	if err := json.Unmarshal(fakemailCleanJSON(body), &rows); err != nil {
		return nil, err
	}
	out := make([]NormEmail, 0, len(rows))
	for _, row := range rows {
		id := strings.TrimSpace(fmt.Sprintf("%v", row.ID))
		var detail *fakemailDetailResponse
		if id != "" {
			detail, _ = fakemailFetchDetail(cookie, id)
		}
		out = append(out, NormalizeMap(fakemailFlatten(row, detail, address), address))
	}
	return out, nil
}
