package tempemail

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"math/rand"
	"net/http"
	"sync"
)

/*
 * mail.gw 渠道实现
 * API 与 mail.tm 完全一致，仅 baseURL 不同
 *
 * @功能 创建临时邮箱并获取邮件
 * @网站 mail.gw
 */

const mailGwBaseURL = "https://api.mail.gw"

/*
 * mailGwGetDomains 获取可用域名列表
 * API: GET /domains
 */
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

/*
 * mailGwCreateAccount 创建账号
 * API: POST /accounts
 */
func mailGwCreateAccount(address, password string) (*mailTmAccountResponse, error) {
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
		return nil, fmt.Errorf("failed to create account: %d %s", resp.StatusCode, string(body))
	}

	var result mailTmAccountResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, err
	}
	return &result, nil
}

/*
 * mailGwGetToken 获取 Bearer Token
 * API: POST /token
 */
func mailGwGetToken(address, password string) (string, error) {
	reqBody, _ := json.Marshal(map[string]string{
		"address":  address,
		"password": password,
	})

	req, err := http.NewRequest("POST", mailGwBaseURL+"/token", bytes.NewReader(reqBody))
	if err != nil {
		return "", err
	}
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Accept", "application/json")

	client := HTTPClient()
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

/*
 * mailGwGenerate 创建临时邮箱
 * @功能 获取域名 → 生成随机邮箱/密码 → 创建账号 → 获取 Token
 */
func mailGwGenerate() (*EmailInfo, error) {
	domains, err := mailGwGetDomains()
	if err != nil {
		return nil, err
	}
	if len(domains) == 0 {
		return nil, fmt.Errorf("no available domains")
	}

	domain := domains[rand.Intn(len(domains))]
	username := randomStr(12)
	address := fmt.Sprintf("%s@%s", username, domain)
	password := randomStr(16)

	account, err := mailGwCreateAccount(address, password)
	if err != nil {
		return nil, err
	}

	token, err := mailGwGetToken(address, password)
	if err != nil {
		return nil, err
	}

	return &EmailInfo{
		Channel:   ChannelMailGw,
		Email:     address,
		token:     token,
		CreatedAt: account.CreatedAt,
	}, nil
}

/*
 * mailGwGetEmails 获取邮件列表
 * @功能 GET /messages → 并发获取每封邮件详情
 */
func mailGwGetEmails(token string, email string) ([]Email, error) {
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
		return nil, fmt.Errorf("failed to get messages: %d", resp.StatusCode)
	}

	var msgItems []mailTmMessageItem
	if err := json.Unmarshal(body, &msgItems); err != nil {
		var listResult mailTmMessagesResponse
		if err2 := json.Unmarshal(body, &listResult); err2 != nil {
			return nil, err2
		}
		msgItems = listResult.Members
	}

	if len(msgItems) == 0 {
		return []Email{}, nil
	}

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
