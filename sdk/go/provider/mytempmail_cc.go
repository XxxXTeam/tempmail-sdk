package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"math/rand"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

/**
 * MytempmailCc — https://api.mytempmail.cc
 * 纯 JSON REST API，无需认证
 * 创建邮箱: POST /api/address  body: {"domain":"nilvaro.com","name":"<随机8位小写字母>","expiry":600}
 * 获取邮件: GET /api/mails/<token>
 */

const mytempmailCcBase = "https://api.mytempmail.cc"

/**
 * mytempmailCcCreateRequest — 创建邮箱的请求体
 */
type mytempmailCcCreateRequest struct {
	Domain string `json:"domain"`
	Name   string `json:"name"`
	Expiry int    `json:"expiry"`
}

/**
 * mytempmailCcCreateResponse — 创建邮箱的响应结构
 */
type mytempmailCcCreateResponse struct {
	Token     string `json:"token"`
	Address   string `json:"address"`
	ExpiresIn int    `json:"expires_in"`
	Verified  bool   `json:"verified"`
}

/**
 * mytempmailCcMailsResponse — 获取邮件列表的响应结构
 */
type mytempmailCcMailsResponse struct {
	Results []struct {
		ID         string `json:"id"`
		From       string `json:"from"`
		Subject    string `json:"subject"`
		BodyText   string `json:"body_text"`
		BodyHTML   string `json:"body_html"`
		ReceivedAt string `json:"received_at"`
	} `json:"results"`
	Count  int  `json:"count"`
	Limit  int  `json:"limit"`
	IsFull bool `json:"is_full"`
}

/**
 * mytempmailCcRandomName — 生成随机 8 位小写字母用户名
 */
func mytempmailCcRandomName() string {
	const chars = "abcdefghijklmnopqrstuvwxyz"
	b := make([]byte, 8)
	for i := range b {
		b[i] = chars[rand.Intn(len(chars))]
	}
	return string(b)
}

/**
 * MytempmailCcGenerate — 创建临时邮箱
 * POST /api/address，生成随机用户名，域名固定为 nilvaro.com，有效期 600 秒
 */
func MytempmailCcGenerate() (*CreatedMailbox, error) {
	reqBody := mytempmailCcCreateRequest{
		Domain: "nilvaro.com",
		Name:   mytempmailCcRandomName(),
		Expiry: 600,
	}
	bodyBytes, err := json.Marshal(reqBody)
	if err != nil {
		return nil, fmt.Errorf("mytempmail-cc 序列化请求体失败: %w", err)
	}

	req, err := http.NewRequest("POST", mytempmailCcBase+"/api/address", strings.NewReader(string(bodyBytes)))
	if err != nil {
		return nil, err
	}
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Accept", "application/json")

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, fmt.Errorf("mytempmail-cc 创建请求失败: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("mytempmail-cc 创建: HTTP %d", resp.StatusCode)
	}

	var data mytempmailCcCreateResponse
	if err := json.Unmarshal(body, &data); err != nil {
		return nil, fmt.Errorf("mytempmail-cc 创建响应解析失败: %w", err)
	}

	email := strings.TrimSpace(data.Address)
	if email == "" || !strings.Contains(email, "@") {
		return nil, fmt.Errorf("mytempmail-cc: 无效的邮箱响应")
	}

	token := strings.TrimSpace(data.Token)
	if token == "" {
		return nil, fmt.Errorf("mytempmail-cc: 响应中缺少 token")
	}

	return &CreatedMailbox{Channel: "mytempmail-cc", Email: email, Token: token}, nil
}

/**
 * MytempmailCcGetEmails — 获取邮件列表
 * GET /api/mails/<token>，解析 results 数组
 * 每条消息构造为 map[string]any 后调用 NormalizeMap 归一化
 */
func MytempmailCcGetEmails(token string, email string) ([]NormEmail, error) {
	token = strings.TrimSpace(token)
	if token == "" {
		return nil, fmt.Errorf("mytempmail-cc: token 为空")
	}

	u := fmt.Sprintf("%s/api/mails/%s", mytempmailCcBase, token)
	req, err := http.NewRequest("GET", u, nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("Accept", "application/json")

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, fmt.Errorf("mytempmail-cc 获取邮件请求失败: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("mytempmail-cc 获取邮件: HTTP %d", resp.StatusCode)
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	var data mytempmailCcMailsResponse
	if err := json.Unmarshal(body, &data); err != nil {
		return nil, fmt.Errorf("mytempmail-cc 邮件响应解析失败: %w", err)
	}

	out := make([]NormEmail, 0, len(data.Results))
	for _, msg := range data.Results {
		row := map[string]any{
			"id":          msg.ID,
			"from":        msg.From,
			"to":          email,
			"subject":     msg.Subject,
			"text":        msg.BodyText,
			"html":        msg.BodyHTML,
			"received_at": msg.ReceivedAt,
		}
		out = append(out, NormalizeMap(row, email))
	}
	return out, nil
}
