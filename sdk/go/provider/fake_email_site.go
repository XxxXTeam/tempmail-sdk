package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"net/url"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

const fakeEmailSiteBase = "https://api.fake-email.site"

/**
 * fakeEmailSiteCreateResponse — 创建临时邮箱的响应结构
 */
type fakeEmailSiteCreateResponse struct {
	TempEmailAddr string `json:"temp_email_addr"`
}

/**
 * fakeEmailSiteInboxResponse — 轮询收件箱的响应结构
 */
type fakeEmailSiteInboxResponse struct {
	TempEmailAddr string `json:"temp_email_addr"`
	NewMailCount  int    `json:"new_mail_count"`
	NextSince     any    `json:"next_since"`
	Messages      []struct {
		ID         string `json:"id"`
		FromAddr   string `json:"from_addr"`
		ToAddr     string `json:"to_addr"`
		Subject    string `json:"subject"`
		BodyText   string `json:"body_text"`
		ReceivedAt string `json:"received_at"`
	} `json:"messages"`
}

/**
 * FakeEmailSiteGenerate — 创建临时邮箱
 * POST /api/temporary-address，body 为空对象 {}，解析 temp_email_addr
 */
func FakeEmailSiteGenerate() (*CreatedMailbox, error) {
	req, err := http.NewRequest("POST", fakeEmailSiteBase+"/api/temporary-address", strings.NewReader("{}"))
	if err != nil {
		return nil, err
	}
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Accept", "application/json")

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, fmt.Errorf("fake-email-site 创建请求失败: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("fake-email-site 创建: HTTP %d", resp.StatusCode)
	}

	var data fakeEmailSiteCreateResponse
	if err := json.Unmarshal(body, &data); err != nil {
		return nil, fmt.Errorf("fake-email-site 创建响应解析失败: %w", err)
	}

	email := strings.TrimSpace(data.TempEmailAddr)
	if email == "" || !strings.Contains(email, "@") {
		return nil, fmt.Errorf("fake-email-site: 无效的邮箱响应")
	}

	return &CreatedMailbox{Channel: "fake-email-site", Email: email, Token: email}, nil
}

/**
 * FakeEmailSiteGetEmails — 获取邮件列表
 * GET /api/inbox/poll?address={email}，解析 messages 数组
 * 404 返回空数组（邮箱未找到或暂无邮件）
 * 每条消息构造为 map[string]any 后调用 NormalizeMap 归一化
 */
func FakeEmailSiteGetEmails(email string) ([]NormEmail, error) {
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("fake-email-site: 邮箱地址为空")
	}

	u := fmt.Sprintf("%s/api/inbox/poll?address=%s", fakeEmailSiteBase, url.QueryEscape(email))
	req, err := http.NewRequest("GET", u, nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("Accept", "application/json")

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, fmt.Errorf("fake-email-site 轮询请求失败: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode == 404 {
		return []NormEmail{}, nil
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("fake-email-site 轮询: HTTP %d", resp.StatusCode)
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	var data fakeEmailSiteInboxResponse
	if err := json.Unmarshal(body, &data); err != nil {
		return nil, fmt.Errorf("fake-email-site 轮询响应解析失败: %w", err)
	}

	out := make([]NormEmail, 0, len(data.Messages))
	for _, msg := range data.Messages {
		row := map[string]any{
			"id":          msg.ID,
			"from":        msg.FromAddr,
			"to":          email,
			"subject":     msg.Subject,
			"text":        msg.BodyText,
			"received_at": msg.ReceivedAt,
		}
		out = append(out, NormalizeMap(row, email))
	}
	return out, nil
}
