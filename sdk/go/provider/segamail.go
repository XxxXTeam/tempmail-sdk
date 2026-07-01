package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

/**
 * Segamail — https://segamail.com
 * POST JSON API，单域名 segamail.com
 * 创建邮箱: POST https://segamail.com/en/getEmailAddress → {"id":"...","address":"...","creation_time":"...","recover_key":"..."}
 * 获取邮件: POST https://segamail.com/en/getInbox（需要 session cookie）→ [{"from":"...","date":"...","subject":"...","body":"..."}]
 * Token 使用 recover_key
 */

const segamailBase = "https://segamail.com"

/* segamailDefaultHeaders 设置通用请求头 */
func segamailDefaultHeaders(req *http.Request) {
	req.Header.Set("Accept", "application/json, text/plain, */*")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
	req.Header.Set("Cache-Control", "no-cache")
	req.Header.Set("DNT", "1")
	req.Header.Set("Pragma", "no-cache")
	req.Header.Set("Referer", segamailBase+"/en")
	req.Header.Set("User-Agent", getCurrentUA())
}

/*
 * SegamailGenerate 创建 segamail.com 临时邮箱
 * POST https://segamail.com/en/getEmailAddress
 * 响应: {"id":"591815","address":"xxx@segamail.com","creation_time":"...","recover_key":"LSJFEYQ591815"}
 * 使用 recover_key 作为 token
 */
func SegamailGenerate(channel ...string) (*CreatedMailbox, error) {
	req, err := http.NewRequest("POST", segamailBase+"/en/getEmailAddress", nil)
	if err != nil {
		return nil, err
	}
	segamailDefaultHeaders(req)

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
		return nil, fmt.Errorf("segamail generate: %d", resp.StatusCode)
	}

	var result struct {
		ID           string `json:"id"`
		Address      string `json:"address"`
		CreationTime string `json:"creation_time"`
		RecoverKey   string `json:"recover_key"`
	}
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, fmt.Errorf("segamail: parse response error: %w", err)
	}

	address := strings.TrimSpace(result.Address)
	if address == "" {
		return nil, fmt.Errorf("segamail: empty address in response")
	}

	ch := "segamail"
	if len(channel) > 0 && channel[0] != "" {
		ch = channel[0]
	}
	return &CreatedMailbox{
		Channel:   ch,
		Email:     address,
		Token:     result.RecoverKey,
		CreatedAt: result.CreationTime,
	}, nil
}

/*
 * SegamailGetEmails 获取 segamail.com 邮件列表
 * POST https://segamail.com/en/getInbox（依赖 session cookie，由 SDK HTTP 客户端自动维护）
 * 响应: [{"from":"...","date":"...","subject":"...","body":"..."}]
 */
func SegamailGetEmails(email string) ([]NormEmail, error) {
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("segamail: empty email")
	}

	req, err := http.NewRequest("POST", segamailBase+"/en/getInbox", nil)
	if err != nil {
		return nil, err
	}
	segamailDefaultHeaders(req)

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
		return nil, fmt.Errorf("segamail inbox: %d", resp.StatusCode)
	}

	var rawMails []map[string]interface{}
	if err := json.Unmarshal(body, &rawMails); err != nil {
		return nil, fmt.Errorf("segamail: parse inbox error: %w", err)
	}

	/* 将 "body" 字段映射为标准化的 "html" 字段 */
	for _, m := range rawMails {
		if htmlBody, ok := m["body"].(string); ok {
			if _, hasHTML := m["html"]; !hasHTML {
				m["html"] = htmlBody
			}
		}
	}

	return normEmailsFromMaps(rawMails, email), nil
}
