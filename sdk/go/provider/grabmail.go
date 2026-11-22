package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"math/rand"
	"net/url"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

const grabmailBase = "https://grabmail.io/api/v1"
const grabmailDomain = "grabmail.io"

/* grabmailRandomLocal 生成随机本地部分；邮箱在首封邮件到达时自动存在 */
func grabmailRandomLocal() string {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	b := make([]byte, 19)
	copy(b, "sdk")
	for i := 3; i < len(b); i++ {
		b[i] = chars[rand.Intn(len(chars))]
	}
	return string(b)
}

func grabmailGetJSON(path string) (map[string]any, error) {
	req, err := http.NewRequest("GET", grabmailBase+path, nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("Accept", "application/json")
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
		return nil, fmt.Errorf("grabmail: HTTP %d", resp.StatusCode)
	}
	var out map[string]any
	if err := json.Unmarshal(body, &out); err != nil {
		return nil, err
	}
	return out, nil
}

/* GrabmailGenerate 无需请求服务端：地址在收到首封邮件时存在 */
func GrabmailGenerate(channel ...string) (*CreatedMailbox, error) {
	ch := "grabmail"
	if len(channel) > 0 && channel[0] != "" {
		ch = channel[0]
	}
	return &CreatedMailbox{
		Channel: ch,
		Email:   fmt.Sprintf("%s@%s", grabmailRandomLocal(), grabmailDomain),
	}, nil
}

func grabmailFlatten(raw map[string]any, recipient string) map[string]any {
	from := raw["from"]
	if name, ok := raw["from_name"].(string); ok && name != "" {
		from = fmt.Sprintf("%s <%s>", name, raw["from"])
	}
	return map[string]any{
		"id":          raw["id"],
		"from":        from,
		"to":          recipient,
		"subject":     raw["subject"],
		"text":        raw["text"],
		"html":        raw["html"],
		"date":        raw["date"],
		"seen":        raw["seen"],
		"attachments": raw["attachments"],
	}
}

/* GrabmailGetEmails 列表与详情均以地址作为读取凭据 */
func GrabmailGetEmails(email string) ([]NormEmail, error) {
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("grabmail: empty email")
	}
	list, err := grabmailGetJSON("/mailbox?address=" + url.QueryEscape(email))
	if err != nil {
		return nil, err
	}
	items, _ := list["messages"].([]any)
	out := make([]NormEmail, 0, len(items))
	for _, item := range items {
		summary, _ := item.(map[string]any)
		id := strings.TrimSpace(fmt.Sprint(summary["id"]))
		if id == "" {
			continue
		}
		detail, detailErr := grabmailGetJSON("/message/" + url.PathEscape(id) + "?mailbox=" + url.QueryEscape(email))
		if detailErr == nil {
			out = append(out, NormalizeMap(grabmailFlatten(detail, email), email))
			continue
		}
		out = append(out, NormalizeMap(map[string]any{
			"id":      id,
			"from":    summary["from"],
			"to":      email,
			"subject": summary["subject"],
			"text":    summary["preview"],
			"date":    summary["date"],
			"seen":    summary["seen"],
		}, email))
	}
	return out, nil
}
