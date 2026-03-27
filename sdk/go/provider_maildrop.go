package tempemail

import (
	"bytes"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"io"
	"math/rand"
	"mime"
	"mime/multipart"
	"mime/quotedprintable"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

/**
 * Maildrop 渠道实现
 * API: GraphQL endpoint https://api.maildrop.cc/graphql
 *
 * 特点:
 * - 无需认证，公开 GraphQL API
 * - 自带反垃圾过滤
 * - 任意用户名@maildrop.cc 即可接收邮件
 */

const maildropGraphQLURL = "https://api.maildrop.cc/graphql"
const maildropDomain = "maildrop.cc"

/*
 * maildropDecodeRfc2047 解码 RFC 2047 编码的邮件头
 * 如 =?utf-8?B?b3BlbmVs?= 解码为可读文本
 */
func maildropDecodeRfc2047(s string) string {
	dec := new(mime.WordDecoder)
	result, err := dec.DecodeHeader(s)
	if err != nil {
		return s
	}
	return result
}

func maildropSplitHeaderBody(raw string) (headerPart, bodyPart string, ok bool) {
	if raw == "" {
		return "", "", false
	}
	if i := strings.Index(raw, "\r\n\r\n"); i >= 0 {
		return raw[:i], raw[i+4:], true
	}
	if i := strings.Index(raw, "\n\n"); i >= 0 {
		return raw[:i], raw[i+2:], true
	}
	return "", raw, false
}

func maildropDecodeCTE(content, cte string) string {
	l := strings.ToLower(cte)
	if strings.Contains(l, "base64") {
		clean := strings.ReplaceAll(strings.ReplaceAll(content, "\r\n", ""), "\n", "")
		decoded, err := base64.StdEncoding.DecodeString(clean)
		if err == nil {
			return string(decoded)
		}
		return content
	}
	if strings.Contains(l, "quoted-printable") {
		b, err := io.ReadAll(quotedprintable.NewReader(strings.NewReader(content)))
		if err == nil {
			return string(b)
		}
	}
	return content
}

func maildropFindBoundary(headerPart string) string {
	var boundary string
	for _, line := range strings.Split(headerPart, "\r\n") {
		lower := strings.ToLower(line)
		if strings.Contains(lower, "boundary") && strings.Contains(lower, "content-type") {
			_, params, err := mime.ParseMediaType(strings.TrimPrefix(strings.TrimPrefix(lower, "content-type:"), " "))
			if err == nil {
				boundary = params["boundary"]
			}
		}
	}
	if boundary == "" {
		ctIdx := strings.Index(strings.ToLower(headerPart), "content-type:")
		if ctIdx >= 0 {
			ctLine := headerPart[ctIdx:]
			endIdx := strings.Index(ctLine, "\r\n\r\n")
			if endIdx == -1 {
				endIdx = len(ctLine)
			}
			ctFull := strings.ReplaceAll(ctLine[:endIdx], "\r\n\t", " ")
			ctFull = strings.ReplaceAll(ctFull, "\r\n ", " ")
			colonIdx := strings.Index(ctFull, ":")
			if colonIdx >= 0 {
				_, params, err := mime.ParseMediaType(strings.TrimSpace(ctFull[colonIdx+1:]))
				if err == nil {
					boundary = params["boundary"]
				}
			}
		}
	}
	return boundary
}

func maildropHeaderValueCI(headers, field string) string {
	prefixLower := strings.ToLower(field) + ":"
	for _, line := range strings.Split(strings.ReplaceAll(headers, "\r\n", "\n"), "\n") {
		t := strings.TrimSpace(line)
		if strings.HasPrefix(strings.ToLower(t), prefixLower) {
			if colon := strings.Index(t, ":"); colon >= 0 {
				return strings.TrimSpace(t[colon+1:])
			}
		}
	}
	return ""
}

/*
 * maildropExtractText 从原始 MIME 邮件源码中提取纯文本正文
 * maildrop 的 data 字段返回完整 MIME 源码，需要解析出 text/plain 部分
 */
func maildropExtractText(raw string) string {
	headerPart, bodyPart, ok := maildropSplitHeaderBody(raw)
	if !ok {
		return strings.TrimSpace(raw)
	}

	boundary := maildropFindBoundary(headerPart)
	if boundary != "" {
		reader := multipart.NewReader(strings.NewReader(bodyPart), boundary)
		for {
			part, err := reader.NextPart()
			if err != nil {
				break
			}
			ct := part.Header.Get("Content-Type")
			if strings.Contains(strings.ToLower(ct), "text/plain") {
				content, _ := io.ReadAll(part)
				cte := part.Header.Get("Content-Transfer-Encoding")
				out := maildropDecodeCTE(string(content), cte)
				part.Close()
				return strings.TrimSpace(out)
			}
			part.Close()
		}
	}

	hl := strings.ToLower(headerPart)
	if strings.Contains(hl, "content-transfer-encoding:") && strings.Contains(hl, "base64") {
		clean := strings.ReplaceAll(strings.ReplaceAll(bodyPart, "\r\n", ""), "\n", "")
		decoded, err := base64.StdEncoding.DecodeString(clean)
		if err == nil {
			return strings.TrimSpace(string(decoded))
		}
	}
	if strings.Contains(hl, "content-transfer-encoding:") && strings.Contains(hl, "quoted-printable") {
		b, err := io.ReadAll(quotedprintable.NewReader(strings.NewReader(bodyPart)))
		if err == nil {
			return strings.TrimSpace(string(b))
		}
	}

	return strings.TrimSpace(bodyPart)
}

func maildropExtractHtmlFromMime(raw string) string {
	headerPart, bodyPart, ok := maildropSplitHeaderBody(raw)
	if !ok {
		return ""
	}

	boundary := maildropFindBoundary(headerPart)
	if boundary != "" {
		reader := multipart.NewReader(strings.NewReader(bodyPart), boundary)
		for {
			part, err := reader.NextPart()
			if err != nil {
				break
			}
			ct := part.Header.Get("Content-Type")
			if strings.Contains(strings.ToLower(ct), "text/html") {
				content, _ := io.ReadAll(part)
				cte := part.Header.Get("Content-Transfer-Encoding")
				out := maildropDecodeCTE(string(content), cte)
				part.Close()
				return strings.TrimSpace(out)
			}
			part.Close()
		}
	}

	hl := strings.ToLower(headerPart)
	if strings.Contains(hl, "content-type:") && strings.Contains(hl, "text/html") {
		cte := maildropHeaderValueCI(headerPart, "Content-Transfer-Encoding")
		return strings.TrimSpace(maildropDecodeCTE(bodyPart, cte))
	}

	return ""
}

func maildropStripHtmlToText(html string) string {
	if html == "" {
		return ""
	}
	var b strings.Builder
	inTag := false
	for _, r := range html {
		switch {
		case r == '<':
			inTag = true
		case r == '>':
			inTag = false
			b.WriteByte(' ')
		case !inTag:
			b.WriteRune(r)
		}
	}
	s := strings.Join(strings.Fields(b.String()), " ")
	return strings.TrimSpace(s)
}

/* maildropRandomUsername 生成随机用户名 */
func maildropRandomUsername(length int) string {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	b := make([]byte, length)
	for i := range b {
		b[i] = chars[rand.Intn(len(chars))]
	}
	return string(b)
}

/*
 * maildropGraphQL 发送 GraphQL 请求并返回解析后的 data 字段
 * 使用 operationName + variables 的标准 GraphQL 格式
 */
func maildropGraphQL(operationName string, query string, variables map[string]string) (json.RawMessage, error) {
	payload, _ := json.Marshal(map[string]interface{}{
		"operationName": operationName,
		"variables":     variables,
		"query":         query,
	})

	req, err := http.NewRequest("POST", maildropGraphQLURL, bytes.NewReader(payload))
	if err != nil {
		return nil, fmt.Errorf("maildrop create request failed: %w", err)
	}
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Origin", "https://maildrop.cc")
	req.Header.Set("Referer", "https://maildrop.cc/")
	req.Header.Set("User-Agent", GetCurrentUA())

	client := HTTPClient()
	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("maildrop graphql request failed: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != 200 {
		return nil, fmt.Errorf("maildrop graphql failed: %d", resp.StatusCode)
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("maildrop read body failed: %w", err)
	}

	var result struct {
		Data   json.RawMessage `json:"data"`
		Errors []struct {
			Message string `json:"message"`
		} `json:"errors"`
	}

	if err := json.Unmarshal(body, &result); err != nil {
		return nil, fmt.Errorf("maildrop parse response failed: %w", err)
	}

	if len(result.Errors) > 0 {
		return nil, fmt.Errorf("maildrop graphql error: %s", result.Errors[0].Message)
	}

	return result.Data, nil
}

/*
 * maildropGenerate 创建临时邮箱
 * Maildrop 无需注册，任意用户名即可接收邮件
 */
func maildropGenerate() (*EmailInfo, error) {
	username := maildropRandomUsername(10)
	email := username + "@" + maildropDomain

	/* 验证 API 可用 */
	_, err := maildropGraphQL(
		"GetInbox",
		"query GetInbox($mailbox: String!) { inbox(mailbox: $mailbox) { id } }",
		map[string]string{"mailbox": username},
	)
	if err != nil {
		return nil, err
	}

	return &EmailInfo{
		Channel: ChannelMaildrop,
		Email:   email,
		token:   username,
	}, nil
}

/*
 * maildropGetEmails 获取邮件列表
 * 先查 inbox 获取邮件 ID 列表，再逐封获取完整内容
 */
func maildropGetEmails(token string, email string) ([]Email, error) {
	mailbox := token

	/* 查询收件箱 */
	data, err := maildropGraphQL(
		"GetInbox",
		"query GetInbox($mailbox: String!) { inbox(mailbox: $mailbox) { id headerfrom subject date } }",
		map[string]string{"mailbox": mailbox},
	)
	if err != nil {
		return nil, err
	}

	var inboxResult struct {
		Inbox []struct {
			ID         string `json:"id"`
			HeaderFrom string `json:"headerfrom"`
			Subject    string `json:"subject"`
			Date       string `json:"date"`
		} `json:"inbox"`
	}

	if err := json.Unmarshal(data, &inboxResult); err != nil {
		return nil, fmt.Errorf("maildrop parse inbox failed: %w", err)
	}

	if len(inboxResult.Inbox) == 0 {
		return []Email{}, nil
	}

	/* 逐封获取完整内容 */
	emails := make([]Email, 0, len(inboxResult.Inbox))
	for _, item := range inboxResult.Inbox {
		msgData, err := maildropGraphQL(
			"GetMessage",
			"query GetMessage($mailbox: String!, $id: String!) { message(mailbox: $mailbox, id: $id) { id headerfrom subject date data html } }",
			map[string]string{"mailbox": mailbox, "id": item.ID},
		)
		if err != nil {
			continue
		}

		var msgResult struct {
			Message struct {
				ID         string `json:"id"`
				HeaderFrom string `json:"headerfrom"`
				Subject    string `json:"subject"`
				Date       string `json:"date"`
				Data       string `json:"data"`
				HTML       string `json:"html"`
			} `json:"message"`
		}

		if err := json.Unmarshal(msgData, &msgResult); err != nil {
			continue
		}

		msg := msgResult.Message
		plain := maildropExtractText(msg.Data)
		fromMime := maildropExtractHtmlFromMime(msg.Data)
		htmlOut := strings.TrimSpace(msg.HTML)
		if htmlOut == "" {
			htmlOut = fromMime
		}
		textOut := plain
		if textOut == "" {
			textOut = maildropStripHtmlToText(htmlOut)
		}
		emails = append(emails, Email{
			ID:          msg.ID,
			From:        maildropDecodeRfc2047(msg.HeaderFrom),
			To:          email,
			Subject:     maildropDecodeRfc2047(msg.Subject),
			Text:        textOut,
			HTML:        htmlOut,
			Date:        msg.Date,
			IsRead:      false,
			Attachments: []EmailAttachment{},
		})
	}

	return emails, nil
}
