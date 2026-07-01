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

const duckmailBaseURL = "https://api.duckmail.sbs"

type duckmailDomainItem struct {
	Domain   string `json:"domain"`
	IsActive bool   `json:"isActive"`
}

type duckmailDomainsResponse struct {
	Members []duckmailDomainItem `json:"hydra:member"`
}

type duckmailAccountResponse struct {
	ID        string `json:"id"`
	Address   string `json:"address"`
	CreatedAt string `json:"createdAt"`
}

type duckmailTokenResponse struct {
	Token string `json:"token"`
}

type duckmailMessageItem struct {
	ID string `json:"id"`
}

type duckmailMessagesResponse struct {
	Members []duckmailMessageItem `json:"hydra:member"`
}

func duckmailGetDomains() ([]string, error) {
	req, err := http.NewRequest("GET", duckmailBaseURL+"/domains?page=1", nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("Accept", "application/json")
	req.Header.Set("User-Agent", getCurrentUA())

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

	var items []duckmailDomainItem
	if err := json.Unmarshal(body, &items); err != nil {
		var hydra duckmailDomainsResponse
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

func duckmailCreateAccount(address, password string) (*duckmailAccountResponse, error) {
	reqBody, _ := json.Marshal(map[string]string{
		"address":  address,
		"password": password,
	})

	req, err := http.NewRequest("POST", duckmailBaseURL+"/accounts", bytes.NewReader(reqBody))
	if err != nil {
		return nil, err
	}
	req.Header.Set("Content-Type", "application/ld+json")
	req.Header.Set("Accept", "application/json")
	req.Header.Set("User-Agent", getCurrentUA())

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

	var result duckmailAccountResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, err
	}
	return &result, nil
}

func duckmailGetToken(address, password string) (string, error) {
	reqBody, _ := json.Marshal(map[string]string{
		"address":  address,
		"password": password,
	})

	req, err := http.NewRequest("POST", duckmailBaseURL+"/token", bytes.NewReader(reqBody))
	if err != nil {
		return "", err
	}
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Accept", "application/json")
	req.Header.Set("User-Agent", getCurrentUA())

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

	var result duckmailTokenResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return "", err
	}
	return result.Token, nil
}

func DuckmailGenerate() (*CreatedMailbox, error) {
	domains, err := duckmailGetDomains()
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

	account, err := duckmailCreateAccount(address, password)
	if err != nil {
		return nil, err
	}

	token, err := duckmailGetToken(address, password)
	if err != nil {
		return nil, err
	}

	info := &CreatedMailbox{Channel: "duckmail", Email: address, Token: token}
	info.CreatedAt = account.CreatedAt
	return info, nil
}

func duckmailFlattenMessage(raw map[string]interface{}, recipientEmail string) map[string]interface{} {
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
					att["downloadUrl"] = duckmailBaseURL + dl
				}
			}
		}
		flat["attachments"] = attachments
	}

	return flat
}

func DuckmailGetEmails(token string, email string) ([]NormEmail, error) {
	_ = email

	req, err := http.NewRequest("GET", duckmailBaseURL+"/messages?page=1", nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("Accept", "application/json")
	req.Header.Set("Authorization", "Bearer "+token)
	req.Header.Set("User-Agent", getCurrentUA())

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

	var msgItems []duckmailMessageItem
	if err := json.Unmarshal(body, &msgItems); err != nil {
		var listResult duckmailMessagesResponse
		if err2 := json.Unmarshal(body, &listResult); err2 != nil {
			return nil, err2
		}
		msgItems = listResult.Members
	}

	if len(msgItems) == 0 {
		return []NormEmail{}, nil
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

			detailReq, err := http.NewRequest("GET", duckmailBaseURL+"/messages/"+msgID, nil)
			if err != nil {
				results[idx] = detailResult{index: idx, err: err}
				return
			}
			detailReq.Header.Set("Accept", "application/json")
			detailReq.Header.Set("Authorization", "Bearer "+token)
			detailReq.Header.Set("User-Agent", getCurrentUA())

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

	emails := make([]NormEmail, 0, len(results))
	for _, r := range results {
		if r.err != nil || r.raw == nil {
			continue
		}
		flat := duckmailFlattenMessage(r.raw, email)
		emails = append(emails, NormalizeMap(flat, email))
	}

	return emails, nil
}
