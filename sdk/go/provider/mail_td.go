package provider

import (
	"crypto/sha256"
	"encoding/json"
	"fmt"
	"io"
	"math/rand"
	"strconv"
	"strings"
	"time"

	http "github.com/bogdanfinn/fhttp"
)

/**
 * MailTd — https://api.mail.td
 * 基于 SHA-256 Proof-of-Work 的临时邮箱服务
 * 流程:
 *   1. GET /api/domains 获取可用域名
 *   2. 求解 PoW: SHA-256(address + timestamp + nonce) 需满足 difficulty 个前导零位
 *   3. POST /api/accounts 携带 PoW 创建账户 → 返回 JWT + ID
 *   4. GET /api/accounts/{id}/messages?page=1 携带 Bearer JWT 获取邮件
 * Token 格式: JSON {"jwt":"...","id":"..."}
 */

const mailTdBase = "https://api.mail.td/api"

/**
 * mailTdDomainsResponse — 域名列表响应
 */
type mailTdDomainsResponse struct {
	Domains []struct {
		ID      string `json:"id"`
		Domain  string `json:"domain"`
		ProOnly bool   `json:"pro_only"`
	} `json:"domains"`
}

/**
 * mailTdAccountResponse — 创建账户响应
 */
type mailTdAccountResponse struct {
	ID                      string `json:"id"`
	Address                 string `json:"address"`
	Token                   string `json:"token"`
	Status                  string `json:"status"`
	RequiredDifficulty      int    `json:"required_difficulty"`
	PowToken                string `json:"token_retry"`
	SuggestedNextDifficulty int    `json:"suggested_next_difficulty"`
	Error                   string `json:"error"`
}

/**
 * mailTdMessagesResponse — 邮件列表响应
 */
type mailTdMessagesResponse struct {
	Messages []struct {
		ID   string `json:"id"`
		From struct {
			Address string `json:"address"`
			Name    string `json:"name"`
		} `json:"from"`
		Subject   string `json:"subject"`
		Text      string `json:"text"`
		HTML      string `json:"html"`
		Seen      bool   `json:"seen"`
		CreatedAt string `json:"created_at"`
	} `json:"messages"`
	Page int `json:"page"`
}

/**
 * mailTdTokenData — 内部存储的 token 结构
 */
type mailTdTokenData struct {
	JWT string `json:"jwt"`
	ID  string `json:"id"`
}

/**
 * mailTdCheckLeadingZeros — 检查哈希是否满足 difficulty 个前导零位
 */
func mailTdCheckLeadingZeros(hash [32]byte, difficulty int) bool {
	fullBytes := difficulty / 8
	remainBits := difficulty % 8
	for i := 0; i < fullBytes; i++ {
		if hash[i] != 0 {
			return false
		}
	}
	if remainBits > 0 && fullBytes < 32 {
		mask := byte((255 << (8 - remainBits)) & 255)
		if hash[fullBytes]&mask != 0 {
			return false
		}
	}
	return true
}

/**
 * mailTdSolvePoW — 求解 Proof-of-Work
 * 输入: address + timestamp + nonce
 * 目标: SHA-256 哈希有 difficulty 个前导零位
 */
func mailTdSolvePoW(address string, timestamp int64, difficulty int) (string, error) {
	base := address + strconv.FormatInt(timestamp, 10)
	for nonce := 0; nonce < 100000000; nonce++ {
		input := base + strconv.Itoa(nonce)
		hash := sha256.Sum256([]byte(input))
		if mailTdCheckLeadingZeros(hash, difficulty) {
			return strconv.Itoa(nonce), nil
		}
	}
	return "", fmt.Errorf("mail-td: PoW 求解超时")
}

/**
 * mailTdRandomString — 生成随机小写字母+数字字符串
 */
func mailTdRandomString(n int) string {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	b := make([]byte, n)
	for i := range b {
		b[i] = chars[rand.Intn(len(chars))]
	}
	return string(b)
}

/**
 * MailTdGenerate — 创建 mail.td 临时邮箱
 */
