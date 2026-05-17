package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"net/url"
)

/**
 * Guerrillamail 镜像渠道实现
 * sharklasers.com / grr.la / guerrillamail.info 共用同一套 API，仅 baseURL 不同
 */

/*
 * GuerrillamailMirrorGenerate 创建临时邮箱（镜像渠道）
 * API: GET <baseURL>?f=get_email_address
 */
func GuerrillamailMirrorGenerate(channel string, baseURL string) (*CreatedMailbox, error) {
	client := HTTPClient()
	resp, err := client.Get(baseURL + "?f=get_email_address&lang=en")
	if err != nil {
		return nil, fmt.Errorf("%s generate request failed: %w", channel, err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != 200 {
		return nil, fmt.Errorf("%s generate failed: %d", channel, resp.StatusCode)
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("%s read body failed: %w", channel, err)
	}

	var result struct {
		EmailAddr      string `json:"email_addr"`
		EmailTimestamp int64  `json:"email_timestamp"`
		SidToken       string `json:"sid_token"`
	}

	if err := json.Unmarshal(body, &result); err != nil {
		return nil, fmt.Errorf("%s parse response failed: %w", channel, err)
	}

	if result.EmailAddr == "" || result.SidToken == "" {
		return nil, fmt.Errorf("%s generate failed: missing email_addr or sid_token", channel)
	}

	info := &CreatedMailbox{Channel: channel, Email: result.EmailAddr, Token: result.SidToken}
	info.ExpiresAt = (result.EmailTimestamp + 3600) * 1000
	return info, nil
}

/*
 * GuerrillamailMirrorGetEmails 获取邮件列表（镜像渠道）
 * API: GET <baseURL>?f=check_email&seq=0&sid_token=xxx
 */
func GuerrillamailMirrorGetEmails(baseURL string, token string, email string) ([]NormEmail, error) {
	u := baseURL + "?f=check_email&seq=0&sid_token=" + url.QueryEscape(token)
	client := HTTPClient()
	resp, err := client.Get(u)
	if err != nil {
		return nil, fmt.Errorf("guerrillamail mirror get emails request failed: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != 200 {
		return nil, fmt.Errorf("guerrillamail mirror get emails failed: %d", resp.StatusCode)
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("guerrillamail mirror read body failed: %w", err)
	}

	var result struct {
		List []json.RawMessage `json:"list"`
	}

	if err := json.Unmarshal(body, &result); err != nil {
		return nil, fmt.Errorf("guerrillamail mirror parse response failed: %w", err)
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
			if v, ok := item["mail_id"].(float64); ok {
				mailID = fmt.Sprintf("%.0f", v)
			}
		}

		// check_email 只返回摘要，需要调用 fetch_email 获取完整 HTML 正文
		if mailBody == "" && mailID != "" {
			fetchURL := baseURL + "?f=fetch_email&sid_token=" + url.QueryEscape(token) + "&email_id=" + url.QueryEscape(mailID)
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
