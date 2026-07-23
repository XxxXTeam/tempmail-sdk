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

/**
 * Maildrop — https://maildrop.cx
 * GET /api/suffixes.php、GET /api/emails.php
 */

const maildropBase = "https://maildrop.cx"
const maildropExcludedSuffix = "transformer.edu.kg"

func maildropDefaultHeaders(req *http.Request) {
	req.Header.Set("Accept", "application/json, text/plain, */*")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6")
	req.Header.Set("Cache-Control", "no-cache")
	req.Header.Set("DNT", "1")
	req.Header.Set("Pragma", "no-cache")
	req.Header.Set("Referer", "https://maildrop.cx/zh-cn/app")
	req.Header.Set("sec-ch-ua", `"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"`)
	req.Header.Set("sec-ch-ua-mobile", "?0")
	req.Header.Set("sec-ch-ua-platform", `"Windows"`)
	req.Header.Set("sec-fetch-dest", "empty")
	req.Header.Set("sec-fetch-mode", "cors")
	req.Header.Set("sec-fetch-site", "same-origin")
	req.Header.Set("User-Agent", GetCurrentUA())
	req.Header.Set("x-requested-with", "XMLHttpRequest")
}

func maildropRandomLocal(n int) string {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	b := make([]byte, n)
	for i := range b {
		b[i] = chars[rand.Intn(len(chars))]
	}
	return string(b)
}

func maildropFetchSuffixes() ([]string, error) {
	req, err := http.NewRequest("GET", maildropBase+"/api/suffixes.php", nil)
	if err != nil {
		return nil, err
	}
	maildropDefaultHeaders(req)

	client := HTTPClient()
	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("maildrop suffixes request: %w", err)
	}
	defer resp.Body.Close()
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("maildrop suffixes: %d", resp.StatusCode)
	}
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	var arr []string
	if err := json.Unmarshal(body, &arr); err != nil {
		return nil, fmt.Errorf("maildrop suffixes parse: %w", err)
	}
	ex := strings.ToLower(maildropExcludedSuffix)
	var out []string
	for _, s := range arr {
		t := strings.TrimSpace(s)
		if t == "" || strings.ToLower(t) == ex {
			continue
		}
		out = append(out, t)
	}
	if len(out) == 0 {
		return nil, fmt.Errorf("maildrop: no domains available")
	}
	return out, nil
}

func maildropPickSuffix(suffixes []string, preferred *string) (string, error) {
	if preferred != nil {
		p := strings.TrimSpace(*preferred)
		if p != "" {
			pl := strings.ToLower(p)
			for _, d := range suffixes {
				if strings.ToLower(d) == pl {
					return d, nil
				}
			}
			return "", fmt.Errorf("maildrop: domain not available: %s", pl)
		}
	}
	return suffixes[rand.Intn(len(suffixes))], nil
}

func maildropCxDateToRFC3339(s string) string {
	s = strings.TrimSpace(s)
	if len(s) == 19 && s[10] == ' ' {
		return s[:10] + "T" + s[11:] + "Z"
	}
	return s
}

/*
 * MaildropGenerate 随机本地部分 + 可选指定域名（须在后缀列表中且未被排除）
 */
func MaildropGenerate(domain *string) (*CreatedMailbox, error) {
	suffixes, err := maildropFetchSuffixes()
	if err != nil {
		return nil, err
	}
	dom, err := maildropPickSuffix(suffixes, domain)
	if err != nil {
		return nil, err
	}
	local := maildropRandomLocal(10)
	email := local + "@" + dom
	return &CreatedMailbox{Channel: "maildrop", Email: email, Token: email}, nil
}

type maildropEmailsResp struct {
	Emails []struct {
		ID          json.RawMessage `json:"id"`
		FromAddr    string          `json:"from_addr"`
		Subject     string          `json:"subject"`
		Date        string          `json:"date"`
		IsRead      json.RawMessage `json:"isRead"`
		Description string          `json:"description"`
	} `json:"emails"`
}

func maildropRawToString(raw json.RawMessage) string {
	if len(raw) == 0 {
		return ""
	}
	var s string
	if json.Unmarshal(raw, &s) == nil {
		return s
	}
	var n float64
	if json.Unmarshal(raw, &n) == nil {
		return fmt.Sprintf("%.0f", n)
	}
	return string(raw)
}

func maildropParseIsRead(raw json.RawMessage) bool {
	if len(raw) == 0 {
		return false
	}
	var b bool
	if json.Unmarshal(raw, &b) == nil {
		return b
	}
	var n int
	if json.Unmarshal(raw, &n) == nil {
		return n != 0
	}
	return false
}

/*
 * maildropFetchDetail 通过详情接口获取单封邮件完整内容
 * GET https://maildrop.cx/api/email_content.php?id={id}
 * 详情响应字段（从前端代码确认）:
 *   - content: 完整 HTML 正文
 *   - subject / from_addr / date: 邮件元数据
 *   - attachment: JSON 字符串数组 [{filename, path, size}]（可能为空）
 * 失败时返回 nil，调用方回退到列表 description
 */
