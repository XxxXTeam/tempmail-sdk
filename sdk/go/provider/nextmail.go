package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"net/url"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

const nextmailBase = "https://api.nextmail.sh"
const nextmailTokenSep = "|"

func nextmailRequest(method, path, bearer string, body io.Reader) (map[string]any, error) {
	req, err := http.NewRequest(method, nextmailBase+path, body)
	if err != nil {
		return nil, err
	}
	req.Header.Set("Accept", "application/json")
	if body != nil {
		req.Header.Set("Content-Type", "application/json")
	}
	if bearer != "" {
		req.Header.Set("Authorization", "Bearer "+bearer)
	}
	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("nextmail: HTTP %d", resp.StatusCode)
	}
	var out map[string]any
	if err := json.Unmarshal(raw, &out); err != nil {
		return nil, err
	}
	return out, nil
}

func nextmailSplitToken(token string) (id, bearer string, err error) {
	parts := strings.SplitN(strings.TrimSpace(token), nextmailTokenSep, 2)
	if len(parts) != 2 || parts[0] == "" || parts[1] == "" {
		return "", "", fmt.Errorf("nextmail: malformed internal token")
	}
	return parts[0], parts[1], nil
}

/* NextmailGenerate 租用临时收件箱；内部令牌打包收件箱 ID 与读取令牌 */
func NextmailGenerate() (*CreatedMailbox, error) {
	raw, err := nextmailRequest("POST", "/temp-mail/inboxes", "", strings.NewReader("{}"))
	if err != nil {
		return nil, err
	}
	address := strings.TrimSpace(fmt.Sprint(raw["address"]))
	id := strings.TrimSpace(fmt.Sprint(raw["id"]))
	token := strings.TrimSpace(fmt.Sprint(raw["token"]))
	if address == "" || id == "" || token == "" {
		return nil, fmt.Errorf("nextmail: incomplete creation response")
	}
	return &CreatedMailbox{
		Channel:   "nextmail",
		Email:     address,
		Token:     id + nextmailTokenSep + token,
		ExpiresAt: raw["expiresAt"],
		CreatedAt: fmt.Sprint(raw["createdAt"]),
	}, nil
}

/* NextmailGetEmails 详情正文位于 message.decodedBody（HTML），纯文本由归一化层生成 */
func NextmailGetEmails(email, token string) ([]NormEmail, error) {
	id, bearer, err := nextmailSplitToken(token)
	if err != nil {
		return nil, err
	}
	basePath := "/temp-mail/inboxes/" + url.PathEscape(id) + "/messages"
	list, err := nextmailRequest("GET", basePath+"?limit=50", bearer, nil)
	if err != nil {
		return nil, err
	}
	items, _ := list["data"].([]any)
	out := make([]NormEmail, 0, len(items))
	for _, item := range items {
		summary, _ := item.(map[string]any)
		messageID := strings.TrimSpace(fmt.Sprint(summary["id"]))
		if messageID == "" {
			continue
		}
		detail, detailErr := nextmailRequest("GET", basePath+"/"+url.PathEscape(messageID), bearer, nil)
		if detailErr == nil {
			flat := map[string]any{
				"id":      detail["id"],
				"to":      detail["to"],
				"subject": detail["subject"],
				"date":    detail["receivedAt"],
				"seen":    detail["seen"],
			}
			if from, ok := detail["from"].(map[string]any); ok {
				flat["from"] = from["email"]
			}
			if message, ok := detail["message"].(map[string]any); ok {
				flat["html"] = message["decodedBody"]
			}
			out = append(out, NormalizeMap(flat, email))
			continue
		}
		flat := map[string]any{
			"id":      messageID,
			"to":      summary["to"],
			"subject": summary["subject"],
			"text":    summary["snippet"],
			"date":    summary["receivedAt"],
			"seen":    summary["seen"],
		}
		if from, ok := summary["from"].(map[string]any); ok {
			flat["from"] = from["email"]
		}
		out = append(out, NormalizeMap(flat, email))
	}
	return out, nil
}
