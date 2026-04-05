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
)

const tmpmailsOrigin = "https://tmpmails.com"

// locale、user_sign、getInboxList 的 next-action ID，以制表符拼接（内部使用）
const tmpmailsTokenSep = "\t"

var (
	tmpmailsEmailRe       = regexp.MustCompile(`[a-zA-Z0-9._-]+@tmpmails\.com`)
	tmpmailsPageChunkRe   = regexp.MustCompile(`/_next/static/chunks/app/%5Blocale%5D/page-[a-f0-9]+\.js`)
	tmpmailsInboxActionRe = regexp.MustCompile(`"([0-9a-f]+)",[^,]+,void 0,[^,]+,"getInboxList"`)
)

func tmpmailsLocale(domain *string) string {
	if domain == nil {
		return "zh"
	}
	s := strings.TrimSpace(*domain)
	if s == "" {
		return "zh"
	}
	return s
}

func tmpmailsUserSign(resp *http.Response) (string, error) {
	for _, c := range resp.Cookies() {
		if strings.EqualFold(c.Name, "user_sign") {
			v := strings.TrimSpace(c.Value)
			if v != "" {
				return v, nil
			}
		}
	}
	return "", fmt.Errorf("tmpmails: missing user_sign cookie")
}

func tmpmailsPickEmail(html string) (string, error) {
	const support = "support@tmpmails.com"
	matches := tmpmailsEmailRe.FindAllString(html, -1)
	counts := make(map[string]int)
	for _, m := range matches {
		if strings.EqualFold(strings.TrimSpace(m), support) {
			continue
		}
		counts[m]++
	}
	if len(counts) == 0 {
		return "", fmt.Errorf("tmpmails: no inbox address in page")
	}
	type pair struct {
		addr  string
		count int
	}
	ranked := make([]pair, 0, len(counts))
	for a, c := range counts {
		ranked = append(ranked, pair{a, c})
	}
	sort.Slice(ranked, func(i, j int) bool {
		if ranked[i].count != ranked[j].count {
			return ranked[i].count > ranked[j].count
		}
		return ranked[i].addr < ranked[j].addr
	})
	return ranked[0].addr, nil
}

func tmpmailsFetchInboxActionID(html string) (string, error) {
	sub := tmpmailsPageChunkRe.FindString(html)
	if sub == "" {
		return "", fmt.Errorf("tmpmails: page chunk script not found")
	}
	req, err := http.NewRequest("GET", tmpmailsOrigin+sub, nil)
	if err != nil {
		return "", err
	}
	req.Header.Set("User-Agent", GetCurrentUA())
	req.Header.Set("Accept", "*/*")
	req.Header.Set("Referer", tmpmailsOrigin+"/")

	client := HTTPClient()
	resp, err := client.Do(req)
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()
	if err := CheckHTTPStatus(resp, "tmpmails page chunk"); err != nil {
		return "", err
	}
	js, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", err
	}
	m := tmpmailsInboxActionRe.FindSubmatch(js)
	if m == nil {
		return "", fmt.Errorf("tmpmails: getInboxList action not found in chunk")
	}
	return string(m[1]), nil
}

func setTmpmailsGETHeaders(req *http.Request, referer string) {
	req.Header.Set("User-Agent", GetCurrentUA())
	req.Header.Set("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6")
	req.Header.Set("Cache-Control", "no-cache")
	req.Header.Set("DNT", "1")
	req.Header.Set("Pragma", "no-cache")
	req.Header.Set("Referer", referer)
	req.Header.Set("Upgrade-Insecure-Requests", "1")
}

func TmpmailsGenerate(domain *string) (*CreatedMailbox, error) {
	locale := tmpmailsLocale(domain)
	pageURL := fmt.Sprintf("%s/%s", tmpmailsOrigin, locale)
	req, err := http.NewRequest("GET", pageURL, nil)
	if err != nil {
		return nil, err
	}
	setTmpmailsGETHeaders(req, pageURL)

	client := HTTPClient()
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if err := CheckHTTPStatus(resp, "tmpmails generate"); err != nil {
		return nil, err
	}
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	html := string(body)
	userSign, err := tmpmailsUserSign(resp)
	if err != nil {
		return nil, err
	}
	email, err := tmpmailsPickEmail(html)
	if err != nil {
		return nil, err
	}
	actionID, err := tmpmailsFetchInboxActionID(html)
	if err != nil {
		return nil, err
	}
	token := locale + tmpmailsTokenSep + userSign + tmpmailsTokenSep + actionID
	return &CreatedMailbox{
		Channel: "tmpmails",
		Email:   email,
		Token:   token,
	}, nil
}

