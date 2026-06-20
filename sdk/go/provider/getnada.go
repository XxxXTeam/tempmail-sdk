package provider

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"math/rand"
	"net/url"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

const getnadaBase = "https://getnada.net/api"

type getnadaDomainsResponse struct {
	Domains []string `json:"domains"`
}

type getnadaOpenResponse struct {
	InboxID     string `json:"inboxId"`
	Token       string `json:"token"`
	ActiveUntil string `json:"activeUntil"`
	Recipient   string `json:"recipient"`
}

type getnadaMessagesResponse struct {
	Messages []map[string]any `json:"messages"`
}

type getnadaDetailResponse struct {
	Message map[string]any `json:"message"`
}

func getnadaRandomLocal() string {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	var b strings.Builder
	b.WriteString("sdk")
	for i := 0; i < 16; i++ {
		b.WriteByte(chars[rand.Intn(len(chars))])
	}
	return b.String()
}

func getnadaJSON(method, u string, body []byte, out any) error {
	req, err := http.NewRequest(method, u, bytes.NewReader(body))
	if err != nil {
		return err
	}
	req.Header.Set("Accept", "application/json")
	if body != nil {
		req.Header.Set("Content-Type", "application/json")
	}
	resp, err := HTTPClient().Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return err
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return fmt.Errorf("getnada: http %d", resp.StatusCode)
	}
	if out == nil {
		return nil
	}
	return json.Unmarshal(raw, out)
}

func getnadaPickDomain() (string, error) {
	var data getnadaDomainsResponse
	if err := getnadaJSON(http.MethodGet, getnadaBase+"/public/domains", nil, &data); err != nil {
		return "", err
	}
	for _, domain := range data.Domains {
		if strings.TrimSpace(domain) == "getnada.net" {
			return "getnada.net", nil
		}
	}
	for _, domain := range data.Domains {
		if d := strings.TrimSpace(domain); d != "" {
			return d, nil
		}
	}
	return "", fmt.Errorf("getnada: no domain available")
}

func getnadaString(v any) string {
	switch x := v.(type) {
	case string:
		return x
	case nil:
		return ""
	default:
		return fmt.Sprintf("%v", x)
	}
}

func getnadaFlattenMessage(raw map[string]any, recipient string) map[string]any {
	out := make(map[string]any, len(raw)+6)
	for k, v := range raw {
		out[k] = v
	}
	out["id"] = raw["id"]
	if v, ok := raw["from_addr"]; ok {
		out["from"] = v
	} else if v, ok := raw["from"]; ok {
		out["from"] = v
	}
	if v, ok := raw["to"]; ok && getnadaString(v) != "" {
		out["to"] = v
	} else {
		out["to"] = recipient
	}
	if v, ok := raw["text_plain"]; ok {
		out["text"] = v
	} else if v, ok := raw["text"]; ok {
		out["text"] = v
	}
	if v, ok := raw["html_sanitized"]; ok {
		out["html"] = v
	} else if v, ok := raw["html"]; ok {
		out["html"] = v
	}
	if v, ok := raw["read_at"]; ok {
		out["read"] = getnadaString(v) != ""
	}
	return out
}

func GetnadaGenerate() (*CreatedMailbox, error) {
	domain, err := getnadaPickDomain()
	if err != nil {
		return nil, err
	}
	requested := getnadaRandomLocal() + "@" + domain
	body, err := json.Marshal(map[string]string{"email": requested})
	if err != nil {
		return nil, err
	}
	var data getnadaOpenResponse
	if err := getnadaJSON(http.MethodPost, getnadaBase+"/inbox/open", body, &data); err != nil {
		return nil, err
	}
	token := strings.TrimSpace(data.Token)
	email := strings.TrimSpace(data.Recipient)
	if email == "" {
		email = requested
	}
	if token == "" || email == "" || !strings.Contains(email, "@") {
		return nil, fmt.Errorf("getnada: invalid open response")
	}
	return &CreatedMailbox{
		Channel:   "getnada",
		Email:     email,
		Token:     token,
		ExpiresAt: data.ActiveUntil,
	}, nil
}

func getnadaFetchDetail(token, messageID string) (map[string]any, error) {
	q := url.Values{}
	q.Set("id", messageID)
	q.Set("token", token)
	var data getnadaDetailResponse
	if err := getnadaJSON(http.MethodGet, getnadaBase+"/inbox/message?"+q.Encode(), nil, &data); err != nil {
		return nil, err
	}
	if data.Message == nil {
		return nil, fmt.Errorf("getnada: empty message detail")
	}
	return data.Message, nil
}

func GetnadaGetEmails(token, email string) ([]NormEmail, error) {
	auth := strings.TrimSpace(token)
	address := strings.TrimSpace(email)
	if auth == "" {
		return nil, fmt.Errorf("getnada: empty token")
	}
	if address == "" {
		return nil, fmt.Errorf("getnada: empty email")
	}

	q := url.Values{}
	q.Set("token", auth)
	var data getnadaMessagesResponse
	if err := getnadaJSON(http.MethodGet, getnadaBase+"/inbox/messages?"+q.Encode(), nil, &data); err != nil {
		return nil, err
	}
	out := make([]NormEmail, 0, len(data.Messages))
	for _, row := range data.Messages {
		raw := row
		if id := strings.TrimSpace(getnadaString(row["id"])); id != "" {
			if detail, err := getnadaFetchDetail(auth, id); err == nil {
				raw = detail
			}
		}
		out = append(out, NormalizeMap(getnadaFlattenMessage(raw, address), address))
	}
	return out, nil
}
