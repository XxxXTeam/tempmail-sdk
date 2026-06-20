package provider

import (
	"encoding/json"
	"fmt"
	"net/http"
	"strings"
	"sync"
	"time"

	"github.com/gorilla/websocket"
)

// Socket.IO 临时邮箱共享实现
// 用于 mjj.cm、mail.xiuvi.cn、linshi.co 等使用相同 Socket.IO 协议的站点

const (
	sioConnectTimeout  = 15 * time.Second
	sioHandshakeWait   = 1 * time.Second
	sioInitialSyncWait = 80 * time.Millisecond
)

var sioVersions = []int{4, 3}

type sioBoxState struct {
	mu      sync.Mutex
	emails  []NormEmail
	seenIDs map[string]bool
	ws      *websocket.Conn
	email   string
	channel string
}

type sioProvider struct {
	channel     string
	defaultHost string
	mu          sync.Mutex
	mailboxes   map[string]*sioBoxState
}

func newSioProvider(channel, defaultHost string) *sioProvider {
	return &sioProvider{
		channel:     channel,
		defaultHost: defaultHost,
		mailboxes:   make(map[string]*sioBoxState),
	}
}

func sioSocketURL(host string, eio int) string {
	return fmt.Sprintf("wss://%s/socket.io/?EIO=%d&transport=websocket", host, eio)
}

func sioParseEventPacket(packet string) (event string, payload json.RawMessage, ok bool) {
	if !strings.HasPrefix(packet, "42") {
		return "", nil, false
	}
	var arr []json.RawMessage
	if err := json.Unmarshal([]byte(packet[2:]), &arr); err != nil || len(arr) < 2 {
		return "", nil, false
	}
	var ev string
	if err := json.Unmarshal(arr[0], &ev); err != nil {
		return "", nil, false
	}
	return ev, arr[1], true
}

func sioSendEvent(ws *websocket.Conn, event string, payload interface{}) error {
	arr := []interface{}{event, payload}
	raw, err := json.Marshal(arr)
	if err != nil {
		return err
	}
	return ws.WriteMessage(websocket.TextMessage, []byte("42"+string(raw)))
}

func (p *sioProvider) connectSocket(host string) (*websocket.Conn, error) {
	var lastErr error
	dialer := websocket.Dialer{
		HandshakeTimeout: sioConnectTimeout,
	}
	headers := http.Header{
		"User-Agent":      {"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"},
		"Accept-Language": {"zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6"},
		"Cache-Control":   {"no-cache"},
		"DNT":             {"1"},
		"Pragma":          {"no-cache"},
		"Origin":          {fmt.Sprintf("https://%s", host)},
	}

	for _, version := range sioVersions {
		url := sioSocketURL(host, version)
		ws, _, err := dialer.Dial(url, headers)
		if err != nil {
			lastErr = err
			continue
		}

		// 等待 Engine.IO open 包并发送 Socket.IO connect
		ws.SetReadDeadline(time.Now().Add(sioConnectTimeout))
		sentConnect := false
		connected := false

		for i := 0; i < 10; i++ {
			_, msg, err := ws.ReadMessage()
			if err != nil {
				lastErr = err
				ws.Close()
				break
			}
			packet := string(msg)

			if packet == "2" {
				ws.WriteMessage(websocket.TextMessage, []byte("3"))
				continue
			}

			if !sentConnect {
				if !strings.HasPrefix(packet, "0") {
					lastErr = fmt.Errorf("%s: unexpected open packet for EIO=%d", p.channel, version)
					ws.Close()
					break
				}
				sentConnect = true
				ws.WriteMessage(websocket.TextMessage, []byte("40"))
				// 等待 40 确认或超时后继续
				time.Sleep(sioHandshakeWait)
				connected = true
				break
			}

			if strings.HasPrefix(packet, "40") {
				connected = true
				break
			}
		}

		if connected {
			ws.SetReadDeadline(time.Time{})
			return ws, nil
		}
		if lastErr == nil {
			lastErr = fmt.Errorf("%s: websocket handshake failed for EIO=%d", p.channel, version)
		}
	}

	if lastErr == nil {
		lastErr = fmt.Errorf("%s: websocket handshake failed", p.channel)
	}
	return nil, lastErr
}

