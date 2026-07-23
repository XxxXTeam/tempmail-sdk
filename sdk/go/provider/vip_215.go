package provider

import (
	"encoding/json"
	"fmt"
	ht "html"
	"io"
	"math"
	"net/http"
	"net/url"
	"strings"
	"sync"
	"time"

	fhttp "github.com/bogdanfinn/fhttp"
	"github.com/gorilla/websocket"
)

const vip215HTTPBase = "https://vip.215.im"
const vip215WSHost = "vip.215.im"

const vip215UserAgent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/148.0.0.0 Safari/537.36 Edg/148.0.0.0"

var vip215RegMu sync.Mutex
var vip215Boxes = make(map[string]*vip215Box)

type vip215Box struct {
	mu      sync.Mutex
	emails  []NormEmail
	seenIDs map[string]struct{}
	started bool
}

func getVip215Box(token string) *vip215Box {
	vip215RegMu.Lock()
	defer vip215RegMu.Unlock()
	b := vip215Boxes[token]
	if b == nil {
		b = &vip215Box{seenIDs: make(map[string]struct{})}
		vip215Boxes[token] = b
	}
	return b
}

const vip215SyntheticMarker = "【tempmail-sdk|synthetic|vip-215|v1】"

func vip215SanitizeOneLine(v interface{}) string {
	if v == nil {
		return ""
	}
	var s string
	switch t := v.(type) {
	case string:
		s = t
	case float64:
		if t == math.Trunc(t) {
			s = fmt.Sprintf("%.0f", t)
		} else {
			s = fmt.Sprintf("%g", t)
		}
	case bool:
		s = fmt.Sprintf("%v", t)
	default:
		s = fmt.Sprintf("%v", t)
	}
	s = strings.ReplaceAll(s, "\r\n", " ")
	s = strings.ReplaceAll(s, "\r", " ")
	s = strings.ReplaceAll(s, "\n", " ")
	return strings.TrimSpace(s)
}

func vip215OptionalSize(v interface{}) (int64, bool) {
	if v == nil {
		return 0, false
	}
	switch t := v.(type) {
	case float64:
		if math.IsNaN(t) || t < 0 {
			return 0, false
		}
		return int64(t), true
	case json.Number:
		n, err := t.Int64()
		if err != nil || n < 0 {
			return 0, false
		}
		return n, true
	default:
		return 0, false
	}
}

func vip215SyntheticBodies(recipient string, data map[string]interface{}) (plain string, htmlDoc string) {
	id := vip215SanitizeOneLine(data["id"])
	subj := vip215SanitizeOneLine(data["subject"])
	from := vip215SanitizeOneLine(data["from"])
	to := vip215SanitizeOneLine(recipient)
	date := vip215SanitizeOneLine(data["date"])
	lines := []string{
		vip215SyntheticMarker,
		"id: " + id,
		"subject: " + subj,
		"from: " + from,
		"to: " + to,
		"date: " + date,
	}
	if sz, ok := vip215OptionalSize(data["size"]); ok {
		lines = append(lines, fmt.Sprintf("size: %d", sz))
	}
	plain = strings.Join(lines, "\n")

	var b strings.Builder
	b.WriteString(`<div class="tempmail-sdk-synthetic" data-tempmail-sdk-format="synthetic-v1" data-channel="vip-215"><dl class="tempmail-sdk-meta">`)
	writePair := func(k, v string) {
		b.WriteString("<dt>")
		b.WriteString(ht.EscapeString(k))
		b.WriteString("</dt><dd>")
		b.WriteString(ht.EscapeString(v))
		b.WriteString("</dd>")
	}
	writePair("id", id)
	writePair("subject", subj)
	writePair("from", from)
	writePair("to", to)
	writePair("date", date)
	if sz, ok := vip215OptionalSize(data["size"]); ok {
		writePair("size", fmt.Sprintf("%d", sz))
	}
	b.WriteString("</dl></div>")
	htmlDoc = b.String()
	return plain, htmlDoc
}

