package provider

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
 * GuerrillaMailGenerate 创建临时邮箱
 * API: GET ajax.php?f=get_email_address
 */
func GuerrillaMailGenerate() (*CreatedMailbox, error) {
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

	info := &CreatedMailbox{Channel: "guerrillamail", Email: result.EmailAddr, Token: result.SidToken}
	info.ExpiresAt = (result.EmailTimestamp + 3600) * 1000
	return info, nil
}

/*
 * GuerrillaMailGetEmails 获取邮件列表
 * API: GET ajax.php?f=check_email&seq=0&sid_token=xxx
 * 对 mail_body 为空的邮件，调用 fetch_email 获取完整正文
 */
func GuerrillaMailGetEmails(token string, email string) ([]NormEmail, error) {
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
		return []NormEmail{}, nil
	}

	out := make([]NormEmail, 0, len(result.List))
	for _, raw := range result.List {
		var item map[string]interface{}
		if err := json.Unmarshal(raw, &item); err != nil {
			continue
		}

		mailBody, _ := item["mail_body"].(string)
		mailID, _ := item["mail_id"].(string)
		if mailID == "" {
			// mail_id 也可能是数字
			if v, ok := item["mail_id"].(float64); ok {
				mailID = fmt.Sprintf("%.0f", v)
			}
		}

		// check_email 只返回摘要，需要调用 fetch_email 获取完整 HTML 正文
		if mailBody == "" && mailID != "" {
			fetchURL := guerrillaMailBaseURL + "?f=fetch_email&sid_token=" + url.QueryEscape(token) + "&email_id=" + url.QueryEscape(mailID)
			if fetchResp, err := client.Get(fetchURL); err == nil {
				if fetchResp.StatusCode == 200 {
					if fetchBody, err := io.ReadAll(fetchResp.Body); err == nil {
						var detail map[string]interface{}
						if json.Unmarshal(fetchBody, &detail) == nil {
							if detailBody, ok := detail["mail_body"].(string); ok && detailBody != "" {
								item["mail_body"] = detailBody
							}
						}
					}
				}
				fetchResp.Body.Close()
			}
		}

		out = append(out, NormalizeMap(item, email))
	}
	return out, nil
}
