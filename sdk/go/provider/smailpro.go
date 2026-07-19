package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"net/url"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

/* smailpro.com 临时邮箱服务商
 * 流程为两段式：
 *   1. GET https://smailpro.com/app/payload?url={目标API}[&email=&mid=] → 返回 JWT（纯文本）
 *   2. 带 JWT 调用 api.sonjj.com 对应接口（payload={JWT}）
 * 创建邮箱、获取列表、获取详情均需先取 payload 再调用 sonjj API。
 * 本渠道不需要额外持久化 token，token 传空字符串即可。
 */

const smailproBaseURL = "https://smailpro.com"
const smailproAPIBaseURL = "https://api.sonjj.com/v1/temp_email"

/* smailproSetHeaders 设置 smailpro/sonjj 请求所需的通用请求头 */
func smailproSetHeaders(req *http.Request, referer string) {
	req.Header.Set("User-Agent", getCurrentUA())
	req.Header.Set("Accept", "application/json, text/plain, */*")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
	if referer != "" {
		req.Header.Set("Referer", referer)
	}
}

/* smailproFetchPayload 获取访问 sonjj API 所需的 JWT
 * targetURL 为目标 sonjj 接口地址（未编码），extra 为附加查询参数（email、mid 等）。
 */
func smailproFetchPayload(targetURL string, extra url.Values) (string, error) {
	client := HTTPClient()

	q := url.Values{}
	q.Set("url", targetURL)
	for k, vs := range extra {
		for _, v := range vs {
			q.Add(k, v)
		}
	}

	reqURL := smailproBaseURL + "/app/payload?" + q.Encode()
	req, err := http.NewRequest("GET", reqURL, nil)
	if err != nil {
		return "", fmt.Errorf("smailpro: 创建 payload 请求失败: %w", err)
	}
	smailproSetHeaders(req, smailproBaseURL+"/")

	resp, err := client.Do(req)
	if err != nil {
		return "", fmt.Errorf("smailpro: 获取 payload 失败: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", fmt.Errorf("smailpro: 读取 payload 响应失败: %w", err)
	}

	if err := CheckHTTPStatus(resp, "smailpro payload"); err != nil {
		return "", err
	}

	/* payload 接口返回纯文本 JWT，去除可能的引号与空白 */
	payload := strings.TrimSpace(string(body))
	payload = strings.Trim(payload, "\"")
	if payload == "" {
		return "", fmt.Errorf("smailpro: payload 为空")
	}
	return payload, nil
}

/* smailproCallAPI 携带 JWT 调用 sonjj API 并返回响应体
 * targetURL 为目标接口地址，extra 为获取 payload 时需要的附加参数（email、mid）。
 */
func smailproCallAPI(targetURL string, extra url.Values, label string) ([]byte, error) {
	payload, err := smailproFetchPayload(targetURL, extra)
	if err != nil {
		return nil, err
	}

	client := HTTPClient()

	q := url.Values{}
	q.Set("payload", payload)
	reqURL := targetURL + "?" + q.Encode()

	req, err := http.NewRequest("GET", reqURL, nil)
	if err != nil {
		return nil, fmt.Errorf("smailpro: 创建 %s 请求失败: %w", label, err)
	}
	smailproSetHeaders(req, smailproBaseURL+"/")

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("smailpro: %s 请求失败: %w", label, err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("smailpro: 读取 %s 响应失败: %w", label, err)
	}

	if err := CheckHTTPStatus(resp, "smailpro "+label); err != nil {
		return nil, err
	}
	return body, nil
}

/* SmailproGenerate 创建 smailpro 临时邮箱
 * 调用 sonjj create 接口，返回 {"email":"...","expired_at":...}
 */
func SmailproGenerate() (*CreatedMailbox, error) {
	body, err := smailproCallAPI(smailproAPIBaseURL+"/create", nil, "create")
	if err != nil {
		return nil, err
	}

	var createResp struct {
		Email     string `json:"email"`
		ExpiredAt any    `json:"expired_at"`
	}
	if err := json.Unmarshal(body, &createResp); err != nil {
		return nil, fmt.Errorf("smailpro: 解析创建响应失败: %w", err)
	}

	email := strings.TrimSpace(createResp.Email)
	if email == "" {
		return nil, fmt.Errorf("smailpro: 创建邮箱失败, 未返回邮箱地址")
	}

	return &CreatedMailbox{
		Channel:   "smailpro",
		Email:     email,
		Token:     "",
		ExpiresAt: createResp.ExpiredAt,
	}, nil
}

/* SmailproGetEmails 获取 smailpro 邮件列表
 * 1. 调用 sonjj inbox 接口获取列表（仅含 mid、from、subject、datetime）
 * 2. 对每封邮件调用 message 接口获取正文，再统一标准化
 * token 未使用，可传空字符串。
 */
func SmailproGetEmails(email, token string) ([]NormEmail, error) {
	_ = token
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("smailpro: 邮箱地址为空")
	}

	inboxExtra := url.Values{}
	inboxExtra.Set("email", email)

	body, err := smailproCallAPI(smailproAPIBaseURL+"/inbox", inboxExtra, "inbox")
	if err != nil {
		return nil, err
	}

	var inboxResp struct {
		Data struct {
			Messages []struct {
				Mid      string `json:"mid"`
				From     string `json:"from"`
				Subject  string `json:"subject"`
				Datetime string `json:"datetime"`
			} `json:"messages"`
		} `json:"data"`
	}
	if err := json.Unmarshal(body, &inboxResp); err != nil {
		return nil, fmt.Errorf("smailpro: 解析邮件列表失败: %w", err)
	}

	if len(inboxResp.Data.Messages) == 0 {
		return []NormEmail{}, nil
	}

	emails := make([]NormEmail, 0, len(inboxResp.Data.Messages))
	for _, m := range inboxResp.Data.Messages {
		mid := strings.TrimSpace(m.Mid)

		flat := map[string]interface{}{
			"id":      mid,
			"from":    m.From,
			"to":      email,
			"subject": m.Subject,
			"date":    m.Datetime,
		}

		/* 拉取邮件正文（含 HTML 与纯文本），失败时保留列表元信息 */
		if html, text := smailproFetchMessage(email, mid); html != "" || text != "" {
			flat["html"] = html
			flat["text"] = text
		}

		emails = append(emails, NormalizeMap(flat, email))
	}

	return emails, nil
}

/* smailproFetchMessage 获取单封邮件正文，返回 (html, text)
 * 调用 sonjj message 接口，需在 payload 参数中携带 email 与 mid。
 */
func smailproFetchMessage(email, mid string) (string, string) {
	if mid == "" {
		return "", ""
	}

	msgExtra := url.Values{}
	msgExtra.Set("email", email)
	msgExtra.Set("mid", mid)

	body, err := smailproCallAPI(smailproAPIBaseURL+"/message", msgExtra, "message")
	if err != nil {
		return "", ""
	}

	var msgResp struct {
		Body     string `json:"body"`
		TextBody string `json:"textBody"`
	}
	if err := json.Unmarshal(body, &msgResp); err != nil {
		return "", ""
	}
	return msgResp.Body, msgResp.TextBody
}
