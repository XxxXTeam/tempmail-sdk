package provider

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"math/rand"
	"strings"
	"sync"

	http "github.com/bogdanfinn/fhttp"
)

/*
 * maildrop.cc — https://maildrop.cc
 * GraphQL API，无认证
 * 创建邮箱: 无需 API，直接生成随机用户名 + "@maildrop.cc"（公共邮箱，任何人可查看任意地址收件箱）
 * 获取邮件: POST https://api.maildrop.cc/graphql
 *   - inbox(mailbox) 查询邮件列表（id/headerfrom/subject/date，无正文）
 *   - message(mailbox,id) 查询单封详情（含 html 正文）
 */

const maildropCcDomain = "maildrop.cc"
const maildropCcGraphQLURL = "https://api.maildrop.cc/graphql"

/* maildropCcRandomLocal 生成随机用户名（小写字母 + 数字） */
func maildropCcRandomLocal(n int) string {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	b := make([]byte, n)
	for i := range b {
		b[i] = chars[rand.Intn(len(chars))]
	}
	return string(b)
}

/* maildropCcMailbox 提取地址 @ 前的用户名部分 */
func maildropCcMailbox(email string) string {
	email = strings.TrimSpace(email)
	if i := strings.IndexByte(email, '@'); i >= 0 {
		return email[:i]
	}
	return email
}

/* maildropCcDefaultHeaders 设置请求头 */
func maildropCcDefaultHeaders(req *http.Request) {
	req.Header.Set("Accept", "application/json")
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Origin", "https://maildrop.cc")
	req.Header.Set("Referer", "https://maildrop.cc/")
	req.Header.Set("User-Agent", getCurrentUA())
}

/* maildropCcInboxItem inbox 查询返回的单条邮件元信息 */
type maildropCcInboxItem struct {
	ID         string `json:"id"`
	HeaderFrom string `json:"headerfrom"`
	Subject    string `json:"subject"`
	Date       string `json:"date"`
}

/* maildropCcInboxResponse inbox 查询响应 */
type maildropCcInboxResponse struct {
	Data struct {
		Inbox []maildropCcInboxItem `json:"inbox"`
	} `json:"data"`
}

/* maildropCcMessage message 查询返回的单封邮件详情 */
type maildropCcMessage struct {
	ID         string `json:"id"`
	HeaderFrom string `json:"headerfrom"`
	Subject    string `json:"subject"`
	Date       string `json:"date"`
	HTML       string `json:"html"`
}

/* maildropCcMessageResponse message 查询响应 */
type maildropCcMessageResponse struct {
	Data struct {
		Message maildropCcMessage `json:"message"`
	} `json:"data"`
}

/*
 * maildropCcDoGraphQL 发送 GraphQL 请求并解析到 out
 * query 通过 JSON 序列化构造 body，避免内联变量时的转义/注入问题
 */
func maildropCcDoGraphQL(query string, out interface{}) error {
	payload, err := json.Marshal(map[string]string{"query": query})
	if err != nil {
		return err
	}

	req, err := http.NewRequest("POST", maildropCcGraphQLURL, bytes.NewReader(payload))
	if err != nil {
		return err
	}
	maildropCcDefaultHeaders(req)

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return err
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return fmt.Errorf("maildrop-cc graphql: %d", resp.StatusCode)
	}

	return json.Unmarshal(body, out)
}

/* maildropCcInboxQuery 构造 inbox 列表查询语句 */
func maildropCcInboxQuery(mailbox string) string {
	mb, _ := json.Marshal(mailbox)
	return fmt.Sprintf("query { inbox(mailbox: %s) { id headerfrom subject date } }", string(mb))
}

/* maildropCcMessageQuery 构造 message 单封详情查询语句 */
func maildropCcMessageQuery(mailbox, id string) string {
	mb, _ := json.Marshal(mailbox)
	mid, _ := json.Marshal(id)
	return fmt.Sprintf("query { message(mailbox: %s, id: %s) { id headerfrom subject date html } }", string(mb), string(mid))
}

/*
 * MaildropCcGenerate 创建 maildrop.cc 临时邮箱
 * 无需 API 调用，直接生成随机用户名 + "@maildrop.cc"
 */
func MaildropCcGenerate() (*CreatedMailbox, error) {
	user := maildropCcRandomLocal(10)
	email := user + "@" + maildropCcDomain
	return &CreatedMailbox{Channel: "maildrop-cc", Email: email, Token: ""}, nil
}

/*
 * MaildropCcGetEmails 获取 maildrop.cc 邮件列表
 * 先用 inbox 查询拿到 id 列表，再并发用 message 查询逐封补全 html 正文
 * maildrop.cc 为公共邮箱服务，无需 token（token 参数保留以对齐接口，忽略）
 */
func MaildropCcGetEmails(email, token string) ([]NormEmail, error) {
	_ = token
	mailbox := maildropCcMailbox(email)
	if mailbox == "" {
		return nil, fmt.Errorf("maildrop-cc: empty email")
	}

	/* 1. 查询邮件列表 */
	var inboxResp maildropCcInboxResponse
	if err := maildropCcDoGraphQL(maildropCcInboxQuery(mailbox), &inboxResp); err != nil {
		return nil, err
	}
	items := inboxResp.Data.Inbox
	if len(items) == 0 {
		return []NormEmail{}, nil
	}

	/* 2. 并发查询每封邮件详情，补全 html 正文 */
	type detailResult struct {
		msg maildropCcMessage
		ok  bool
	}
	results := make([]detailResult, len(items))
	var wg sync.WaitGroup
	for i, item := range items {
		wg.Add(1)
		go func(idx int, id string) {
			defer wg.Done()
			var msgResp maildropCcMessageResponse
			if err := maildropCcDoGraphQL(maildropCcMessageQuery(mailbox, id), &msgResp); err != nil {
				return
			}
			results[idx] = detailResult{msg: msgResp.Data.Message, ok: true}
		}(i, item.ID)
	}
	wg.Wait()

	/* 3. 扁平化并标准化；详情查询失败时回退使用列表元信息 */
	emails := make([]NormEmail, 0, len(items))
	for i, item := range items {
		var flat map[string]interface{}
		if results[i].ok {
			m := results[i].msg
			flat = map[string]interface{}{
				"id":      m.ID,
				"from":    m.HeaderFrom,
				"subject": m.Subject,
				"date":    m.Date,
				"html":    m.HTML,
			}
		} else {
			flat = map[string]interface{}{
				"id":      item.ID,
				"from":    item.HeaderFrom,
				"subject": item.Subject,
				"date":    item.Date,
			}
		}
		emails = append(emails, NormalizeMap(flat, email))
	}

	return emails, nil
}
