package provider

import (
	"encoding/json"
	"fmt"
	"net/http"
	"net/url"
	"strings"
	"sync"
	"time"

	"github.com/gorilla/websocket"
)

const tempmailCNDefaultHost = "tempmail.cn"

var tempmailCNVersions = []int{4, 3}

var tempmailCNRegMu sync.Mutex
var tempmailCNBoxes = make(map[string]*tempmailCNBox)

type tempmailCNBox struct {
	mu      sync.Mutex
	emails  []NormEmail
	seenIDs map[string]struct{}
	started bool
}

func tempmailCNGetBox(email string) *tempmailCNBox {
	key := strings.ToLower(strings.TrimSpace(email))
	tempmailCNRegMu.Lock()
	defer tempmailCNRegMu.Unlock()
	b := tempmailCNBoxes[key]
	if b == nil {
		b = &tempmailCNBox{seenIDs: make(map[string]struct{})}
		tempmailCNBoxes[key] = b
	}
	return b
}

func tempmailCNNormalizeHost(domain *string) string {
	if domain == nil || strings.TrimSpace(*domain) == "" {
		return tempmailCNDefaultHost
	}
	raw := strings.TrimSpace(*domain)
	host := raw
	if strings.HasPrefix(strings.ToLower(host), "http://") || strings.HasPrefix(strings.ToLower(host), "https://") {
		if u, err := url.Parse(host); err == nil && u.Host != "" {
			host = u.Host
		}
	} else if strings.Contains(host, "/") {
		if u, err := url.Parse("https://" + host); err == nil && u.Host != "" {
			host = u.Host
		} else {
			host = strings.Split(host, "/")[0]
		}
	}
	host = strings.Trim(host, ".")
	if at := strings.LastIndex(host, "@"); at >= 0 {
		host = host[at+1:]
	}
	if host == "" {
		return tempmailCNDefaultHost
	}
	return host
}

func tempmailCNParseAddress(email string) (local string, host string, err error) {
	trimmed := strings.TrimSpace(email)
	at := strings.Index(trimmed, "@")
	if at <= 0 || at >= len(trimmed)-1 {
		return "", "", fmt.Errorf("tempmail-cn: invalid email address")
	}
	hostPart := trimmed[at+1:]
	return trimmed[:at], tempmailCNNormalizeHost(&hostPart), nil
}

func tempmailCNEventPacket(event string, payload interface{}) ([]byte, error) {
	b, err := json.Marshal([]interface{}{event, payload})
	if err != nil {
		return nil, err
	}
	return append([]byte("42"), b...), nil
}

func tempmailCNWriteEvent(conn *websocket.Conn, event string, payload interface{}) error {
	b, err := tempmailCNEventPacket(event, payload)
	if err != nil {
		return err
	}
	return conn.WriteMessage(websocket.TextMessage, b)
}

func tempmailCNParseEvent(packet string) (string, interface{}, bool) {
	if !strings.HasPrefix(packet, "42") {
		return "", nil, false
	}
	var arr []json.RawMessage
	if err := json.Unmarshal([]byte(packet[2:]), &arr); err != nil || len(arr) == 0 {
		return "", nil, false
	}
	var event string
	if err := json.Unmarshal(arr[0], &event); err != nil {
		return "", nil, false
	}
	if len(arr) < 2 {
		return event, nil, true
	}
	var payload interface{}
	if err := json.Unmarshal(arr[1], &payload); err != nil {
		return "", nil, false
	}
	return event, payload, true
}

func tempmailCNDial(host string) (*websocket.Conn, error) {
	var lastErr error
	for _, version := range tempmailCNVersions {
		u := url.URL{Scheme: "wss", Host: host, Path: "/socket.io/"}
		q := u.Query()
		q.Set("EIO", fmt.Sprintf("%d", version))
		q.Set("transport", "websocket")
		u.RawQuery = q.Encode()

		hdr := http.Header{}
		hdr.Set("Origin", "https://"+host)
		hdr.Set("Referer", "https://"+host+"/")
		hdr.Set("User-Agent", GetCurrentUA())
		hdr.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6")

		d := websocket.Dialer{HandshakeTimeout: 15 * time.Second}
		conn, _, err := d.Dial(u.String(), hdr)
		if err != nil {
			lastErr = err
			continue
		}
		if err := tempmailCNHandshake(conn); err != nil {
			conn.Close()
			lastErr = err
			continue
		}
		return conn, nil
	}
	if lastErr == nil {
		lastErr = fmt.Errorf("tempmail-cn: websocket handshake failed")
	}
	return nil, lastErr
}

func tempmailCNHandshake(conn *websocket.Conn) error {
	if err := conn.SetReadDeadline(time.Now().Add(15 * time.Second)); err != nil {
		return err
	}
	_, msg, err := conn.ReadMessage()
	if err != nil {
		return err
	}
	if len(msg) == 0 || msg[0] != '0' {
		return fmt.Errorf("tempmail-cn: unexpected open packet: %q", string(msg))
	}
	if err := conn.WriteMessage(websocket.TextMessage, []byte("40")); err != nil {
		return err
	}
	if err := conn.SetReadDeadline(time.Now().Add(1 * time.Second)); err != nil {
		return err
	}
	for {
		_, msg, err = conn.ReadMessage()
		if err != nil {
			if ne, ok := err.(interface{ Timeout() bool }); ok && ne.Timeout() {
				return conn.SetReadDeadline(time.Time{})
			}
			return err
		}
		packet := string(msg)
		if packet == "2" {
			if err := conn.WriteMessage(websocket.TextMessage, []byte("3")); err != nil {
				return err
			}
			continue
		}
		if strings.HasPrefix(packet, "40") {
			return conn.SetReadDeadline(time.Time{})
		}
	}
}

