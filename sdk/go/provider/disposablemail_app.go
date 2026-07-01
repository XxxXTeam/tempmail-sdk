package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

/**
 * DisposableMail — https://disposablemail.app
 * 纯 REST JSON API，无需认证
 * 创建收件箱: POST https://disposablemail.app/api/inbox
 * 获取邮件: GET https://disposablemail.app/api/inbox/emails?token={token}
 * 域名: @disposablemail.dev, @mailmehere.cc
 */

const disposablemailAppAPIBase = "https://disposablemail.app/api"

/* disposablemailAppHeaders 设置请求头 */
func disposablemailAppHeaders(req *http.Request) {
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Accept", "application/json, text/plain, */*")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
	req.Header.Set("Referer", "https://disposablemail.app/")
	req.Header.Set("Origin", "https://disposablemail.app")
	req.Header.Set("User-Agent", getCurrentUA())
}

/*
 * DisposablemailAppGenerate 创建 disposablemail.app 临时邮箱
 * 流程:
 *   1. POST /api/inbox （body 为空 JSON {}）
 *   2. 解析响应中的 address、token
 *   3. token 直接存储 API 返回的 token 字符串
 */
func DisposablemailAppGenerate(channel ...string) (*CreatedMailbox, error) {
	req, err := http.NewRequest("POST", disposablemailAppAPIBase+"/inbox", strings.NewReader("{}"))
	if err != nil {
		return nil, fmt.Errorf("disposablemail-app: 创建请求失败: %w", err)
	}
	disposablemailAppHeaders(req)

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, fmt.Errorf("disposablemail-app: 请求创建收件箱失败: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("disposablemail-app: 读取响应失败: %w", err)
	}

	if err := CheckHTTPStatus(resp, "disposablemail-app inbox"); err != nil {
		return nil, err
	}

	/*
	 * 解析响应:
	 * {"id":"cmqzoipqq...","address":"8c802d7d6a@disposablemail.dev",
	 *  "token":"t6rGc1kHFrTCwheYDdIZnBvhdmaI3rQc_SaSJsnAMD4",
	 *  "domain":"disposablemail.dev","expiresAt":"...","createdAt":"..."}
	 */
	var result struct {
		Address   string `json:"address"`
		Token     string `json:"token"`
		ExpiresAt string `json:"expiresAt"`
		CreatedAt string `json:"createdAt"`
	}
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, fmt.Errorf("disposablemail-app: 解析响应失败: %w", err)
	}

	if result.Address == "" || result.Token == "" {
		return nil, fmt.Errorf("disposablemail-app: 创建收件箱失败, address 或 token 为空, body=%s", string(body))
	}

	ch := "disposablemail-app"
	if len(channel) > 0 && channel[0] != "" {
		ch = channel[0]
	}

	return &CreatedMailbox{
		Channel:   ch,
		Email:     result.Address,
		Token:     result.Token,
		ExpiresAt: result.ExpiresAt,
		CreatedAt: result.CreatedAt,
	}, nil
}

/*
 * DisposablemailAppGetEmails 获取 disposablemail.app 邮件列表
 * 流程:
 *   1. GET /api/inbox/emails?token={token}
 *   2. 解析响应中的 emails 数组并归一化
 */
func DisposablemailAppGetEmails(email, token string) ([]NormEmail, error) {
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("disposablemail-app: 邮箱地址为空")
	}
	if token == "" {
		return nil, fmt.Errorf("disposablemail-app: token 为空")
	}

	u := fmt.Sprintf("%s/inbox/emails?token=%s", disposablemailAppAPIBase, token)
	req, err := http.NewRequest("GET", u, nil)
	if err != nil {
		return nil, fmt.Errorf("disposablemail-app: 创建请求失败: %w", err)
	}
	disposablemailAppHeaders(req)
	/* GET 请求不需要 Content-Type */
	req.Header.Del("Content-Type")

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, fmt.Errorf("disposablemail-app: 请求获取邮件失败: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("disposablemail-app: 读取响应失败: %w", err)
	}

	if err := CheckHTTPStatus(resp, "disposablemail-app emails"); err != nil {
		return nil, err
	}

	/*
	 * 解析响应:
	 * {"emails":[],"total":0,"inbox":{"address":"...","expiresAt":"..."}}
	 * 有邮件时 emails 数组包含邮件对象
	 */
	var result struct {
		Emails []map[string]interface{} `json:"emails"`
		Total  int                      `json:"total"`
	}
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, fmt.Errorf("disposablemail-app: 解析邮件列表失败: %w", err)
	}

	if len(result.Emails) == 0 {
		return []NormEmail{}, nil
	}

	return normEmailsFromMaps(result.Emails, email), nil
}