func tmpmailsParseInboxResponse(recipient string, raw []byte) ([]NormEmail, error) {
	text := strings.ReplaceAll(string(raw), `"$undefined"`, "null")
	var dataErr error
	for _, line := range strings.Split(text, "\n") {
		line = strings.TrimSpace(line)
		if line == "" {
			continue
		}
		colon := strings.Index(line, ":")
		if colon <= 0 || colon >= len(line)-1 {
			continue
		}
		jsonPart := strings.TrimSpace(line[colon+1:])
		if !strings.HasPrefix(jsonPart, "{") {
			continue
		}
		var wrap struct {
			Code int             `json:"code"`
			Data json.RawMessage `json:"data"`
		}
		if err := json.Unmarshal([]byte(jsonPart), &wrap); err != nil {
			continue
		}
		if wrap.Code != 200 || len(wrap.Data) == 0 {
			continue
		}
		var data struct {
			List []json.RawMessage `json:"list"`
		}
		if err := json.Unmarshal(wrap.Data, &data); err != nil {
			dataErr = err
			continue
		}
		out := make([]NormEmail, 0, len(data.List))
		for _, rm := range data.List {
			var m map[string]interface{}
			if err := json.Unmarshal(rm, &m); err != nil {
				continue
			}
			out = append(out, NormalizeMap(m, recipient))
		}
		return out, nil
	}
	if dataErr != nil {
		return nil, dataErr
	}
	return nil, fmt.Errorf("tmpmails: inbox payload not found")
}

// tmpmailsNextRouterStateTreeJSON 与浏览器请求 Next.js Server Action 时的 next-router-state-tree 一致（locale 随路径变化）。
func tmpmailsNextRouterStateTreeJSON(locale string) (string, error) {
	state := []interface{}{
		"",
		map[string]interface{}{
			"children": []interface{}{
				[]interface{}{"locale", locale, "d"},
				map[string]interface{}{
					"children": []interface{}{
						"__PAGE__",
						map[string]interface{}{},
						"/" + locale,
						"refresh",
					},
				},
			},
		},
		nil,
		nil,
		true,
	}
	b, err := json.Marshal(state)
	if err != nil {
		return "", err
	}
	return string(b), nil
}

func setTmpmailsInboxPOSTHeaders(req *http.Request, postURL, locale, userSign, actionID string) error {
	tree, err := tmpmailsNextRouterStateTreeJSON(locale)
	if err != nil {
		return err
	}
	req.Header.Set("User-Agent", GetCurrentUA())
	req.Header.Set("Accept", "text/x-component")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6")
	req.Header.Set("Cache-Control", "no-cache")
	req.Header.Set("Content-Type", "text/plain;charset=UTF-8")
	req.Header.Set("DNT", "1")
	req.Header.Set("Next-Action", actionID)
	req.Header.Set("Next-Router-State-Tree", tree)
	req.Header.Set("Origin", tmpmailsOrigin)
	req.Header.Set("Pragma", "no-cache")
	req.Header.Set("Referer", postURL)
	req.Header.Set("Sec-Fetch-Dest", "empty")
	req.Header.Set("Sec-Fetch-Mode", "cors")
	req.Header.Set("Sec-Fetch-Site", "same-origin")
	req.Header.Set("Cookie", fmt.Sprintf("NEXT_LOCALE=%s; user_sign=%s", locale, userSign))
	return nil
}

// TmpmailsGetEmails token 格式：locale + "\t" + user_sign + "\t" + next-action（getInboxList）
func TmpmailsGetEmails(email, token string) ([]NormEmail, error) {
	parts := strings.Split(token, tmpmailsTokenSep)
	if len(parts) != 3 {
		return nil, fmt.Errorf("tmpmails: invalid session token")
	}
	locale, userSign, actionID := parts[0], parts[1], parts[2]
	postURL := fmt.Sprintf("%s/%s", tmpmailsOrigin, locale)
	payload := []interface{}{userSign, email, 0}
	body, err := json.Marshal(payload)
	if err != nil {
		return nil, err
	}
	req, err := http.NewRequest("POST", postURL, bytes.NewReader(body))
	if err != nil {
		return nil, err
	}
	if err := setTmpmailsInboxPOSTHeaders(req, postURL, locale, userSign, actionID); err != nil {
		return nil, err
	}

	client := HTTPClient()
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if err := CheckHTTPStatus(resp, "tmpmails inbox"); err != nil {
		return nil, err
	}
	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	return tmpmailsParseInboxResponse(email, raw)
}
