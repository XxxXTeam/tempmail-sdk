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
	"net/http"
	"strings"
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

/*
 * maildropExtractText 从原始 MIME 邮件源码中提取纯文本正文
 * maildrop 的 data 字段返回完整 MIME 源码，需要解析出 text/plain 部分
 */
func maildropExtractText(raw string) string {
	if raw == "" {
		return ""
	}

	/* 分离邮件头和正文 */
	splitIdx := strings.Index(raw, "\r\n\r\n")
	if splitIdx == -1 {
		return raw
	}
	headerPart := raw[:splitIdx]
	bodyPart := raw[splitIdx+4:]

	/* 检查是否为 multipart */
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

	/* 如果找不到 boundary，尝试从原始头中提取 */
	if boundary == "" {
		ctIdx := strings.Index(strings.ToLower(headerPart), "content-type:")
		if ctIdx >= 0 {
			ctLine := headerPart[ctIdx:]
			endIdx := strings.Index(ctLine, "\r\n\r\n")
			if endIdx == -1 {
				endIdx = len(ctLine)
			}
			/* 合并多行头 */
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
				cte := strings.ToLower(part.Header.Get("Content-Transfer-Encoding"))
				if cte == "base64" {
					decoded, err := base64.StdEncoding.DecodeString(strings.ReplaceAll(string(content), "\r\n", ""))
					if err == nil {
						return strings.TrimSpace(string(decoded))
					}
				}
				return strings.TrimSpace(string(content))
			}
			part.Close()
		}
	}

	/* 非 multipart：检查整体编码 */
	if strings.Contains(strings.ToLower(headerPart), "content-transfer-encoding: base64") {
		decoded, err := base64.StdEncoding.DecodeString(strings.ReplaceAll(bodyPart, "\r\n", ""))
		if err == nil {
			return strings.TrimSpace(string(decoded))
		}
	}

	return strings.TrimSpace(bodyPart)
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
	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36")

	resp, err := http.DefaultClient.Do(req)
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
		Token:   username,
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
		emails = append(emails, Email{
			ID:          msg.ID,
			From:        maildropDecodeRfc2047(msg.HeaderFrom),
			To:          email,
			Subject:     maildropDecodeRfc2047(msg.Subject),
			Text:        maildropExtractText(msg.Data),
			HTML:        msg.HTML,
			Date:        msg.Date,
			IsRead:      false,
			Attachments: []EmailAttachment{},
		})
	}

	return emails, nil
}
