package tempemail

import (
	"encoding/json"
	"fmt"
	"io"
	"net/url"
)

/**
 * Guerrilla Mail 渠道实现
 * API 文档: https://www.guerrillamail.com/GuerrillaMailAPI.html
 *
 * 特点:
 * - 无需认证，公开 JSON API
 * - 通过 sid_token 维持会话
 * - 邮箱有效期 60 分钟
 */

const guerrillaMailBaseURL = "https://api.guerrillamail.com/ajax.php"

/*
 * guerrillaMailGenerate 创建临时邮箱
 * API: GET ajax.php?f=get_email_address
 */
func guerrillaMailGenerate() (*EmailInfo, error) {
	client := HTTPClient()
	resp, err := client.Get(guerrillaMailBaseURL + "?f=get_email_address&lang=en")
	if err != nil {
		return nil, fmt.Errorf("guerrillamail generate request failed: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != 200 {
		return nil, fmt.Errorf("guerrillamail generate failed: %d", resp.StatusCode)
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("guerrillamail read body failed: %w", err)
	}

	var result struct {
		EmailAddr      string `json:"email_addr"`
		EmailTimestamp int64  `json:"email_timestamp"`
		SidToken       string `json:"sid_token"`
	}

	if err := json.Unmarshal(body, &result); err != nil {
		return nil, fmt.Errorf("guerrillamail parse response failed: %w", err)
	}

	if result.EmailAddr == "" || result.SidToken == "" {
		return nil, fmt.Errorf("guerrillamail generate failed: missing email_addr or sid_token")
	}

	return &EmailInfo{
		Channel:   ChannelGuerrillaMail,
		Email:     result.EmailAddr,
		Token:     result.SidToken,
		ExpiresAt: (result.EmailTimestamp + 3600) * 1000,
	}, nil
}

/*
 * guerrillaMailGetEmails 获取邮件列表
 * API: GET ajax.php?f=check_email&seq=0&sid_token=xxx
 */
func guerrillaMailGetEmails(token string, email string) ([]Email, error) {
	u := guerrillaMailBaseURL + "?f=check_email&seq=0&sid_token=" + url.QueryEscape(token)
	client := HTTPClient()
	resp, err := client.Get(u)
	if err != nil {
		return nil, fmt.Errorf("guerrillamail get emails request failed: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != 200 {
		return nil, fmt.Errorf("guerrillamail get emails failed: %d", resp.StatusCode)
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("guerrillamail read body failed: %w", err)
	}

	var result struct {
		List []json.RawMessage `json:"list"`
	}

	if err := json.Unmarshal(body, &result); err != nil {
		return nil, fmt.Errorf("guerrillamail parse response failed: %w", err)
	}

	if len(result.List) == 0 {
		return []Email{}, nil
	}

	return normalizeRawEmails(result.List, email)
}