func (p *sioProvider) requestShortID(host string) (string, error) {
	ws, err := p.connectSocket(host)
	if err != nil {
		return "", err
	}
	defer ws.Close()

	if err := sioSendEvent(ws, "request shortid", true); err != nil {
		return "", err
	}

	ws.SetReadDeadline(time.Now().Add(sioConnectTimeout))
	for i := 0; i < 50; i++ {
		_, msg, err := ws.ReadMessage()
		if err != nil {
			return "", fmt.Errorf("%s: 等待 shortid 时连接断开: %w", p.channel, err)
		}
		packet := string(msg)
		if packet == "2" {
			ws.WriteMessage(websocket.TextMessage, []byte("3"))
			continue
		}
		event, payload, ok := sioParseEventPacket(packet)
		if !ok || event != "shortid" {
			continue
		}
		var shortid string
		if err := json.Unmarshal(payload, &shortid); err != nil {
			continue
		}
		return shortid, nil
	}
	return "", fmt.Errorf("%s: 等待 shortid 超时", p.channel)
}

func (p *sioProvider) Generate() (*CreatedMailbox, error) {
	host := p.defaultHost
	shortid, err := p.requestShortID(host)
	if err != nil {
		return nil, err
	}
	email := fmt.Sprintf("%s@%s", shortid, host)
	if err := p.ensureMailbox(email); err != nil {
		return nil, err
	}
	return &CreatedMailbox{
		Channel: p.channel,
		Email:   email,
	}, nil
}

func (p *sioProvider) getState(email string) *sioBoxState {
	key := strings.ToLower(strings.TrimSpace(email))
	p.mu.Lock()
	defer p.mu.Unlock()
	st, ok := p.mailboxes[key]
	if !ok {
		st = &sioBoxState{
			emails:  make([]NormEmail, 0),
			seenIDs: make(map[string]bool),
			email:   email,
			channel: p.channel,
		}
		p.mailboxes[key] = st
	}
	return st
}

func sioFlattenMail(raw map[string]interface{}, recipientEmail string) map[string]interface{} {
	headers, _ := raw["headers"].(map[string]interface{})
	if headers == nil {
		headers = map[string]interface{}{}
	}

	id := ""
	if v, ok := raw["id"]; ok {
		id = fmt.Sprintf("%v", v)
	} else if v, ok := raw["messageId"]; ok {
		id = fmt.Sprintf("%v", v)
	} else if v, ok := headers["message-id"]; ok {
		id = fmt.Sprintf("%v", v)
	} else {
		from, _ := headers["from"]
		if from == nil {
			from = raw["from"]
		}
		subj, _ := headers["subject"]
		if subj == nil {
			subj = raw["subject"]
		}
		dt, _ := headers["date"]
		if dt == nil {
			dt = raw["date"]
		}
		id = fmt.Sprintf("%v\n%v\n%v\n%s", from, subj, dt, recipientEmail)
	}

	from := ""
	if v, ok := headers["from"]; ok {
		from = fmt.Sprintf("%v", v)
	} else if v, ok := raw["from"]; ok {
		from = fmt.Sprintf("%v", v)
	}

	subject := ""
	if v, ok := headers["subject"]; ok {
		subject = fmt.Sprintf("%v", v)
	} else if v, ok := raw["subject"]; ok {
		subject = fmt.Sprintf("%v", v)
	}

	text, _ := raw["text"].(string)
	if text == "" {
		text, _ = raw["body"].(string)
	}
	htmlContent, _ := raw["html"].(string)

	date := ""
	if v, ok := headers["date"]; ok {
		date = fmt.Sprintf("%v", v)
	} else if v, ok := raw["date"]; ok {
		date = fmt.Sprintf("%v", v)
	}

	return map[string]interface{}{
		"id":      id,
		"from":    from,
		"to":      recipientEmail,
		"subject": subject,
		"text":    text,
		"html":    htmlContent,
		"date":    date,
		"isRead":  false,
	}
}

