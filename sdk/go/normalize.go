package tempemail

import (
	"encoding/json"
	"fmt"
	"math"
	"time"
)

// normalizeRawEmails 将原始 JSON 消息列表标准化为统一 Email 格式
func normalizeRawEmails(rawMessages []json.RawMessage, recipientEmail string) ([]Email, error) {
	emails := make([]Email, 0, len(rawMessages))
	for _, raw := range rawMessages {
		var m map[string]interface{}
		if err := json.Unmarshal(raw, &m); err != nil {
			return nil, err
		}
		emails = append(emails, normalizeRawEmail(m, recipientEmail))
	}
	return emails, nil
}

// normalizeRawEmail 将原始 map 数据标准化为统一 Email 格式
func normalizeRawEmail(raw map[string]interface{}, recipientEmail string) Email {
	return Email{
		ID:          normalizeID(raw),
		From:        normalizeFrom(raw),
		To:          normalizeTo(raw, recipientEmail),
		Subject:     normalizeSubject(raw),
		Text:        normalizeText(raw),
		HTML:        normalizeHTML(raw),
		Date:        normalizeDate(raw),
		IsRead:      normalizeIsRead(raw),
		Attachments: normalizeAttachments(raw),
	}
}

func getStr(m map[string]interface{}, keys ...string) string {
	for _, key := range keys {
		if v, ok := m[key]; ok && v != nil {
			switch val := v.(type) {
			case string:
				return val
			default:
				return fmt.Sprintf("%v", val)
			}
		}
	}
	return ""
}

func normalizeID(raw map[string]interface{}) string {
	return getStr(raw, "id", "eid", "_id", "mailboxId", "messageId", "mail_id")
}

func normalizeFrom(raw map[string]interface{}) string {
	return getStr(raw, "from_address", "address_from", "from", "messageFrom", "sender")
}

func normalizeTo(raw map[string]interface{}, recipientEmail string) string {
	result := getStr(raw, "to", "to_address", "name_to", "email_address", "address")
	if result == "" {
		return recipientEmail
	}
	return result
}

func normalizeSubject(raw map[string]interface{}) string {
	return getStr(raw, "subject", "e_subject")
}

func normalizeText(raw map[string]interface{}) string {
	return getStr(raw, "text", "body", "content", "body_text", "text_content")
}

func normalizeHTML(raw map[string]interface{}) string {
	return getStr(raw, "html", "html_content", "body_html")
}

func normalizeDate(raw map[string]interface{}) string {
	// 尝试字符串日期字段
	for _, key := range []string{"received_at", "created_at", "createdAt", "date"} {
		if v, ok := raw[key]; ok && v != nil {
			switch val := v.(type) {
			case string:
				if t, err := time.Parse(time.RFC3339, val); err == nil {
					return t.UTC().Format(time.RFC3339)
				}
				if t, err := time.Parse(time.RFC3339Nano, val); err == nil {
					return t.UTC().Format(time.RFC3339)
				}
				// 尝试直接返回字符串
				return val
			case float64:
				// 大数字可能是毫秒时间戳
				if val > 1e12 {
					return time.UnixMilli(int64(val)).UTC().Format(time.RFC3339)
				}
				return time.Unix(int64(val), 0).UTC().Format(time.RFC3339)
			}
		}
	}

	// 尝试数字时间戳字段
	for _, key := range []string{"timestamp", "e_date"} {
		if v, ok := raw[key]; ok && v != nil {
			if num, ok := v.(float64); ok && num > 0 {
				if key == "timestamp" && num < 1e12 {
					return time.Unix(int64(num), 0).UTC().Format(time.RFC3339)
				}
				return time.UnixMilli(int64(num)).UTC().Format(time.RFC3339)
			}
		}
	}

	return ""
}

func normalizeIsRead(raw map[string]interface{}) bool {
	// 布尔值 seen (mail.tm)
	if v, ok := raw["seen"]; ok {
		if b, ok := v.(bool); ok {
			return b
		}
	}
	// 布尔值 read
	if v, ok := raw["read"]; ok {
		if b, ok := v.(bool); ok {
			return b
		}
	}
	// 布尔值 isRead
	if v, ok := raw["isRead"]; ok {
		if b, ok := v.(bool); ok {
			return b
		}
	}
	// 数字或字符串 is_read
	if v, ok := raw["is_read"]; ok {
		switch val := v.(type) {
		case float64:
			return int(math.Round(val)) != 0
		case bool:
			return val
		case string:
			return val == "1"
		}
	}
	return false
}

func normalizeAttachments(raw map[string]interface{}) []EmailAttachment {
	v, ok := raw["attachments"]
	if !ok || v == nil {
		return []EmailAttachment{}
	}

	arr, ok := v.([]interface{})
	if !ok {
		return []EmailAttachment{}
	}

	attachments := make([]EmailAttachment, 0, len(arr))
	for _, item := range arr {
		m, ok := item.(map[string]interface{})
		if !ok {
			continue
		}
		att := EmailAttachment{
			Filename: getStr(m, "filename", "name"),
		}
		if size, ok := m["size"].(float64); ok {
			att.Size = int64(size)
		} else if size, ok := m["filesize"].(float64); ok {
			att.Size = int64(size)
		}
		att.ContentType = getStr(m, "contentType", "content_type", "mimeType", "mime_type")
		att.URL = getStr(m, "url", "download_url", "downloadUrl")
		attachments = append(attachments, att)
	}
	return attachments
}