func tempmailCNString(v interface{}) string {
	if v == nil {
		return ""
	}
	s, ok := v.(string)
	if ok {
		return strings.TrimSpace(s)
	}
	return strings.TrimSpace(fmt.Sprintf("%v", v))
}

func tempmailCNStableID(raw map[string]interface{}, headers map[string]interface{}, recipient string) string {
	if id := tempmailCNString(raw["id"]); id != "" {
		return id
	}
	if id := tempmailCNString(raw["messageId"]); id != "" {
		return id
	}
	if id := tempmailCNString(headers["message-id"]); id != "" {
		return id
	}
	if id := tempmailCNString(headers["messageId"]); id != "" {
		return id
	}
	return fmt.Sprintf("%s\n%s\n%s\n%s",
		tempmailCNString(headers["from"]),
		tempmailCNString(headers["subject"]),
		tempmailCNString(headers["date"]),
		recipient,
	)
}

func tempmailCNFlatten(raw map[string]interface{}, recipient string) map[string]interface{} {
	headers, _ := raw["headers"].(map[string]interface{})
	if headers == nil {
		headers = map[string]interface{}{}
	}
	return map[string]interface{}{
		"id":          tempmailCNStableID(raw, headers, recipient),
		"from":        tempmailCNString(headers["from"]),
		"to":          recipient,
		"subject":     tempmailCNString(headers["subject"]),
		"text":        tempmailCNString(raw["text"]),
		"html":        tempmailCNString(raw["html"]),
		"date":        tempmailCNString(headers["date"]),
		"isRead":      false,
		"attachments": raw["attachments"],
	}
}

func tempmailCNRequestShortID(host string) (string, error) {
	conn, err := tempmailCNDial(host)
	if err != nil {
		return "", err
	}
	defer conn.Close()

	if err := tempmailCNWriteEvent(conn, "request shortid", true); err != nil {
		return "", err
	}
	if err := conn.SetReadDeadline(time.Now().Add(15 * time.Second)); err != nil {
		return "", err
	}
	defer conn.SetReadDeadline(time.Time{})

	for {
		_, msg, err := conn.ReadMessage()
		if err != nil {
			return "", err
		}
		packet := string(msg)
		if packet == "2" {
			if err := conn.WriteMessage(websocket.TextMessage, []byte("3")); err != nil {
				return "", err
			}
			continue
		}
		event, payload, ok := tempmailCNParseEvent(packet)
		if !ok || event != "shortid" {
			continue
		}
		id, ok := payload.(string)
		if ok && strings.TrimSpace(id) != "" {
			return id, nil
		}
	}
}

func TempmailCNGenerate(domain *string) (*CreatedMailbox, error) {
	host := tempmailCNNormalizeHost(domain)
	shortID, err := tempmailCNRequestShortID(host)
	if err != nil {
		return nil, err
	}
	return &CreatedMailbox{
		Channel: "tempmail-cn",
		Email:   shortID + "@" + host,
	}, nil
}

func tempmailCNReadLoop(email, local, host string, box *tempmailCNBox) {
	defer func() {
		box.mu.Lock()
		box.started = false
		box.mu.Unlock()
	}()

	conn, err := tempmailCNDial(host)
	if err != nil {
		return
	}
	defer conn.Close()

	if err := tempmailCNWriteEvent(conn, "set shortid", local); err != nil {
		return
	}

	for {
		_, msg, err := conn.ReadMessage()
		if err != nil {
			return
		}
		packet := string(msg)
		if packet == "2" {
			if err := conn.WriteMessage(websocket.TextMessage, []byte("3")); err != nil {
				return
			}
			continue
		}
		event, payload, ok := tempmailCNParseEvent(packet)
		if !ok || event != "mail" {
			continue
		}
		raw, ok := payload.(map[string]interface{})
		if !ok {
			continue
		}
		em := NormalizeMap(tempmailCNFlatten(raw, email), email)
		if strings.TrimSpace(em.ID) == "" {
			continue
		}
		box.mu.Lock()
		if _, dup := box.seenIDs[em.ID]; !dup {
			box.seenIDs[em.ID] = struct{}{}
			box.emails = append(box.emails, em)
		}
		box.mu.Unlock()
	}
}

func TempmailCNGetEmails(email string) ([]NormEmail, error) {
	local, host, err := tempmailCNParseAddress(email)
	if err != nil {
		return nil, err
	}
	box := tempmailCNGetBox(email)

	box.mu.Lock()
	needStart := !box.started
	if needStart {
		box.started = true
	}
	box.mu.Unlock()

	if needStart {
		go tempmailCNReadLoop(email, local, host, box)
		time.Sleep(80 * time.Millisecond)
	}

	box.mu.Lock()
	defer box.mu.Unlock()
	out := make([]NormEmail, len(box.emails))
	copy(out, box.emails)
	return out, nil
}