func MailTdGenerate() (*CreatedMailbox, error) {
	client := HTTPClient()

	/* 获取可用域名 */
	req, err := http.NewRequest("GET", mailTdBase+"/domains", nil)
	if err != nil {
		return nil, fmt.Errorf("mail-td: 创建域名请求失败: %w", err)
	}
	req.Header.Set("Accept", "application/json")
	req.Header.Set("User-Agent", getCurrentUA())

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("mail-td: 域名请求失败: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("mail-td: 读取域名响应失败: %w", err)
	}

	if resp.StatusCode != 200 {
		return nil, fmt.Errorf("mail-td: 域名请求 HTTP %d", resp.StatusCode)
	}

	var domsResp mailTdDomainsResponse
	if err := json.Unmarshal(body, &domsResp); err != nil {
		return nil, fmt.Errorf("mail-td: 域名响应解析失败: %w", err)
	}

	/* 筛选免费域名 */
	var freeDomains []string
	for _, d := range domsResp.Domains {
		if !d.ProOnly && d.Domain != "" {
			freeDomains = append(freeDomains, d.Domain)
		}
	}
	if len(freeDomains) == 0 {
		return nil, fmt.Errorf("mail-td: 无可用免费域名")
	}

	domain := freeDomains[rand.Intn(len(freeDomains))]
	username := mailTdRandomString(10)
	address := username + "@" + domain
	password := mailTdRandomString(20)
	authKeyBytes := sha256.Sum256([]byte(password)) //nolint:gosec // mail.td API 认证协议要求，非密码存储
	authKey := fmt.Sprintf("%x", authKeyBytes)

	addressLower := strings.ToLower(strings.TrimSpace(address))
	difficulty := 15
	var powToken string

	/* PoW 重试循环 */
	for retry := 0; retry < 4; retry++ {
		timestamp := time.Now().Unix()
		nonce, err := mailTdSolvePoW(addressLower, timestamp, difficulty)
		if err != nil {
			return nil, err
		}

		powObj := map[string]interface{}{
			"t": timestamp,
			"n": nonce,
			"d": difficulty,
		}
		if powToken != "" {
			powObj["token"] = powToken
		}

		reqBody := map[string]interface{}{
			"address":  address,
			"auth_key": authKey,
			"pow":      powObj,
		}
		reqBytes, _ := json.Marshal(reqBody)

		req2, err := http.NewRequest("POST", mailTdBase+"/accounts", strings.NewReader(string(reqBytes)))
		if err != nil {
			return nil, fmt.Errorf("mail-td: 创建账户请求构建失败: %w", err)
		}
		req2.Header.Set("Content-Type", "application/json")
		req2.Header.Set("Accept", "application/json")
		req2.Header.Set("User-Agent", getCurrentUA())

		resp2, err := client.Do(req2)
		if err != nil {
			return nil, fmt.Errorf("mail-td: 创建账户请求失败: %w", err)
		}

		body2, _ := io.ReadAll(resp2.Body)
		resp2.Body.Close()

		var result map[string]interface{}
		if err := json.Unmarshal(body2, &result); err != nil {
			return nil, fmt.Errorf("mail-td: 创建账户响应解析失败: %w", err)
		}

		/* 检查是否需要增加 difficulty 重试 */
		if status, ok := result["status"].(string); ok && status == "retry" {
			if rd, ok := result["required_difficulty"].(float64); ok {
				difficulty = int(rd)
			} else {
				difficulty += 2
			}
			if tk, ok := result["token"].(string); ok {
				powToken = tk
			}
			continue
		}

		if resp2.StatusCode < 200 || resp2.StatusCode >= 300 {
			errMsg := ""
			if e, ok := result["error"].(string); ok {
				errMsg = e
			}
			return nil, fmt.Errorf("mail-td: 创建账户 HTTP %d: %s", resp2.StatusCode, errMsg)
		}

		resultAddr, _ := result["address"].(string)
		resultToken, _ := result["token"].(string)
		resultID, _ := result["id"].(string)

		if resultAddr == "" || resultToken == "" || resultID == "" {
			return nil, fmt.Errorf("mail-td: 响应缺少必要字段")
		}

		tokenData := mailTdTokenData{JWT: resultToken, ID: resultID}
		tokenBytes, _ := json.Marshal(tokenData)

		return &CreatedMailbox{
			Channel: "mail-td",
			Email:   resultAddr,
			Token:   string(tokenBytes),
		}, nil
	}

	return nil, fmt.Errorf("mail-td: PoW 重试次数超限")
}

/**
 * MailTdGetEmails — 获取 mail.td 邮件列表
 */
func MailTdGetEmails(token, email string) ([]NormEmail, error) {
	if token == "" {
		return nil, fmt.Errorf("mail-td: token 为空")
	}

	var tokenData mailTdTokenData
	if err := json.Unmarshal([]byte(token), &tokenData); err != nil {
		return nil, fmt.Errorf("mail-td: token 格式无效: %w", err)
	}

	if tokenData.JWT == "" || tokenData.ID == "" {
		return nil, fmt.Errorf("mail-td: token 缺少 jwt 或 id")
	}

	client := HTTPClient()
	u := fmt.Sprintf("%s/accounts/%s/messages?page=1", mailTdBase, tokenData.ID)
	req, err := http.NewRequest("GET", u, nil)
	if err != nil {
		return nil, fmt.Errorf("mail-td: 创建邮件请求失败: %w", err)
	}
	req.Header.Set("Accept", "application/json")
	req.Header.Set("Authorization", "Bearer "+tokenData.JWT)
	req.Header.Set("User-Agent", getCurrentUA())

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("mail-td: 邮件请求失败: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("mail-td: 邮件请求 HTTP %d", resp.StatusCode)
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("mail-td: 读取邮件响应失败: %w", err)
	}

	var data mailTdMessagesResponse
	if err := json.Unmarshal(body, &data); err != nil {
		return nil, fmt.Errorf("mail-td: 邮件响应解析失败: %w", err)
	}

	out := make([]NormEmail, 0, len(data.Messages))
	for _, msg := range data.Messages {
		row := map[string]any{
			"id":         msg.ID,
			"from":       msg.From.Address,
			"to":         email,
			"subject":    msg.Subject,
			"text":       msg.Text,
			"html":       msg.HTML,
			"created_at": msg.CreatedAt,
		}
		out = append(out, NormalizeMap(row, email))
	}
	return out, nil
}
