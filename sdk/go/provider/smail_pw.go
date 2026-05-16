package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"net/url"
	"regexp"
	"strconv"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

const smailPwRootDataURL = "https://smail.pw/_root.data"

var smailPwHeaders = map[string]string{
	"Accept":             "*/*",
	"accept-language":    "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
	"cache-control":      "no-cache",
	"dnt":                "1",
	"origin":             "https://smail.pw",
	"pragma":             "no-cache",
	"referer":            "https://smail.pw/",
	"sec-ch-ua":          `"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"`,
	"sec-ch-ua-mobile":   "?0",
	"sec-ch-ua-platform": `"Windows"`,
	"sec-fetch-dest":     "empty",
	"sec-fetch-mode":     "cors",
	"sec-fetch-site":     "same-origin",
}

var (
	smailPwQuotedInbox = regexp.MustCompile(`(?i)"([a-z0-9][a-z0-9.-]*@smail\.pw)"`)
	smailPwPlainInbox  = regexp.MustCompile(`(?i)\b([a-z0-9][a-z0-9.-]*@smail\.pw)\b`)
	smailPwMailBlock      = regexp.MustCompile(`"id","([^"]+)","to_address","([^"]*)","from_name","([^"]*)","from_address","([^"]*)","subject","([^"]*)","time",(\d+)`)
	smailPwMailBlockNoSub = regexp.MustCompile(`"id","([^"]+)","to_address","([^"]*)","from_name","([^"]*)","from_address","([^"]*)","subject","time",(\d+)`)
	smailPwMailBlock2     = regexp.MustCompile(`"id","([^"]+)","from_name","([^"]*)","from_address","([^"]*)","subject","([^"]*)","time",(\d+)`)
)

func setSmailPwHeaders(req *http.Request) {
	for k, v := range smailPwHeaders {
		req.Header.Set(k, v)
	}
	req.Header.Set("User-Agent", GetCurrentUA())
}

func smailPwExtractSession(resp *http.Response) string {
	for _, c := range resp.Cookies() {
		if c.Name == "__session" {
			return "__session=" + c.Value
		}
	}
	return ""
}

func smailPwExtractInbox(body string) string {
	if m := smailPwQuotedInbox.FindStringSubmatch(body); len(m) > 1 {
		return m[1]
	}
	if m := smailPwPlainInbox.FindStringSubmatch(body); len(m) > 1 {
		return m[1]
	}
	return ""
}

func smailPwRowFromFields(fields map[string]interface{}, recipient string) map[string]interface{} {
	subj, _ := fields["subject"].(string)
	toAddr, _ := fields["to_address"].(string)
	if toAddr == "" {
		toAddr = recipient
	}
	if subj == toAddr || strings.HasSuffix(strings.ToLower(subj), "@smail.pw") {
		subj = ""
	}
	ts, _ := fields["time"].(int64)
	return map[string]interface{}{
		"id": fields["id"], "to_address": toAddr, "from_name": fields["from_name"],
		"from_address": fields["from_address"], "subject": subj, "date": float64(ts),
		"text": "", "html": "", "attachments": []interface{}{},
	}
}

func smailPwResolveDeferred(root []interface{}, obj map[string]interface{}) map[string]interface{} {
	fields := map[string]interface{}{}
	for k, v := range obj {
		if !strings.HasPrefix(k, "_") {
			continue
		}
		keyIdx, err1 := strconv.Atoi(k[1:])
		valIdx := 0
		switch n := v.(type) {
		case float64:
			valIdx = int(n)
		case int:
			valIdx = n
		default:
			continue
		}
		if err1 != nil || keyIdx < 0 || keyIdx >= len(root) || valIdx < 0 || valIdx >= len(root) {
			continue
		}
		key, _ := root[keyIdx].(string)
		val := root[valIdx]
		if key == "time" {
			switch t := val.(type) {
			case float64:
				fields["time"] = int64(t)
			case json.Number:
				n, _ := t.Int64()
				fields["time"] = n
			}
		} else if s, ok := val.(string); ok && key != "" {
			fields[key] = s
		}
	}
	if fields["id"] == nil && fields["time"] == nil {
		return nil
	}
	return fields
}

func smailPwParseFlightRoot(root []interface{}, recipient string) []map[string]interface{} {
	var out []map[string]interface{}
	for i := 0; i+1 < len(root); i++ {
		if root[i] != "emails" {
			continue
		}
		refs, ok := root[i+1].([]interface{})
		if !ok {
			break
		}
		for _, r := range refs {
			idx := 0
			switch n := r.(type) {
			case float64:
				idx = int(n)
			case int:
				idx = n
			default:
				continue
			}
			if idx < 0 || idx >= len(root) {
				continue
			}
			if obj, ok := root[idx].(map[string]interface{}); ok {
				if fields := smailPwResolveDeferred(root, obj); fields != nil {
					out = append(out, smailPwRowFromFields(fields, recipient))
				}
			}
		}
		break
	}
	return out
}

