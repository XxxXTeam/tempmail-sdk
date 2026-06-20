package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"net/url"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

const tempmailcBase = "https://tempmailc.com/api/v1"

type tempmailcNewResponse struct {
	OK    bool   `json:"ok"`
	Email string `json:"email"`
}

type tempmailcInboxResponse struct {
	OK       bool             `json:"ok"`
	Email    string           `json:"email"`
	Count    int              `json:"count"`
	Messages []map[string]any `json:"messages"`
}

func tempmailcGetJSON(u string) ([]byte, error) {
	req, err := http.NewRequest("GET", u, nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("Accept", "application/json")
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
		return nil, fmt.Errorf("tempmailc: http %d", resp.StatusCode)
	}
	return body, nil
}

func tempmailcString(v any) string {
	if v == nil {
		return ""
	}
	if s, ok := v.(string); ok {
		return strings.TrimSpace(s)
	}
	return strings.TrimSpace(fmt.Sprint(v))
}

func tempmailcFlattenListMessage(raw map[string]any, recipient string) map[string]any {
	return map[string]any{
		"id":        raw["id"],
		"from":      raw["from"],
		"to":        recipient,
		"subject":   raw["subject"],
		"timestamp": raw["ts"],
		"read":      raw["read"],
	}
}

func TempmailcGenerate() (*CreatedMailbox, error) {
	body, err := tempmailcGetJSON(tempmailcBase + "/new")
	if err != nil {
		return nil, err
	}
	var data tempmailcNewResponse
	if err := json.Unmarshal(body, &data); err != nil {
		return nil, err
	}
	email := strings.TrimSpace(data.Email)
	if !data.OK || email == "" || !strings.Contains(email, "@") {
		return nil, fmt.Errorf("tempmailc: invalid new email response")
	}
	return &CreatedMailbox{Channel: "tempmailc", Email: email}, nil
}

func tempmailcFetchMessage(email, messageID string) (map[string]any, error) {
	u := fmt.Sprintf(
		"%s/message?email=%s&msg_id=%s",
		tempmailcBase,
		url.QueryEscape(email),
		url.QueryEscape(messageID),
	)
	body, err := tempmailcGetJSON(u)
	if err != nil {
		return nil, err
	}
	var raw map[string]any
	if err := json.Unmarshal(body, &raw); err != nil {
		return nil, err
	}
	if ok, exists := raw["ok"].(bool); exists && !ok {
		return nil, fmt.Errorf("tempmailc: message response failed")
	}
	return raw, nil
}

func TempmailcGetEmails(email string) ([]NormEmail, error) {
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("tempmailc: empty email")
	}

	u := fmt.Sprintf("%s/inbox?email=%s", tempmailcBase, url.QueryEscape(email))
	body, err := tempmailcGetJSON(u)
	if err != nil {
		return nil, err
	}
	var data tempmailcInboxResponse
	if err := json.Unmarshal(body, &data); err != nil {
		return nil, err
	}
	if !data.OK {
		return nil, fmt.Errorf("tempmailc: inbox response failed")
	}

	out := make([]NormEmail, 0, len(data.Messages))
	for _, item := range data.Messages {
		id := tempmailcString(item["id"])
		if id != "" {
			detail, err := tempmailcFetchMessage(email, id)
			if err == nil {
				out = append(out, NormalizeMap(detail, email))
				continue
			}
		}
		out = append(out, NormalizeMap(tempmailcFlattenListMessage(item, email), email))
	}
	return out, nil
}
