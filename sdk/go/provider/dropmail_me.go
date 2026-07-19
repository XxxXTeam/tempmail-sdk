package provider

import (
	"bytes"
	"crypto/rand"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"io"
	"regexp"
	"strings"
	"time"

	http "github.com/bogdanfinn/fhttp"
)

/* dropmail.me 临时邮箱服务商
 * 流程：GET /en/ 提取 data-k → 解码 secret → 生成 auth token
 *       GraphQL introduceSession 创建会话 → session(id) 获取邮件
 * token 存储 JSON: {"session_id":"...","auth_token":"website_..."}
 */

const dropmailMeBaseURL = "https://dropmail.me"

var dropmailMeDataKRegex = regexp.MustCompile(`<meta\s+name="app-config"\s+data-k="([^"]+)"`)

/* dropmailMeFnvHash 计算 FNV-1a 哈希（与前端 JS 实现一致） */
func dropmailMeFnvHash(s string) string {
	hash := uint32(2166136261)
	for i := 0; i < len(s); i++ {
		hash ^= uint32(s[i])
		hash += (hash << 1) + (hash << 4) + (hash << 7) + (hash << 8) + (hash << 24)
	}
	return fmt.Sprintf("%x", hash)
}

/* dropmailMeRandomPart 生成随机部分：YYYYMMDD + 16位随机字母数字 */
func dropmailMeRandomPart() string {
	dateStr := time.Now().UTC().Format("20060102")
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	b := make([]byte, 16)
	rand.Read(b)
	for i := range b {
		b[i] = chars[int(b[i])%len(chars)]
	}
	return dateStr + string(b)
}

/* dropmailMeExtractSecret 从 data-k 值中提取 secret: reverse + base64 decode */
func dropmailMeExtractSecret(dataK string) (string, error) {
	runes := []rune(dataK)
	for i, j := 0, len(runes)-1; i < j; i, j = i+1, j-1 {
		runes[i], runes[j] = runes[j], runes[i]
	}
	decoded, err := base64.StdEncoding.DecodeString(string(runes))
	if err != nil {
		return "", fmt.Errorf("dropmail-me: base64 解码 secret 失败: %w", err)
	}
	return string(decoded), nil
}