type vip215CreateResp struct {
	Success bool `json:"success"`
	Data    *struct {
		Address   string `json:"address"`
		Token     string `json:"token"`
		CreatedAt string `json:"createdAt"`
		ExpiresAt string `json:"expiresAt"`
	} `json:"data"`
}

type vip215WsTicketResp struct {
	Success bool `json:"success"`
	Data    *struct {
		Ticket string `json:"ticket"`
	} `json:"data"`
}

func vip215JoinCookies(cookies []*fhttp.Cookie) string {
	var parts []string
	for _, c := range cookies {
		if c == nil || c.Name == "" {
			continue
		}
		parts = append(parts, c.Name+"="+c.Value)
	}
	return strings.Join(parts, "; ")
}

func vip215SetAPIHeaders(h fhttp.Header) {
	h.Set("User-Agent", vip215UserAgent)
	h.Set("Accept", "*/*")
	h.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6")
	h.Set("Cache-Control", "no-cache")
	h.Set("Content-Type", "application/json")
	h.Set("DNT", "1")
	h.Set("Origin", vip215HTTPBase)
	h.Set("Pragma", "no-cache")
	h.Set("Referer", vip215HTTPBase+"/")
	h.Set("Sec-CH-UA", `"Chromium";v="148", "Microsoft Edge";v="148", "Not/A)Brand";v="99"`)
	h.Set("Sec-CH-UA-Mobile", "?0")
	h.Set("Sec-CH-UA-Platform", `"Windows"`)
	h.Set("Sec-Fetch-Dest", "empty")
	h.Set("Sec-Fetch-Mode", "cors")
	h.Set("Sec-Fetch-Site", "same-origin")
	h.Set("X-Locale", "zh")
}

func vip215EstablishSession() (string, error) {
	req, err := fhttp.NewRequest("GET", vip215HTTPBase+"/", nil)
	if err != nil {
		return "", err
	}
	h := req.Header
	h.Set("User-Agent", vip215UserAgent)
	h.Set("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8")
	h.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6")
	h.Set("Cache-Control", "no-cache")
	h.Set("DNT", "1")
	h.Set("Pragma", "no-cache")
	h.Set("Sec-CH-UA", `"Chromium";v="148", "Microsoft Edge";v="148", "Not/A)Brand";v="99"`)
	h.Set("Sec-CH-UA-Mobile", "?0")
	h.Set("Sec-CH-UA-Platform", `"Windows"`)
	h.Set("Sec-Fetch-Dest", "document")
	h.Set("Sec-Fetch-Mode", "navigate")
	h.Set("Sec-Fetch-Site", "same-origin")
	h.Set("Sec-Fetch-User", "?1")
	h.Set("Upgrade-Insecure-Requests", "1")

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()
	_, _ = io.Copy(io.Discard, resp.Body)
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return "", fmt.Errorf("vip-215 homepage failed: %d", resp.StatusCode)
	}
	cookie := vip215JoinCookies(resp.Cookies())
	if !strings.Contains(cookie, "yyds_homepage_bridge=") || !strings.Contains(cookie, "yyds_homepage_device=") {
		return "", fmt.Errorf("vip-215: missing homepage cookies")
	}
	return cookie, nil
}

func vip215FetchWsTicket(jwt string) (string, error) {
	req, err := fhttp.NewRequest("GET", vip215HTTPBase+"/v1/auth/ws-ticket", nil)
	if err != nil {
		return "", err
	}
	vip215SetAPIHeaders(req.Header)
	req.Header.Set("Authorization", "Bearer "+jwt)

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", err
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return "", fmt.Errorf("vip-215 ws-ticket failed: %d %s", resp.StatusCode, string(body))
	}
	var parsed vip215WsTicketResp
	if err := json.Unmarshal(body, &parsed); err != nil {
		return "", err
	}
	if !parsed.Success || parsed.Data == nil || parsed.Data.Ticket == "" {
		return "", fmt.Errorf("vip-215: invalid ws-ticket response")
	}
	return parsed.Data.Ticket, nil
}