func maildropFetchDetail(id string) map[string]interface{} {
	id = strings.TrimSpace(id)
	if id == "" {
		return nil
	}
	q := url.Values{}
	q.Set("id", id)
	full := maildropBase + "/api/email_content.php?" + q.Encode()

	req, err := http.NewRequest("GET", full, nil)
	if err != nil {
		return nil
	}
	maildropDefaultHeaders(req)

	client := HTTPClient()
	resp, err := client.Do(req)
	if err != nil {
		return nil
	}
	defer resp.Body.Close()
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil
	}
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil
	}
	var detail map[string]interface{}
	if err := json.Unmarshal(body, &detail); err != nil {
		return nil
	}
	return detail
}

/*
 * maildropParseAttachments 将详情接口的 attachment 字段（JSON 字符串）
 * 解析为归一化附件列表
 */
func maildropParseAttachments(raw interface{}) []NormAttachment {
	if raw == nil {
		return []NormAttachment{}
	}
	s, ok := raw.(string)
	if !ok || strings.TrimSpace(s) == "" {
		return []NormAttachment{}
	}
	var items []map[string]interface{}
	if err := json.Unmarshal([]byte(s), &items); err != nil {
		return []NormAttachment{}
	}
	out := make([]NormAttachment, 0, len(items))
	for _, it := range items {
		filename, _ := it["filename"].(string)
		if strings.TrimSpace(filename) == "" {
			continue
		}
		att := NormAttachment{Filename: filename}
		if sz, ok := it["size"].(float64); ok {
			att.Size = int64(sz)
		}
		out = append(out, att)
	}
	return out
}

/*
 * MaildropGetEmails 获取邮件列表并对每封邮件补拉详情
 * 流程:
 *   1. GET /api/emails.php?addr={email} 拉取列表（仅含 description 摘要）
 *   2. 对每封邮件 GET /api/email_content.php?id={id} 拉取详情（含 content 完整 HTML）
 *   3. 详情失败时保留列表 description 作为回退
 */
func MaildropGetEmails(token string, email string) ([]NormEmail, error) {
	addr := strings.TrimSpace(email)
	if addr == "" {
		addr = strings.TrimSpace(token)
	}
	if addr == "" {
		return nil, fmt.Errorf("maildrop: empty address")
	}
	q := url.Values{}
	q.Set("addr", addr)
	q.Set("page", "1")
	q.Set("limit", "20")
	full := maildropBase + "/api/emails.php?" + q.Encode()

	req, err := http.NewRequest("GET", full, nil)
	if err != nil {
		return nil, err
	}
	maildropDefaultHeaders(req)

	client := HTTPClient()
	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("maildrop emails: %w", err)
	}
	defer resp.Body.Close()
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("maildrop emails: %d", resp.StatusCode)
	}
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	var data maildropEmailsResp
	if err := json.Unmarshal(body, &data); err != nil {
		return nil, fmt.Errorf("maildrop emails parse: %w", err)
	}

	out := make([]NormEmail, 0, len(data.Emails))
	for _, row := range data.Emails {
		id := maildropRawToString(row.ID)
		desc := strings.TrimSpace(row.Description)

		/* 默认使用列表字段构造 */
		item := NormEmail{
			ID:          id,
			From:        strings.TrimSpace(row.FromAddr),
			To:          addr,
			Subject:     strings.TrimSpace(row.Subject),
			Text:        desc,
			HTML:        "",
			Date:        maildropCxDateToRFC3339(row.Date),
			IsRead:      maildropParseIsRead(row.IsRead),
			Attachments: []NormAttachment{},
		}

		/* 拉取详情覆盖 text/html/attachments */
		if id != "" {
			if detail := maildropFetchDetail(id); detail != nil {
				if content, ok := detail["content"].(string); ok && strings.TrimSpace(content) != "" {
					item.HTML = content
					/* text 字段维持列表 description（作为纯文本摘要） */
				}
				/* 详情返回的 from_addr / subject / date 优先级更高 */
				if fromAddr, ok := detail["from_addr"].(string); ok && strings.TrimSpace(fromAddr) != "" {
					item.From = strings.TrimSpace(fromAddr)
				}
				if subj, ok := detail["subject"].(string); ok && strings.TrimSpace(subj) != "" {
					item.Subject = strings.TrimSpace(subj)
				}
				if dateStr, ok := detail["date"].(string); ok && strings.TrimSpace(dateStr) != "" {
					item.Date = maildropCxDateToRFC3339(dateStr)
				}
				/* 解析附件 */
				if atts := maildropParseAttachments(detail["attachment"]); len(atts) > 0 {
					item.Attachments = atts
				}
			}
		}

		out = append(out, item)
	}
	return out, nil
}
