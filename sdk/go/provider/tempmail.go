package provider

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/url"

	http "github.com/bogdanfinn/fhttp"
)

const tempmailBaseURL = "https://api.tempmail.ing/api"

type TempmailGenerateRequest struct {
	Duration int `json:"duration"`
}

type TempmailGenerateResponse struct {
	Email struct {
		Address         string `json:"address"`
		ExpiresAt       string `json:"expiresAt"`
		CreatedAt       string `json:"createdAt"`
		DurationMinutes int    `json:"durationMinutes"`
	} `json:"email"`
	Success bool `json:"success"`
}

type tempmailEmailsResponse struct {
	Emails  []json.RawMessage `json:"emails"`
	Success bool              `json:"success"`
}

func TempmailGenerate(duration int) (*CreatedMailbox, error) {
	if duration <= 0 {
		duration = 30
	}

	reqBody, _ := json.Marshal(TempmailGenerateRequest{Duration: duration})

	req, err := http.NewRequest("POST", tempmailBaseURL+"/generate", bytes.NewReader(reqBody))
	if err != nil {
		return nil, err
	}

	req.Header.Set("User-Agent", GetCurrentUA())
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Referer", "https://tempmail.ing/")
	req.Header.Set("DNT", "1")

	client := HTTPClient()
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	if err := CheckHTTPStatus(resp, "tempmail generate"); err != nil {
		return nil, err
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	var result TempmailGenerateResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, err
	}

	if !result.Success {
		return nil, fmt.Errorf("failed to generate email")
	}

	info := &CreatedMailbox{Channel: "tempmail", Email: result.Email.Address, Token: ""}
	info.ExpiresAt = result.Email.ExpiresAt
	info.CreatedAt = result.Email.CreatedAt
	return info, nil
}

func TempmailGetEmails(email string) ([]NormEmail, error) {
	encodedEmail := url.QueryEscape(email)

	req, err := http.NewRequest("GET", tempmailBaseURL+"/emails/"+encodedEmail, nil)
	if err != nil {
		return nil, err
	}

	req.Header.Set("User-Agent", GetCurrentUA())
	req.Header.Set("Referer", "https://tempmail.ing/")
	req.Header.Set("DNT", "1")

	client := HTTPClient()
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	if err := CheckHTTPStatus(resp, "tempmail get emails"); err != nil {
		return nil, err
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	var result tempmailEmailsResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, err
	}

	if !result.Success {
		return nil, fmt.Errorf("failed to get emails")
	}

	/*
	 * tempmail.ing API 返回格式特殊：
	 *   from_address → from
	 *   content → html（是 HTML 内容）
	 *   text → text（通常为空）
	 *   received_at → date
	 *   is_read → 0/1
	 * 需要先扁平化再 normalize
	 */
	emails := make([]NormEmail, 0, len(result.Emails))
	for _, raw := range result.Emails {
		var m map[string]interface{}
		if err := json.Unmarshal(raw, &m); err != nil {
			continue
		}
		flat := tempmailFlattenMessage(m, email)
		emails = append(emails, NormalizeMap(flat, email))
	}
	return emails, nil
}

/*
 * tempmailFlattenMessage 将 tempmail.ing 的原始格式扁平化
 * content 映射到 html，from_address 映射到 from
 */
func tempmailFlattenMessage(raw map[string]interface{}, recipientEmail string) map[string]interface{} {
	return map[string]interface{}{
		"id":      raw["id"],
		"from":    raw["from_address"],
		"to":      recipientEmail,
		"subject": raw["subject"],
		"text":    raw["text"],
		"html":    raw["content"],
		"date":    raw["received_at"],
		"is_read": raw["is_read"],
	}
}
