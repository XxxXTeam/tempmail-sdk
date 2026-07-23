package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"strings"
	"time"

	http "github.com/bogdanfinn/fhttp"
)

const mailinatorBase = "https://mailinator.com"

func mailinatorParseMessages(data any) []map[string]any {
	switch v := data.(type) {
	case []any:
		out := make([]map[string]any, 0, len(v))
		for _, item := range v {
			if m, ok := item.(map[string]any); ok {
				out = append(out, m)
			}
		}
		return out
	case map[string]any:
		if msgs, ok := v["msgs"].([]any); ok {
			out := make([]map[string]any, 0, len(msgs))
			for _, item := range msgs {
				if m, ok := item.(map[string]any); ok {
					out = append(out, m)
				}
			}
			return out
		}
		if dataMsgs, ok := v["data"].([]any); ok {
			out := make([]map[string]any, 0, len(dataMsgs))
			for _, item := range dataMsgs {
				if m, ok := item.(map[string]any); ok {
					out = append(out, m)
				}
			}
			return out
		}
	}
	return nil
}

func mailinatorToString(value any) string {
	switch v := value.(type) {
	case string:
		return v
	case float64:
		return fmt.Sprintf("%g", v)
	case json.Number:
		return v.String()
	default:
		return ""
	}
}

func mailinatorIsoTime(value any) string {
	switch v := value.(type) {
	case string:
		return v
	case float64:
		if v <= 0 {
			return ""
		}
		millis := int64(v)
		if millis < 1e12 {
			return time.Unix(millis, 0).UTC().Format(time.RFC3339)
		}
		return time.UnixMilli(millis).UTC().Format(time.RFC3339)
	case json.Number:
		if i, err := v.Int64(); err == nil {
			if i < 1e12 {
				return time.Unix(i, 0).UTC().Format(time.RFC3339)
			}
			return time.UnixMilli(i).UTC().Format(time.RFC3339)
		}
	}
	return ""
}

func mailinatorAttachmentURL(value any) string {
	if s, ok := value.(string); ok && strings.TrimSpace(s) != "" {
		if strings.HasPrefix(s, "http://") || strings.HasPrefix(s, "https://") {
			return s
		}
		return mailinatorBase + s
	}
	return ""
}

func mailinatorRequestJSON(path string) (map[string]any, error) {
	req, err := http.NewRequest(http.MethodGet, path, nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("Accept", "application/json")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6")
	req.Header.Set("Cache-Control", "no-cache")
	req.Header.Set("Pragma", "no-cache")
	req.Header.Set("User-Agent", getCurrentUA())

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
		return nil, fmt.Errorf("mailinator http %d", resp.StatusCode)
	}
	var data any
	if err := json.Unmarshal(body, &data); err != nil {
		return nil, err
	}
	if m, ok := data.(map[string]any); ok {
		return m, nil
	}
	return map[string]any{"data": data}, nil
}

func mailinatorFetchDetail(messageID string, suffix string) (map[string]any, error) {
	u := fmt.Sprintf("%s/api/v2/domains/public/messages/%s/%s", mailinatorBase, messageID, suffix)
	return mailinatorRequestJSON(u)
}

func mailinatorFlattenMessage(summary map[string]any, recipient string, textPayload map[string]any, htmlPayload map[string]any, attachmentsPayload map[string]any) map[string]any {
	attachments := make([]map[string]any, 0)
	if raw, ok := attachmentsPayload["attachments"].([]any); ok {
		for _, item := range raw {
			if att, ok := item.(map[string]any); ok {
				attachments = append(attachments, map[string]any{
					"filename":    mailinatorToString(att["name"]),
					"size":        att["size"],
					"contentType": att["contentType"],
					"url":         mailinatorAttachmentURL(att["downloadUrl"]),
				})
			}
		}
	}
	from := mailinatorToString(summary["from"])
	if from == "" {
		from = mailinatorToString(summary["origfrom"])
	}
	subject := mailinatorToString(summary["subject"])
	date := mailinatorIsoTime(summary["time"])
	if date == "" {
		date = mailinatorIsoTime(summary["date"])
	}
	// 优先读 "text" 键（/text 端点实际返回键），回退兜底 "text/plain"（防御性编程）
	textContent := mailinatorToString(textPayload["text"])
	if textContent == "" {
		textContent = mailinatorToString(textPayload["text/plain"])
	}
	// 优先读 "html" 键，回退兜底 "text/html"
	htmlContent := mailinatorToString(htmlPayload["html"])
	if htmlContent == "" {
		htmlContent = mailinatorToString(htmlPayload["text/html"])
	}
	return map[string]any{
		"id":          summary["id"],
		"from":        from,
		"to":          mailinatorToString(summary["to"]),
		"subject":     subject,
		"text":        textContent,
		"html":        htmlContent,
		"date":        date,
		"isRead":      false,
		"attachments": attachments,
	}
}

func MailinatorGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "mailinator",
		Email:   local + "@mailinator.com",
	}, nil
}

func MailinatorGetEmails(email string) ([]NormEmail, error) {
	inbox := strings.TrimSpace(email)
	if at := strings.Index(inbox, "@"); at >= 0 {
		inbox = inbox[:at]
	}
	if inbox == "" {
		return []NormEmail{}, nil
	}

	data, err := mailinatorRequestJSON(fmt.Sprintf("%s/api/v2/domains/public/inboxes/%s", mailinatorBase, inbox))
	if err != nil {
		return nil, err
	}
	messages := mailinatorParseMessages(data)
	if len(messages) == 0 {
		return []NormEmail{}, nil
	}

	out := make([]NormEmail, 0, len(messages))
	for _, msg := range messages {
		messageID := strings.TrimSpace(mailinatorToString(msg["id"]))
		if messageID == "" {
			messageID = strings.TrimSpace(mailinatorToString(msg["messageId"]))
		}

		textPayload := map[string]any{}
		htmlPayload := map[string]any{}
		attachmentsPayload := map[string]any{}

		if messageID != "" {
			if payload, err := mailinatorFetchDetail(messageID, "text"); err == nil {
				textPayload = payload
			}
			if payload, err := mailinatorFetchDetail(messageID, "texthtml"); err == nil {
				htmlPayload = payload
			}
			if payload, err := mailinatorFetchDetail(messageID, "attachments"); err == nil {
				attachmentsPayload = payload
			}
		}

		flat := mailinatorFlattenMessage(msg, email, textPayload, htmlPayload, attachmentsPayload)
		out = append(out, NormalizeMap(flat, email))
	}
	return out, nil
}
