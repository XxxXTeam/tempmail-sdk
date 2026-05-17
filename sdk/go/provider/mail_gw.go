package provider

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"math/rand"
	"strings"
	"sync"

	http "github.com/bogdanfinn/fhttp"
)

/**
 * mail.gw 渠道实现
 * API 与 mail.tm 完全一致，仅 baseURL 不同
 */

const mailGwBaseURL = "https://api.mail.gw"

/* mailGwGetDomains 获取可用域名列表 */
func mailGwGetDomains() ([]string, error) {
	req, err := http.NewRequest("GET", mailGwBaseURL+"/domains", nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("Accept", "application/json")

	client := HTTPClient()
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	/* 兼容两种响应格式：Hydra 格式和纯数组 */
	var items []mailTmDomainItem
	if err := json.Unmarshal(body, &items); err != nil {
		var hydra mailTmDomainsResponse
		if err2 := json.Unmarshal(body, &hydra); err2 != nil {
			return nil, err2
		}
		items = hydra.Members
	}

	var domains []string
	for _, d := range items {
		if d.IsActive {
			domains = append(domains, d.Domain)
		}
	}
	return domains, nil
}

/* MailGwGenerate 创建临时邮箱 */
func MailGwGenerate() (*CreatedMailbox, error) {
	// 1. 获取可用域名
	domains, err := mailGwGetDomains()
	if err != nil {
		return nil, err
	}
	if len(domains) == 0 {
		return nil, fmt.Errorf("mail-gw: no available domains")
	}

	// 2. 生成随机邮箱和密码
	domain := domains[rand.Intn(len(domains))]
	username := randomStr(12)
	address := fmt.Sprintf("%s@%s", username, domain)
	password := randomStr(16)

	// 3. 创建账号
	reqBody, _ := json.Marshal(map[string]string{
		"address":  address,
		"password": password,
	})

	req, err := http.NewRequest("POST", mailGwBaseURL+"/accounts", bytes.NewReader(reqBody))
	if err != nil {
		return nil, err
	}
	req.Header.Set("Content-Type", "application/ld+json")
	req.Header.Set("Accept", "application/json")

	client := HTTPClient()
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	if resp.StatusCode >= 400 {
		return nil, fmt.Errorf("mail-gw create account failed: %d %s", resp.StatusCode, string(body))
	}

	var account mailTmAccountResponse
	if err := json.Unmarshal(body, &account); err != nil {
		return nil, err
	}

	// 4. 获取 Bearer Token
	tokenReqBody, _ := json.Marshal(map[string]string{
		"address":  address,
		"password": password,
	})

	tokenReq, err := http.NewRequest("POST", mailGwBaseURL+"/token", bytes.NewReader(tokenReqBody))
	if err != nil {
		return nil, err
	}
	tokenReq.Header.Set("Content-Type", "application/json")
	tokenReq.Header.Set("Accept", "application/json")

	tokenResp, err := client.Do(tokenReq)
	if err != nil {
		return nil, err
	}
	defer tokenResp.Body.Close()

	tokenBody, err := io.ReadAll(tokenResp.Body)
	if err != nil {
		return nil, err
	}

	if tokenResp.StatusCode >= 400 {
		return nil, fmt.Errorf("mail-gw get token failed: %d", tokenResp.StatusCode)
	}

	var tokenResult mailTmTokenResponse
	if err := json.Unmarshal(tokenBody, &tokenResult); err != nil {
		return nil, err
	}

	info := &CreatedMailbox{Channel: "mail-gw", Email: address, Token: tokenResult.Token}
	info.CreatedAt = account.CreatedAt
	return info, nil
}

/* MailGwGetEmails 获取邮件列表 */
func MailGwGetEmails(token string, email string) ([]NormEmail, error) {
	// 1. 获取邮件列表
	req, err := http.NewRequest("GET", mailGwBaseURL+"/messages", nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("Accept", "application/json")
	req.Header.Set("Authorization", "Bearer "+token)

	client := HTTPClient()
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	if resp.StatusCode >= 400 {
		return nil, fmt.Errorf("mail-gw get messages failed: %d", resp.StatusCode)
	}

	/* 兼容 Hydra 格式和纯数组格式 */
	var msgItems []mailTmMessageItem
	if err := json.Unmarshal(body, &msgItems); err != nil {
		var listResult mailTmMessagesResponse
		if err2 := json.Unmarshal(body, &listResult); err2 != nil {
			return nil, err2
		}
		msgItems = listResult.Members
	}

	if len(msgItems) == 0 {
		return []NormEmail{}, nil
	}

	// 2. 并发获取每封邮件的详情
	type detailResult struct {
		index int
		raw   map[string]interface{}
		err   error
	}

	results := make([]detailResult, len(msgItems))
	var wg sync.WaitGroup

	for i, msg := range msgItems {
		wg.Add(1)
		go func(idx int, msgID string) {
			defer wg.Done()

			detailReq, err := http.NewRequest("GET", mailGwBaseURL+"/messages/"+msgID, nil)
			if err != nil {
				results[idx] = detailResult{index: idx, err: err}
				return
			}
			detailReq.Header.Set("Accept", "application/json")
			detailReq.Header.Set("Authorization", "Bearer "+token)

			detailResp, err := client.Do(detailReq)
			if err != nil {
				results[idx] = detailResult{index: idx, err: err}
				return
			}
			defer detailResp.Body.Close()

			detailBody, err := io.ReadAll(detailResp.Body)
			if err != nil {
				results[idx] = detailResult{index: idx, err: err}
				return
			}

			var m map[string]interface{}
			if err := json.Unmarshal(detailBody, &m); err != nil {
				results[idx] = detailResult{index: idx, err: err}
				return
			}

			results[idx] = detailResult{index: idx, raw: m}
		}(i, msg.ID)
	}

	wg.Wait()

	// 3. 扁平化并标准化
	emails := make([]NormEmail, 0, len(results))
	for _, r := range results {
		if r.err != nil || r.raw == nil {
			continue
		}
		flat := mailGwFlattenMessage(r.raw, email)
		emails = append(emails, NormalizeMap(flat, email))
	}

	return emails, nil
}

/* mailGwFlattenMessage 将 mail.gw 的消息格式扁平化 */
func mailGwFlattenMessage(raw map[string]interface{}, recipientEmail string) map[string]interface{} {
	flat := map[string]interface{}{
		"id":        raw["id"],
		"subject":   raw["subject"],
		"text":      raw["text"],
		"seen":      raw["seen"],
		"createdAt": raw["createdAt"],
	}

	if from, ok := raw["from"].(map[string]interface{}); ok {
		flat["from"] = from["address"]
	}

	if to, ok := raw["to"].([]interface{}); ok && len(to) > 0 {
		if first, ok := to[0].(map[string]interface{}); ok {
			flat["to"] = first["address"]
		}
	}
	if flat["to"] == nil || flat["to"] == "" {
		flat["to"] = recipientEmail
	}

	if htmlArr, ok := raw["html"].([]interface{}); ok {
		parts := make([]string, 0, len(htmlArr))
		for _, h := range htmlArr {
			if s, ok := h.(string); ok {
				parts = append(parts, s)
			}
		}
		flat["html"] = strings.Join(parts, "")
	}

	if attachments, ok := raw["attachments"].([]interface{}); ok {
		for _, a := range attachments {
			if att, ok := a.(map[string]interface{}); ok {
				if dl, ok := att["downloadUrl"].(string); ok && dl != "" {
					att["downloadUrl"] = mailGwBaseURL + dl
				}
			}
		}
		flat["attachments"] = attachments
	}

	return flat
}
