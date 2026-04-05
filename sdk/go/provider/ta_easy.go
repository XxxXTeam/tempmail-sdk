package provider

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"

	http "github.com/bogdanfinn/fhttp"
)

const taEasyAPIBase = "https://api-endpoint.ta-easy.com"
const taEasyOrigin = "https://www.ta-easy.com"

func setTaEasyHeaders(req *http.Request) {
	req.Header.Set("Accept", "application/json")
	req.Header.Set("User-Agent", GetCurrentUA())
	req.Header.Set("Origin", taEasyOrigin)
	req.Header.Set("Referer", taEasyOrigin+"/")
}

// TaEasyGenerate 创建临时邮箱
// API: POST /temp-email/address/new（空 body）
func TaEasyGenerate() (*CreatedMailbox, error) {
	req, err := http.NewRequest("POST", taEasyAPIBase+"/temp-email/address/new", nil)
	if err != nil {
		return nil, err
	}
	setTaEasyHeaders(req)
	req.Header.Set("Content-Length", "0")

	client := HTTPClient()
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if err := CheckHTTPStatus(resp, "ta-easy generate"); err != nil {
		return nil, err
	}
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	var out struct {
		Status    string `json:"status"`
		Message   string `json:"message"`
		Address   string `json:"address"`
		Token     string `json:"token"`
		ExpiresAt int64  `json:"expiresAt"`
	}
	if err := json.Unmarshal(body, &out); err != nil {
		return nil, err
	}
	if out.Status != "success" || out.Address == "" || out.Token == "" {
		if out.Message != "" {
			return nil, fmt.Errorf("ta-easy: %s", out.Message)
		}
		return nil, fmt.Errorf("ta-easy: create failed")
	}
	return &CreatedMailbox{
		Channel:   "ta-easy",
		Email:     out.Address,
		Token:     out.Token,
		ExpiresAt: out.ExpiresAt,
	}, nil
}

// TaEasyGetEmails 拉取收件箱
// API: POST /temp-email/inbox/list，JSON body: { "token", "email" }
func TaEasyGetEmails(email, token string) ([]NormEmail, error) {
	payload, err := json.Marshal(map[string]string{
		"token": token,
		"email": email,
	})
	if err != nil {
		return nil, err
	}
	req, err := http.NewRequest("POST", taEasyAPIBase+"/temp-email/inbox/list", bytes.NewReader(payload))
	if err != nil {
		return nil, err
	}
	setTaEasyHeaders(req)
	req.Header.Set("Content-Type", "application/json")

	client := HTTPClient()
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if err := CheckHTTPStatus(resp, "ta-easy inbox"); err != nil {
		return nil, err
	}
	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	var wrap struct {
		Status  string            `json:"status"`
		Message string            `json:"message"`
		Data    []json.RawMessage `json:"data"`
	}
	if err := json.Unmarshal(raw, &wrap); err != nil {
		return nil, err
	}
	if wrap.Status != "success" {
		if wrap.Message != "" {
			return nil, fmt.Errorf("ta-easy: %s", wrap.Message)
		}
		return nil, fmt.Errorf("ta-easy: inbox failed")
	}
	out := make([]NormEmail, 0, len(wrap.Data))
	for _, rm := range wrap.Data {
		var m map[string]interface{}
		if err := json.Unmarshal(rm, &m); err != nil {
			continue
		}
		out = append(out, NormalizeMap(m, email))
	}
	return out, nil
}