func MailVip215Generate() (*CreatedMailbox, error) {
	cookie, err := vip215EstablishSession()
	if err != nil {
		return nil, err
	}

	req, err := fhttp.NewRequest("POST", vip215HTTPBase+"/api/temp-inbox", nil)
	if err != nil {
		return nil, err
	}
	vip215SetAPIHeaders(req.Header)
	req.Header.Set("Cookie", cookie)

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
		return nil, fmt.Errorf("vip-215 create inbox failed: %d %s", resp.StatusCode, string(body))
	}

	var parsed vip215CreateResp
	if err := json.Unmarshal(body, &parsed); err != nil {
		return nil, err
	}
	if !parsed.Success || parsed.Data == nil || parsed.Data.Address == "" || parsed.Data.Token == "" {
		return nil, fmt.Errorf("vip-215: invalid API response")
	}

	info := &CreatedMailbox{Channel: "vip-215", Email: parsed.Data.Address, Token: parsed.Data.Token}
	info.CreatedAt = parsed.Data.CreatedAt
	info.ExpiresAt = parsed.Data.ExpiresAt
	vip215EnsureReader(info.Token, info.Email)
	return info, nil
}

func vip215WsLoop(jwt, recipient string, box *vip215Box) {
	wsTicket, err := vip215FetchWsTicket(jwt)
	if err != nil {
		return
	}

	u := url.URL{Scheme: "wss", Host: vip215WSHost, Path: "/v1/ws"}
	q := u.Query()
	q.Set("token", wsTicket)
	u.RawQuery = q.Encode()

	hdr := http.Header{}
	hdr.Set("Origin", vip215HTTPBase)
	hdr.Set("User-Agent", vip215UserAgent)

	d := websocket.Dialer{
		HandshakeTimeout: 15 * time.Second,
	}
	conn, _, err := d.Dial(u.String(), hdr)
	if err != nil {
		return
	}
	defer conn.Close()

	for {
		_, msg, err := conn.ReadMessage()
		if err != nil {
			return
		}
		var outer struct {
			Type string          `json:"type"`
			Data json.RawMessage `json:"data"`
		}
		if err := json.Unmarshal(msg, &outer); err != nil {
			continue
		}
		if outer.Type != "message.new" || len(outer.Data) == 0 {
			continue
		}
		var data map[string]interface{}
		if err := json.Unmarshal(outer.Data, &data); err != nil {
			continue
		}
		tPlain, tHTML := vip215SyntheticBodies(recipient, data)
		raw := map[string]interface{}{
			"id":      data["id"],
			"from":    data["from"],
			"subject": data["subject"],
			"date":    data["date"],
			"to":      recipient,
			"text":    tPlain,
			"html":    tHTML,
		}
		em := NormalizeMap(raw, recipient)
		if strings.TrimSpace(em.ID) == "" {
			continue
		}
		box.mu.Lock()
		if _, dup := box.seenIDs[em.ID]; dup {
			box.mu.Unlock()
			continue
		}
		box.seenIDs[em.ID] = struct{}{}
		box.emails = append(box.emails, em)
		box.mu.Unlock()
	}
}

func vip215EnsureReader(token, email string) *vip215Box {
	box := getVip215Box(token)
	box.mu.Lock()
	needStart := !box.started
	if needStart {
		box.started = true
	}
	box.mu.Unlock()

	if needStart {
		go vip215WsLoop(token, email, box)
		time.Sleep(80 * time.Millisecond)
	}
	return box
}

/*
 * vip215FetchMessagesViaHTTP 通过 HTTP 接口获取邮件列表
 * GET https://vip.215.im/v1/messages
 * 返回完整邮件列表（含 text/html 字段），供 HTTP 拉取模式使用
 * 相比 WebSocket 推送的元数据 + 合成卡片，HTTP 接口能拿到完整正文
 */
