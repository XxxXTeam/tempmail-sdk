package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

/* mailcat.ai 临时邮箱服务商
 * 流程：POST /mailboxes 创建邮箱（无 body），返回 email + token
 *       GET /inbox 获取邮件列表，需要 Authorization: Bearer {token}
 */

const mailcatAiBaseURL = "https://api.mailcat.ai"

/* mailcatAiCreateResponse 创建邮箱接口响应 */
type mailcatAiCreateResponse struct {
	Data struct {
		Email string `json:"email"`
		Token string `json:"token"`
	} `json:"data"`
}

/* mailcatAiInboxResponse 获取邮件列表接口响应 */
type mailcatAiInboxResponse struct {
	Data []json.RawMessage `json:"data"`
	Meta struct {
		Mailbox    string `json:"mailbox"`
		Unread     int    `json:"unread"`
		Pagination struct {
			Offset     int  `json:"offset"`
			Limit      int  `json:"limit"`
			TotalCount int  `json:"totalCount"`
			HasMore    bool `json:"hasMore"`
		} `json:"pagination"`
	} `json:"meta"`
}

/* MailcatAiGenerate 创建 mailcat.ai 临时邮箱
 * API: POST /mailboxes（无 body）
 * 返回邮箱地址和 Bearer token
 */
func MailcatAiGenerate() (*CreatedMailbox, error) {
	client := HTTPClient()

	req, err := http.NewRequest("POST", mailcatAiBaseURL+"/mailboxes", nil)
	if err != nil {
		return nil, fmt.Errorf("mailcat-ai: 创建请求失败: %w", err)
	}
	req.Header.Set("Accept", "application/json")
	ua := getCurrentUA()
	if ua != "" {
		req.Header.Set("User-Agent", ua)
	}

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("mailcat-ai: 请求失败: %w", err)
	}
	defer resp.Body.Close()

	if err := CheckHTTPStatus(resp, "mailcat-ai create mailbox"); err != nil {
		return nil, err
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("mailcat-ai: 读取响应失败: %w", err)
	}

	var result mailcatAiCreateResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, fmt.Errorf("mailcat-ai: 解析响应失败: %w", err)
	}

	if result.Data.Email == "" || result.Data.Token == "" {
		return nil, fmt.Errorf("mailcat-ai: 响应缺少必要字段（email 或 token）")
	}

	return &CreatedMailbox{
		Channel: "mailcat-ai",
		Email:   result.Data.Email,
		Token:   result.Data.Token,
	}, nil
}

/* MailcatAiGetEmails 获取 mailcat.ai 邮件列表
 * API: GET /inbox
 * Headers: Authorization: Bearer {token}
 */
func MailcatAiGetEmails(token, email string) ([]NormEmail, error) {
	if token == "" {
		return nil, fmt.Errorf("mailcat-ai: token 为空")
	}

	client := HTTPClient()

	req, err := http.NewRequest("GET", mailcatAiBaseURL+"/inbox", nil)
	if err != nil {
		return nil, fmt.Errorf("mailcat-ai: 创建获取邮件请求失败: %w", err)
	}
	req.Header.Set("Accept", "application/json")
	req.Header.Set("Authorization", "Bearer "+strings.TrimSpace(token))
	ua := getCurrentUA()
	if ua != "" {
		req.Header.Set("User-Agent", ua)
	}

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("mailcat-ai: 获取邮件请求失败: %w", err)
	}
	defer resp.Body.Close()

	if err := CheckHTTPStatus(resp, "mailcat-ai get inbox"); err != nil {
		return nil, err
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("mailcat-ai: 读取邮件响应失败: %w", err)
	}

	var result mailcatAiInboxResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, fmt.Errorf("mailcat-ai: 解析邮件列表失败: %w", err)
	}

	if len(result.Data) == 0 {
		return []NormEmail{}, nil
	}

	return NormalizeRawMessages(result.Data, email)
}
