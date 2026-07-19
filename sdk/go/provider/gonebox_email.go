package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

/* gonebox.email 临时邮箱服务商
 * 流程：POST /api/v1/inboxes 创建邮箱（指定 domain）
 *       GET /api/v1/inboxes/{address}/messages 获取邮件列表
 * 无需认证 token
 * 可选域名: gonebox.email, sumiu.email, nemexiste.email
 */

const goneboxEmailBaseURL = "https://api.gonebox.email/api/v1"

/* goneboxEmailCreateResponse 创建邮箱接口响应 */
type goneboxEmailCreateResponse struct {
	Success bool `json:"success"`
	Data    struct {
		ID        string `json:"id"`
		Address   string `json:"address"`
		Domain    string `json:"domain"`
		ExpiresAt int64  `json:"expiresAt"`
		TTL       int64  `json:"ttl"`
	} `json:"data"`
}

/* goneboxEmailMessagesResponse 获取邮件列表接口响应 */
type goneboxEmailMessagesResponse struct {
	Success bool `json:"success"`
	Data    struct {
		Address   string            `json:"address"`
		ExpiresAt int64             `json:"expiresAt"`
		TTL       int64             `json:"ttl"`
		Count     int               `json:"count"`
		Messages  []json.RawMessage `json:"messages"`
	} `json:"data"`
}

/* GoneboxEmailGenerate 创建 gonebox.email 临时邮箱
 * API: POST /api/v1/inboxes
 * Body: {"domain":"gonebox.email"}
 * 返回邮箱地址，无需 token
 */
func GoneboxEmailGenerate() (*CreatedMailbox, error) {
	client := HTTPClient()

	body := `{"domain":"gonebox.email"}`
	req, err := http.NewRequest("POST", goneboxEmailBaseURL+"/inboxes", strings.NewReader(body))
	if err != nil {
		return nil, fmt.Errorf("gonebox-email: 创建请求失败: %w", err)
	}
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Accept", "application/json")
	ua := getCurrentUA()
	if ua != "" {
		req.Header.Set("User-Agent", ua)
	}

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("gonebox-email: 请求失败: %w", err)
	}
	defer resp.Body.Close()

	if err := CheckHTTPStatus(resp, "gonebox-email create inbox"); err != nil {
		return nil, err
	}

	respBody, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("gonebox-email: 读取响应失败: %w", err)
	}

	var result goneboxEmailCreateResponse
	if err := json.Unmarshal(respBody, &result); err != nil {
		return nil, fmt.Errorf("gonebox-email: 解析响应失败: %w", err)
	}

	if !result.Success || result.Data.Address == "" {
		return nil, fmt.Errorf("gonebox-email: 创建邮箱失败，响应缺少 address")
	}

	return &CreatedMailbox{
		Channel:   "gonebox-email",
		Email:     result.Data.Address,
		Token:     "",
		ExpiresAt: result.Data.ExpiresAt,
	}, nil
}

/* GoneboxEmailGetEmails 获取 gonebox.email 邮件列表
 * API: GET /api/v1/inboxes/{address}/messages
 * 无需认证
 */
func GoneboxEmailGetEmails(email string) ([]NormEmail, error) {
	address := strings.TrimSpace(email)
	if address == "" {
		return nil, fmt.Errorf("gonebox-email: 缺少邮箱地址")
	}

	client := HTTPClient()

	u := fmt.Sprintf("%s/inboxes/%s/messages", goneboxEmailBaseURL, address)
	req, err := http.NewRequest("GET", u, nil)
	if err != nil {
		return nil, fmt.Errorf("gonebox-email: 创建获取邮件请求失败: %w", err)
	}
	req.Header.Set("Accept", "application/json")
	ua := getCurrentUA()
	if ua != "" {
		req.Header.Set("User-Agent", ua)
	}

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("gonebox-email: 获取邮件请求失败: %w", err)
	}
	defer resp.Body.Close()

	if err := CheckHTTPStatus(resp, "gonebox-email get messages"); err != nil {
		return nil, err
	}

	respBody, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("gonebox-email: 读取邮件响应失败: %w", err)
	}

	var result goneboxEmailMessagesResponse
	if err := json.Unmarshal(respBody, &result); err != nil {
		return nil, fmt.Errorf("gonebox-email: 解析邮件列表失败: %w", err)
	}

	if len(result.Data.Messages) == 0 {
		return []NormEmail{}, nil
	}

	return NormalizeRawMessages(result.Data.Messages, address)
}