func vip215FetchMessagesViaHTTP(jwt string) ([]map[string]interface{}, error) {
	req, err := fhttp.NewRequest("GET", vip215HTTPBase+"/v1/messages", nil)
	if err != nil {
		return nil, err
	}
	vip215SetAPIHeaders(req.Header)
	req.Header.Set("Authorization", "Bearer "+jwt)

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
		return nil, fmt.Errorf("vip-215 messages: %d %s", resp.StatusCode, string(body))
	}

	var parsed struct {
		Success bool `json:"success"`
		Data    struct {
			Messages []map[string]interface{} `json:"messages"`
			Total    int                      `json:"total"`
		} `json:"data"`
	}
	if err := json.Unmarshal(body, &parsed); err != nil {
		return nil, err
	}
	if !parsed.Success {
		return nil, fmt.Errorf("vip-215: messages success=false")
	}
	return parsed.Data.Messages, nil
}

/*
 * vip215FetchMessageDetail 通过 HTTP 详情接口获取单封邮件完整内容
 * GET https://vip.215.im/v1/messages/{id}
 * 用于列表接口只返回元数据时补拉正文
 */
func vip215FetchMessageDetail(jwt, id string) map[string]interface{} {
	id = strings.TrimSpace(id)
	if id == "" {
		return nil
	}
	req, err := fhttp.NewRequest("GET", vip215HTTPBase+"/v1/messages/"+url.PathEscape(id), nil)
	if err != nil {
		return nil
	}
	vip215SetAPIHeaders(req.Header)
	req.Header.Set("Authorization", "Bearer "+jwt)

	resp, err := HTTPClient().Do(req)
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
	var parsed struct {
		Success bool                   `json:"success"`
		Data    map[string]interface{} `json:"data"`
	}
	if err := json.Unmarshal(body, &parsed); err != nil {
		return nil
	}
	if !parsed.Success || parsed.Data == nil {
		return nil
	}
	return parsed.Data
}

/*
 * vip215HasRealBody 判断邮件条目是否已包含真实正文
 * 用于决定是否需要补拉详情接口
 */
func vip215HasRealBody(row map[string]interface{}) bool {
	for _, key := range []string{"text", "body_text", "html", "body_html", "content", "body"} {
		if v, ok := row[key]; ok && v != nil {
			if s, ok := v.(string); ok && strings.TrimSpace(s) != "" {
				return true
			}
		}
	}
	return false
}

/*
 * vip215ExtractMessageID 从 HTTP 消息 map 中提取邮件 ID
 */
func vip215ExtractMessageID(row map[string]interface{}) string {
	for _, key := range []string{"id", "messageId", "message_id"} {
		if v, ok := row[key]; ok && v != nil {
			switch val := v.(type) {
			case string:
				return strings.TrimSpace(val)
			case float64:
				return fmt.Sprintf("%.0f", val)
			}
		}
	}
	return ""
}

/*
 * MailVip215GetEmails 获取 vip-215 邮件列表（HTTP 优先，WebSocket 回退）
 * 流程:
 *   1. 调用 HTTP GET /v1/messages 获取完整邮件列表（含真实正文）
 *   2. 对缺正文的条目调用 GET /v1/messages/{id} 补详情
 *   3. HTTP 失败时回退到 WebSocket 已缓存的合成卡片
 * WebSocket 通道仍保留用于新邮件通知（去重合并）
 */
func MailVip215GetEmails(token, email string) ([]NormEmail, error) {
	box := vip215EnsureReader(token, email)

	/* 优先通过 HTTP 拉取完整邮件 */
	messages, err := vip215FetchMessagesViaHTTP(token)
	if err == nil {
		out := make([]NormEmail, 0, len(messages))
		for _, row := range messages {
			id := vip215ExtractMessageID(row)
			/* 若列表条目无真实正文，补拉详情 */
			if !vip215HasRealBody(row) && id != "" {
				if detail := vip215FetchMessageDetail(token, id); detail != nil {
					for k, v := range detail {
						if v != nil {
							row[k] = v
						}
					}
				}
			}
			/* 补齐 to 字段 */
			if _, ok := row["to"]; !ok {
				row["to"] = email
			}
			out = append(out, NormalizeMap(row, email))
		}
		return out, nil
	}

	/* HTTP 失败时回退到 WebSocket 缓存（可能含合成卡片） */
	box.mu.Lock()
	defer box.mu.Unlock()
	fallback := make([]NormEmail, len(box.emails))
	copy(fallback, box.emails)
	return fallback, nil
}