func smailPwParseMails(body, recipient string) []map[string]interface{} {
	var out []map[string]interface{}
	for _, m := range smailPwMailBlock.FindAllStringSubmatch(body, -1) {
		ts := float64(parseInt64Safe(m[6]))
		out = append(out, map[string]interface{}{
			"id": m[1], "to_address": m[2], "from_name": m[3], "from_address": m[4],
			"subject": m[5], "date": ts, "text": "", "html": "", "attachments": []interface{}{},
		})
	}
	for _, m := range smailPwMailBlockNoSub.FindAllStringSubmatch(body, -1) {
		ts := float64(parseInt64Safe(m[5]))
		out = append(out, map[string]interface{}{
			"id": m[1], "to_address": m[2], "from_name": m[3], "from_address": m[4],
			"subject": "", "date": ts, "text": "", "html": "", "attachments": []interface{}{},
		})
	}
	if len(out) > 0 {
		return out
	}
	var root []interface{}
	if json.Unmarshal([]byte(body), &root) == nil {
		out = smailPwParseFlightRoot(root, recipient)
		if len(out) > 0 {
			return out
		}
	}
	for _, m := range smailPwMailBlock2.FindAllStringSubmatch(body, -1) {
		ts := float64(parseInt64Safe(m[5]))
		out = append(out, map[string]interface{}{
			"id": m[1], "to_address": recipient, "from_name": m[2], "from_address": m[3],
			"subject": m[4], "date": ts, "text": "", "html": "", "attachments": []interface{}{},
		})
	}
	return out
}

func parseInt64Safe(s string) int64 {
	var n int64
	_, _ = fmt.Sscanf(s, "%d", &n)
	return n
}

// SmailPwGenerate POST intent=generate，保存 __session，解析收件地址
func SmailPwGenerate() (*CreatedMailbox, error) {
	req, err := http.NewRequest(http.MethodPost, smailPwRootDataURL, strings.NewReader("intent=generate"))
	if err != nil {
		return nil, err
	}
	setSmailPwHeaders(req)
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded;charset=UTF-8")

	client := HTTPClient()
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("smail.pw generate failed: %d", resp.StatusCode)
	}

	cookie := smailPwExtractSession(resp)
	if cookie == "" {
		return nil, fmt.Errorf("failed to extract __session cookie")
	}

	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	email := smailPwExtractInbox(string(raw))
	if email == "" {
		return nil, fmt.Errorf("failed to parse inbox from smail.pw response")
	}

	return &CreatedMailbox{Channel: "smail-pw", Email: email, Token: cookie}, nil
}

// SmailPwGetEmails GET _root.data + Cookie
func SmailPwGetEmails(token, email string) ([]NormEmail, error) {
	req, err := http.NewRequest(http.MethodGet, smailPwRootDataURL, nil)
	if err != nil {
		return nil, err
	}
	setSmailPwHeaders(req)
	req.Header.Set("Cookie", token)

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("smail.pw get emails failed: %d", resp.StatusCode)
	}

	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	mails := smailPwParseMails(string(raw), email)
	if len(mails) == 0 {
		return []NormEmail{}, nil
	}
	out := make([]NormEmail, 0, len(mails))
	for _, m := range mails {
		ne := NormalizeMap(m, email)
		if id, ok := m["id"].(string); ok && id != "" {
			if body := smailPwFetchBody(token, id); body != "" {
				ne.HTML = body
			}
		}
		out = append(out, ne)
	}
	return out, nil
}

func smailPwFetchBody(token, id string) string {
	if id == "" || strings.HasPrefix(id, "__smail_") {
		return ""
	}
	u := "https://smail.pw/api/email/" + url.PathEscape(id)
	req, err := http.NewRequest(http.MethodGet, u, nil)
	if err != nil {
		return ""
	}
	setSmailPwHeaders(req)
	req.Header.Set("Cookie", token)
	req.Header.Set("Accept", "application/json")
	resp, err := HTTPClient().Do(req)
	if err != nil {
		return ""
	}
	defer resp.Body.Close()
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return ""
	}
	raw, _ := io.ReadAll(resp.Body)
	var data struct {
		Body string `json:"body"`
	}
	if json.Unmarshal(raw, &data) != nil {
		return ""
	}
	return data.Body
}