/* dropmailMeGenerateToken 从页面提取 data-k 并生成 auth token */
func dropmailMeGenerateToken() (string, error) {
	client := HTTPClient()

	req, err := http.NewRequest("GET", dropmailMeBaseURL+"/en/", nil)
	if err != nil {
		return "", fmt.Errorf("dropmail-me: 创建页面请求失败: %w", err)
	}
	req.Header.Set("User-Agent", getCurrentUA())
	req.Header.Set("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8")

	resp, err := client.Do(req)
	if err != nil {
		return "", fmt.Errorf("dropmail-me: 获取页面失败: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", fmt.Errorf("dropmail-me: 读取页面响应失败: %w", err)
	}

	if err := CheckHTTPStatus(resp, "dropmail-me home"); err != nil {
		return "", err
	}

	matches := dropmailMeDataKRegex.FindSubmatch(body)
	if len(matches) < 2 {
		return "", fmt.Errorf("dropmail-me: 未找到 data-k 属性")
	}

	secret, err := dropmailMeExtractSecret(string(matches[1]))
	if err != nil {
		return "", err
	}

	randomPart := dropmailMeRandomPart()
	hash := dropmailMeFnvHash(randomPart + secret)
	return "website_" + randomPart + "_" + hash, nil
}

/* dropmailMeGraphQL 执行 GraphQL 请求 */
func dropmailMeGraphQL(authToken, query string) ([]byte, error) {
	client := HTTPClient()

	payload, _ := json.Marshal(map[string]string{"query": query})
	apiURL := fmt.Sprintf("%s/api/graphql/%s", dropmailMeBaseURL, authToken)

	req, err := http.NewRequest("POST", apiURL, bytes.NewReader(payload))
	if err != nil {
		return nil, fmt.Errorf("dropmail-me: 创建 GraphQL 请求失败: %w", err)
	}
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("User-Agent", getCurrentUA())

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("dropmail-me: GraphQL 请求失败: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("dropmail-me: 读取 GraphQL 响应失败: %w", err)
	}

	if err := CheckHTTPStatus(resp, "dropmail-me graphql"); err != nil {
		return nil, err
	}
	return body, nil
}

/* DropmailMeGenerate 创建 dropmail.me 临时邮箱
 * 1. 生成 auth token
 * 2. GraphQL introduceSession 创建会话
 * duration 和 domain 参数保留以匹配接口签名（当前未使用）
 */
func DropmailMeGenerate(duration int, domain string) (*CreatedMailbox, error) {
	authToken, err := dropmailMeGenerateToken()
	if err != nil {
		return nil, err
	}

	query := `mutation { introduceSession { id expiresAt addresses { address } } }`
	body, err := dropmailMeGraphQL(authToken, query)
	if err != nil {
		return nil, err
	}

	var result struct {
		Data struct {
			IntroduceSession struct {
				ID        string `json:"id"`
				ExpiresAt string `json:"expiresAt"`
				Addresses []struct {
					Address string `json:"address"`
				} `json:"addresses"`
			} `json:"introduceSession"`
		} `json:"data"`
	}
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, fmt.Errorf("dropmail-me: 解析 introduceSession 响应失败: %w", err)
	}

	session := result.Data.IntroduceSession
	if session.ID == "" {
		return nil, fmt.Errorf("dropmail-me: 创建会话失败，响应: %s", string(body))
	}

	if len(session.Addresses) == 0 {
		return nil, fmt.Errorf("dropmail-me: 会话中无可用邮箱地址")
	}

	emailAddr := session.Addresses[0].Address

	/* token 存储 session_id 和 auth_token */
	tokenData, _ := json.Marshal(map[string]string{
		"session_id": session.ID,
		"auth_token": authToken,
	})

	return &CreatedMailbox{
		Channel:   "dropmail-me",
		Email:     emailAddr,
		Token:     string(tokenData),
		ExpiresAt: session.ExpiresAt,
	}, nil
}

/* DropmailMeGetEmails 获取 dropmail.me 邮件列表 */
func DropmailMeGetEmails(token, email string) ([]NormEmail, error) {
	token = strings.TrimSpace(token)
	if token == "" {
		return nil, fmt.Errorf("dropmail-me: token 为空")
	}

	var tokenData struct {
		SessionID string `json:"session_id"`
		AuthToken string `json:"auth_token"`
	}
	if err := json.Unmarshal([]byte(token), &tokenData); err != nil {
		return nil, fmt.Errorf("dropmail-me: 解析 token 失败: %w", err)
	}

	if tokenData.SessionID == "" || tokenData.AuthToken == "" {
		return nil, fmt.Errorf("dropmail-me: token 中缺少 session_id 或 auth_token")
	}

	query := fmt.Sprintf(`{ session(id:"%s") { mails { id headerFrom headerSubject text html receivedAt } } }`, tokenData.SessionID)
	body, err := dropmailMeGraphQL(tokenData.AuthToken, query)
	if err != nil {
		return nil, err
	}

	var result struct {
		Data struct {
			Session struct {
				Mails []struct {
					ID            string `json:"id"`
					HeaderFrom    string `json:"headerFrom"`
					HeaderSubject string `json:"headerSubject"`
					Text          string `json:"text"`
					HTML          string `json:"html"`
					ReceivedAt    string `json:"receivedAt"`
				} `json:"mails"`
			} `json:"session"`
		} `json:"data"`
	}
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, fmt.Errorf("dropmail-me: 解析邮件响应失败: %w", err)
	}

	mails := result.Data.Session.Mails
	if len(mails) == 0 {
		return []NormEmail{}, nil
	}

	emails := make([]NormEmail, 0, len(mails))
	for _, m := range mails {
		emails = append(emails, NormEmail{
			ID:      m.ID,
			From:    m.HeaderFrom,
			To:      email,
			Subject: m.HeaderSubject,
			Text:    m.Text,
			HTML:    m.HTML,
			Date:    m.ReceivedAt,
		})
	}
	return emails, nil
}
