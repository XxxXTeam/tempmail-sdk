package provider

import (
	"encoding/json"
	"fmt"
	"io"

	http "github.com/bogdanfinn/fhttp"
)

// temp-mail.org：web2.temp-mail.org 后端，动态分配域名的临时邮箱服务

const tempMailOrgBase = "https://web2.temp-mail.org"

// tempMailOrgCreateResponse 创建邮箱 API 响应
type tempMailOrgCreateResponse struct {
	Token   string `json:"token"`
	Mailbox string `json:"mailbox"`
}

// tempMailOrgMessagesResponse 邮件列表 API 响应
type tempMailOrgMessagesResponse struct {
	Mailbox  string               `json:"mailbox"`
	Messages []tempMailOrgMessage `json:"messages"`
}

// tempMailOrgMessage 邮件列表中的单封邮件摘要
type tempMailOrgMessage struct {
	ID               string `json:"_id"`
	ReceivedAt       int64  `json:"receivedAt"`
	From             string `json:"from"`
	Subject          string `json:"subject"`
	BodyPreview      string `json:"bodyPreview"`
	AttachmentsCount int    `json:"attachmentsCount"`
}

// tempMailOrgDetail 邮件详情 API 响应
type tempMailOrgDetail struct {
	ID               string `json:"_id"`
	ReceivedAt       int64  `json:"receivedAt"`
	Mailbox          string `json:"mailbox"`
	From             string `json:"from"`
	Subject          string `json:"subject"`
	BodyPreview      string `json:"bodyPreview"`
	BodyHtml         string `json:"bodyHtml"`
	AttachmentsCount int    `json:"attachmentsCount"`
	Attachments      []any  `json:"attachments"`
	CreatedAt        string `json:"createdAt"`
}

// tempMailOrgHeaders 设置 temp-mail.org 请求的通用 Headers
func tempMailOrgHeaders(req *http.Request, token string) {
	req.Header.Set("User-Agent", "Mozilla/5.0")
	req.Header.Set("Origin", "https://temp-mail.org")
	req.Header.Set("Referer", "https://temp-mail.org/")
	if token != "" {
		req.Header.Set("Authorization", "Bearer "+token)
	}
}

/*
 * TempMailOrgGenerate 创建 temp-mail.org 临时邮箱
 * POST /mailbox 返回 JWT token 和动态分配的邮箱地址
 * @param duration - 保留参数，当前未使用
 * @param domain - 保留参数，当前未使用（域名由服务端动态分配）
 */
func TempMailOrgGenerate(duration int, domain string) (*CreatedMailbox, error) {
	req, err := http.NewRequest(http.MethodPost, tempMailOrgBase+"/mailbox", nil)
	if err != nil {
		return nil, err
	}
	tempMailOrgHeaders(req, "")
	req.Header.Set("Content-Length", "0")

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
		return nil, fmt.Errorf("temp-mail-org: 创建邮箱失败 http %d: %s", resp.StatusCode, string(body))
	}

	var data tempMailOrgCreateResponse
	if err := json.Unmarshal(body, &data); err != nil {
		return nil, fmt.Errorf("temp-mail-org: 解析创建响应失败: %w", err)
	}
	if data.Token == "" || data.Mailbox == "" {
		return nil, fmt.Errorf("temp-mail-org: 创建邮箱响应缺少必要字段: %s", string(body))
	}

	return &CreatedMailbox{
		Channel: "temp-mail-org",
		Email:   data.Mailbox,
		Token:   data.Token,
	}, nil
}

/*
 * TempMailOrgGetEmails 获取 temp-mail.org 邮件列表
 * 先 GET /messages 获取邮件 ID 列表，再逐封 GET /messages/{id} 取详情
 * @param token - JWT 认证令牌
 * @param email - 邮箱地址
 */
func TempMailOrgGetEmails(token string, email string) ([]NormEmail, error) {
	// 获取邮件列表
	req, err := http.NewRequest(http.MethodGet, tempMailOrgBase+"/messages", nil)
	if err != nil {
		return nil, err
	}
	tempMailOrgHeaders(req, token)

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
		return nil, fmt.Errorf("temp-mail-org: 获取邮件列表失败 http %d: %s", resp.StatusCode, string(body))
	}

	var listResp tempMailOrgMessagesResponse
	if err := json.Unmarshal(body, &listResp); err != nil {
		return nil, fmt.Errorf("temp-mail-org: 解析邮件列表失败: %w", err)
	}

	out := make([]NormEmail, 0, len(listResp.Messages))
	for _, msg := range listResp.Messages {
		// 逐封获取详情
		detail, err := tempMailOrgGetDetail(token, msg.ID)
		if err != nil {
			// 详情获取失败时使用摘要信息
			out = append(out, NormalizeMap(map[string]interface{}{
				"id":          msg.ID,
				"from":        msg.From,
				"to":          email,
				"subject":     msg.Subject,
				"text":        msg.BodyPreview,
				"html":        "",
				"date":        msg.ReceivedAt,
				"attachments": []interface{}{},
			}, email))
			continue
		}
		out = append(out, NormalizeMap(map[string]interface{}{
			"id":          detail.ID,
			"from":        detail.From,
			"to":          detail.Mailbox,
			"subject":     detail.Subject,
			"text":        detail.BodyPreview,
			"html":        detail.BodyHtml,
			"date":        detail.ReceivedAt,
			"attachments": detail.Attachments,
		}, email))
	}
	return out, nil
}

// tempMailOrgGetDetail 获取单封邮件详情
func tempMailOrgGetDetail(token string, msgID string) (*tempMailOrgDetail, error) {
	req, err := http.NewRequest(http.MethodGet, tempMailOrgBase+"/messages/"+msgID, nil)
	if err != nil {
		return nil, err
	}
	tempMailOrgHeaders(req, token)

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
		return nil, fmt.Errorf("temp-mail-org: 获取邮件详情失败 http %d: %s", resp.StatusCode, string(body))
	}

	var detail tempMailOrgDetail
	if err := json.Unmarshal(body, &detail); err != nil {
		return nil, fmt.Errorf("temp-mail-org: 解析邮件详情失败: %w", err)
	}
	return &detail, nil
}