func (p *sioProvider) ensureMailbox(email string) error {
	st := p.getState(email)
	st.mu.Lock()
	defer st.mu.Unlock()

	if st.ws != nil {
		return nil
	}

	atIdx := strings.Index(email, "@")
	if atIdx <= 0 {
		return fmt.Errorf("%s: invalid email address", p.channel)
	}
	local := email[:atIdx]
	host := email[atIdx+1:]
	if host == "" {
		host = p.defaultHost
	}

	ws, err := p.connectSocket(host)
	if err != nil {
		return err
	}
	st.ws = ws

	if err := sioSendEvent(ws, "set shortid", local); err != nil {
		ws.Close()
		st.ws = nil
		return err
	}

	// 启动后台监听 goroutine
	go func() {
		defer func() {
			st.mu.Lock()
			if st.ws == ws {
				st.ws = nil
			}
			st.mu.Unlock()
			ws.Close()
		}()

		for {
			_, msg, err := ws.ReadMessage()
			if err != nil {
				return
			}
			packet := string(msg)
			if packet == "2" {
				ws.WriteMessage(websocket.TextMessage, []byte("3"))
				continue
			}
			event, payload, ok := sioParseEventPacket(packet)
			if !ok || event != "mail" {
				continue
			}
			var rawMail map[string]interface{}
			if err := json.Unmarshal(payload, &rawMail); err != nil {
				continue
			}
			flat := sioFlattenMail(rawMail, email)
			normalized := NormalizeMap(flat, email)
			st.mu.Lock()
			if normalized.ID != "" && !st.seenIDs[normalized.ID] {
				st.seenIDs[normalized.ID] = true
				st.emails = append(st.emails, normalized)
			}
			st.mu.Unlock()
		}
	}()

	time.Sleep(sioInitialSyncWait)
	return nil
}

func (p *sioProvider) GetEmails(email string) ([]NormEmail, error) {
	if err := p.ensureMailbox(email); err != nil {
		return nil, err
	}
	st := p.getState(email)
	st.mu.Lock()
	defer st.mu.Unlock()
	result := make([]NormEmail, len(st.emails))
	copy(result, st.emails)
	return result, nil
}

// ====== 三个渠道实例 ======

var (
	mjjCmProvider     = newSioProvider("mjj-cm", "mjj.cm")
	mailXiuviProvider = newSioProvider("mail-xiuvi", "mail.xiuvi.cn")
	linshiCoProvider  = newSioProvider("linshi-co", "linshi.co")
)

// MjjCmGenerate 创建 mjj-cm 临时邮箱
func MjjCmGenerate() (*CreatedMailbox, error) {
	return mjjCmProvider.Generate()
}

// MjjCmGetEmails 获取 mjj-cm 邮件列表
func MjjCmGetEmails(email string) ([]NormEmail, error) {
	return mjjCmProvider.GetEmails(email)
}

// MailXiuviGenerate 创建 mail-xiuvi 临时邮箱
func MailXiuviGenerate() (*CreatedMailbox, error) {
	return mailXiuviProvider.Generate()
}

// MailXiuviGetEmails 获取 mail-xiuvi 邮件列表
func MailXiuviGetEmails(email string) ([]NormEmail, error) {
	return mailXiuviProvider.GetEmails(email)
}

// LinshiCoGenerate 创建 linshi-co 临时邮箱
func LinshiCoGenerate() (*CreatedMailbox, error) {
	return linshiCoProvider.Generate()
}

// LinshiCoGetEmails 获取 linshi-co 邮件列表
func LinshiCoGetEmails(email string) ([]NormEmail, error) {
	return linshiCoProvider.GetEmails(email)
}
