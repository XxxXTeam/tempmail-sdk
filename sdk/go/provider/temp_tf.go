package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

const tempTFBase = "https://temp.tf/api"

/* TempTFGenerate 获取一个一次性地址（服务返回自有域名或别名） */
func TempTFGenerate() (*CreatedMailbox, error) {
	req, err := http.NewRequest("GET", tempTFBase+"/account?plus=1", nil)
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
		return nil, fmt.Errorf("temp-tf: HTTP %d", resp.StatusCode)
	}
	var out struct {
		Email string `json:"email"`
	}
	if err := json.Unmarshal(body, &out); err != nil {
		return nil, err
	}
	if strings.TrimSpace(out.Email) == "" {
		return nil, fmt.Errorf("temp-tf: empty email")
	}
	return &CreatedMailbox{Channel: "temp-tf", Email: out.Email}, nil
}

func tempTFFlatten(raw map[string]any, recipient string) map[string]any {
	contentType := strings.ToLower(strings.TrimSpace(fmt.Sprint(raw["bodyContentType"])))
	body := raw["body"]
	flat := map[string]any{
		"id":          raw["id"],
		"from":        raw["from"],
		"to":          recipient,
		"subject":     raw["subject"],
		"date":        raw["date"],
		"attachments": raw["attachments"],
	}
	if contentType == "text" {
		flat["text"] = body
	} else {
		flat["html"] = body
	}
	return flat
}

/* TempTFGetEmails 列表即含正文；body 按服务端 bodyContentType 归位 */
func TempTFGetEmails(email string) ([]NormEmail, error) {
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("temp-tf: empty email")
	}
	payload := strings.NewReader(fmt.Sprintf(`{"email":%q}`, email))
	req, err := http.NewRequest("POST", tempTFBase+"/check", payload)
	if err != nil {
		return nil, err
	}
	req.Header.Set("Accept", "application/json")
	req.Header.Set("Content-Type", "application/json")
	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("temp-tf: HTTP %d", resp.StatusCode)
	}
	var out struct {
		Data []map[string]any `json:"data"`
	}
	if err := json.Unmarshal(raw, &out); err != nil {
		return nil, err
	}
	result := make([]NormEmail, 0, len(out.Data))
	for _, item := range out.Data {
		result = append(result, NormalizeMap(tempTFFlatten(item, email), email))
	}
	return result, nil
}
