package tempemail

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"math/rand"
	"net/http"
	"strings"
	"sync"
)

const mailTmBaseURL = "https://api.mail.tm"

// randomStr 生成随机字符串
func randomStr(n int) string {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	b := make([]byte, n)
	for i := range b {
		b[i] = chars[rand.Intn(len(chars))]
	}
	return string(b)
}

type mailTmDomainsResponse struct {
	Members []struct {
		Domain   string `json:"domain"`
		IsActive bool   `json:"isActive"`
	} `json:"hydra:member"`
}

type mailTmAccountResponse struct {
	ID        string `json:"id"`
	Address   string `json:"address"`
	CreatedAt string `json:"createdAt"`
}

type mailTmTokenResponse struct {
	Token string `json:"token"`
}

type mailTmMessagesResponse struct {
	Members []struct {
		ID string `json:"id"`
	} `json:"hydra:member"`
}

// mailTmGetDomains 获取可用域名列表
func mailTmGetDomains() ([]string, error) {
	req, err := http.NewRequest("GET", mailTmBaseURL+"/domains", nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("Accept", "application/json")

	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	var result mailTmDomainsResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, err
	}

	var domains []string
	for _, d := range result.Members {
		if d.IsActive {
			domains = append(domains, d.Domain)
		}
	}
	return domains, nil
}

// mailTmCreateAccount 创建账号
func mailTmCreateAccount(address, password string) (*mailTmAccountResponse, error) {
	reqBody, _ := json.Marshal(map[string]string{
		"address":  address,
		"password": password,
	})

	req, err := http.NewRequest("POST", mailTmBaseURL+"/accounts", bytes.NewReader(reqBody))
	if err != nil {
		return nil, err
	}
	req.Header.Set("Content-Type", "application/ld+json")
	req.Header.Set("Accept", "application/json")

	client := &http.Client{}
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
		return nil, fmt.Errorf("failed to create account: %d %s", resp.StatusCode, string(body))
	}

	var result mailTmAccountResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, err
	}
	return &result, nil
}

// mailTmGetToken 获取 Bearer Token
func mailTmGetToken(address, password string) (string, error) {
	reqBody, _ := json.Marshal(map[string]string{
		"address":  address,
		"password": password,
	})

	req, err := http.NewRequest("POST", mailTmBaseURL+"/token", bytes.NewReader(reqBody))
	if err != nil {
		return "", err
	}
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Accept", "application/json")

	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", err
	}

	if resp.StatusCode >= 400 {
		return "", fmt.Errorf("failed to get token: %d", resp.StatusCode)
	}

	var result mailTmTokenResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return "", err
	}
	return result.Token, nil
}

// mailTmGenerate 创建临时邮箱
// 流程: 获取域名 → 生成随机邮箱/密码 → 创建账号 → 获取 Token
func mailTmGenerate() (*EmailInfo, error) {
	// 1. 获取可用域名
	domains, err := mailTmGetDomains()
	if err != nil {
		return nil, err
	}
	if len(domains) == 0 {
		return nil, fmt.Errorf("no available domains")
	}

	// 2. 生成随机邮箱和密码
	domain := domains[rand.Intn(len(domains))]
	username := randomStr(12)
	address := fmt.Sprintf("%s@%s", username, domain)
	password := randomStr(16)

	// 3. 创建账号
	account, err := mailTmCreateAccount(address, password)
	if err != nil {
		return nil, err
	}

	// 4. 获取 Bearer Token
	token, err := mailTmGetToken(address, password)
	if err != nil {
		return nil, err
	}

	return &EmailInfo{
		Channel:   ChannelMailTm,
		Email:     address,
		Token:     token,
		CreatedAt: account.CreatedAt,
	}, nil
}

// mailTmFlattenMessage 将 mail.tm 的消息格式扁平化为 normalizeEmail 可处理的 map
func mailTmFlattenMessage(raw map[string]interface{}, recipientEmail string) map[string]interface{} {
	flat := map[string]interface{}{
		"id":        raw["id"],
		"subject":   raw["subject"],
		"text":      raw["text"],
		"seen":      raw["seen"],
		"createdAt": raw["createdAt"],
	}

	// from: {name, address} → 提取 address
	if from, ok := raw["from"].(map[string]interface{}); ok {
		flat["from"] = from["address"]
	}

	// to: [{name, address}] → 提取第一个 address
	if to, ok := raw["to"].([]interface{}); ok && len(to) > 0 {
		if first, ok := to[0].(map[string]interface{}); ok {
			flat["to"] = first["address"]
		}
	}
	if flat["to"] == nil || flat["to"] == "" {
		flat["to"] = recipientEmail
	}

	// html: string[] → 合并为一个字符串
	if htmlArr, ok := raw["html"].([]interface{}); ok {
		parts := make([]string, 0, len(htmlArr))
		for _, h := range htmlArr {
			if s, ok := h.(string); ok {
				parts = append(parts, s)
			}
		}
		flat["html"] = strings.Join(parts, "")
	}

	// attachments: 添加完整 URL
	if attachments, ok := raw["attachments"].([]interface{}); ok {
		for _, a := range attachments {
			if att, ok := a.(map[string]interface{}); ok {
				if dl, ok := att["downloadUrl"].(string); ok && dl != "" {
					att["downloadUrl"] = mailTmBaseURL + dl
				}
			}
		}
		flat["attachments"] = attachments
	}

	return flat
}

// mailTmGetEmails 获取邮件列表
// 流程: GET /messages 获取列表 → 并发 GET /messages/{id} 获取每封邮件详情
func mailTmGetEmails(token string, email string) ([]Email, error) {
	// 1. 获取邮件列表
	req, err := http.NewRequest("GET", mailTmBaseURL+"/messages", nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("Accept", "application/json")
	req.Header.Set("Authorization", "Bearer "+token)

	client := &http.Client{}
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
		return nil, fmt.Errorf("failed to get messages: %d", resp.StatusCode)
	}

	var listResult mailTmMessagesResponse
	if err := json.Unmarshal(body, &listResult); err != nil {
		return nil, err
	}

	if len(listResult.Members) == 0 {
		return []Email{}, nil
	}

	// 2. 并发获取每封邮件的详情
	type detailResult struct {
		index int
		raw   map[string]interface{}
		err   error
	}

	results := make([]detailResult, len(listResult.Members))
	var wg sync.WaitGroup

	for i, msg := range listResult.Members {
		wg.Add(1)
		go func(idx int, msgID string) {
			defer wg.Done()

			detailReq, err := http.NewRequest("GET", mailTmBaseURL+"/messages/"+msgID, nil)
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
	emails := make([]Email, 0, len(results))
	for _, r := range results {
		if r.err != nil || r.raw == nil {
			continue
		}
		flat := mailTmFlattenMessage(r.raw, email)
		emails = append(emails, normalizeRawEmail(flat, email))
	}

	return emails, nil
}
