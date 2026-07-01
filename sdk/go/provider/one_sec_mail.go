package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

const oneSecMailBase = "https://tmaily.com/"

func oneSecMailCookieValue(headers http.Header) string {
	for _, line := range headers.Values("Set-Cookie") {
		first := strings.TrimSpace(strings.Split(line, ";")[0])
		if strings.HasPrefix(first, "TMaily_sid=") {
			return first
		}
	}
	return ""
}

func OneSecMailGenerate() (*CreatedMailbox, error) {
	req, err := http.NewRequest(http.MethodGet, oneSecMailBase+"generate", nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("Accept", "application/json")
	req.Header.Set("User-Agent", "Mozilla/5.0")
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
		return nil, fmt.Errorf("1sec-mail generate: %d", resp.StatusCode)
	}
	cookie := oneSecMailCookieValue(resp.Header)
	if cookie == "" {
		return nil, fmt.Errorf("1sec-mail: 未获取到会话 Cookie")
	}
	var data struct {
		Address string `json:"address"`
	}
	if err := json.Unmarshal(body, &data); err != nil {
		return nil, fmt.Errorf("1sec-mail: 无效的响应格式")
	}
	email := strings.TrimSpace(data.Address)
	if email == "" || !strings.Contains(email, "@") {
		return nil, fmt.Errorf("1sec-mail: 无效的邮箱响应")
	}
	return &CreatedMailbox{
		Channel: "1sec-mail",
		Email:   email,
		Token:   cookie,
	}, nil
}

func OneSecMailGetEmails(token, email string) ([]NormEmail, error) {
	address := strings.TrimSpace(email)
	if address == "" {
		return nil, fmt.Errorf("1sec-mail: 邮箱地址为空")
	}
	req, err := http.NewRequest(http.MethodGet, oneSecMailBase+"emails?address="+address, nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("Accept", "application/json")
	req.Header.Set("Cookie", token)
	req.Header.Set("User-Agent", "Mozilla/5.0")
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
		return nil, fmt.Errorf("1sec-mail emails: %d", resp.StatusCode)
	}
	var rows []map[string]interface{}
	if err := json.Unmarshal(body, &rows); err != nil {
		return nil, fmt.Errorf("1sec-mail: 无效的邮件列表响应")
	}
	out := make([]NormEmail, 0, len(rows))
	for _, row := range rows {
		out = append(out, NormalizeMap(row, address))
	}
	return out, nil
}
