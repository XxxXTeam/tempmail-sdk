package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"net/url"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

/*
 * mailtemp.cc 临时邮箱服务商（PHP POST form-urlencoded API）
 * 域名: @neocea.com
 * 创建邮箱: POST https://mailtemp.cc/api.php  body: action=inbox  返回 JSON 字符串（用户名）
 * 获取邮件列表: POST https://mailtemp.cc/api.php  body: action=fetch&inbox={username}&last_id=0
 * 查看邮件详情: POST https://mailtemp.cc/api.php  body: action=view&id={id}&inbox={username}
 * token 存储: username（@前面的部分）
 */

const mailtempCcAPIURL = "https://mailtemp.cc/api.php"

/* mailtempCcHeaders 设置请求头 */
func mailtempCcHeaders(req *http.Request) {
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	req.Header.Set("Accept", "application/json, text/plain, */*")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
	req.Header.Set("Referer", "https://mailtemp.cc/")
	req.Header.Set("Origin", "https://mailtemp.cc")
	req.Header.Set("User-Agent", getCurrentUA())
}

/*
 * MailtempCcGenerate 创建 mailtemp.cc 临时邮箱
 * 流程:
 *   1. POST api.php body: action=inbox
 *   2. 返回值为 JSON 字符串（带引号），如 "vindictiverate"，需 json.Unmarshal 去掉引号
 *   3. 完整邮箱地址: {username}@neocea.com
 * token: 存储 username
 */
func MailtempCcGenerate(channel ...string) (*CreatedMailbox, error) {
	form := url.Values{}
	form.Set("action", "inbox")

	req, err := http.NewRequest("POST", mailtempCcAPIURL, strings.NewReader(form.Encode()))
	if err != nil {
		return nil, fmt.Errorf("mailtemp-cc: 创建请求失败: %w", err)
	}
	mailtempCcHeaders(req)

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, fmt.Errorf("mailtemp-cc: 请求创建邮箱失败: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("mailtemp-cc: 读取响应失败: %w", err)
	}

	if err := CheckHTTPStatus(resp, "mailtemp-cc inbox"); err != nil {
		return nil, err
	}

	/* 返回值为 JSON 字符串格式（带引号），使用 json.Unmarshal 解析去掉引号 */
	var username string
	if err := json.Unmarshal(body, &username); err != nil {
		return nil, fmt.Errorf("mailtemp-cc: 解析用户名失败（原始响应: %s）: %w", string(body), err)
	}

	username = strings.TrimSpace(username)
	if username == "" {
		return nil, fmt.Errorf("mailtemp-cc: 返回的用户名为空")
	}

	emailAddr := username + "@neocea.com"

	ch := "mailtemp-cc"
	if len(channel) > 0 && channel[0] != "" {
		ch = channel[0]
	}

	return &CreatedMailbox{
		Channel: ch,
		Email:   emailAddr,
		Token:   username,
	}, nil
}

/*
 * MailtempCcGetEmails 获取 mailtemp.cc 邮件列表
 * 流程:
 *   1. POST api.php body: action=fetch&inbox={token}&last_id=0 获取邮件列表
 *   2. 对每封邮件 POST api.php body: action=view&id={id}&inbox={token} 获取 body_html
 *   3. 将详情中的 body_html 合入邮件数据后归一化
 */
func MailtempCcGetEmails(email, token string) ([]NormEmail, error) {
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("mailtemp-cc: 邮箱地址为空")
	}
	token = strings.TrimSpace(token)
	if token == "" {
		return nil, fmt.Errorf("mailtemp-cc: token 为空")
	}

	/* 步骤 1：获取邮件列表 */
	form := url.Values{}
	form.Set("action", "fetch")
	form.Set("inbox", token)
	form.Set("last_id", "0")

	req, err := http.NewRequest("POST", mailtempCcAPIURL, strings.NewReader(form.Encode()))
	if err != nil {
		return nil, fmt.Errorf("mailtemp-cc: 创建获取邮件请求失败: %w", err)
	}
	mailtempCcHeaders(req)

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, fmt.Errorf("mailtemp-cc: 请求获取邮件失败: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("mailtemp-cc: 读取邮件列表响应失败: %w", err)
	}

	if err := CheckHTTPStatus(resp, "mailtemp-cc fetch"); err != nil {
		return nil, err
	}

	/* 解析邮件列表 JSON 数组 */
	var rawItems []map[string]interface{}
	if err := json.Unmarshal(body, &rawItems); err != nil {
		trimmed := strings.TrimSpace(string(body))
		if trimmed == "[]" || trimmed == "" || trimmed == "null" {
			return []NormEmail{}, nil
		}
		return nil, fmt.Errorf("mailtemp-cc: 解析邮件列表失败: %w", err)
	}

	if len(rawItems) == 0 {
		return []NormEmail{}, nil
	}

	/* 步骤 2：逐封获取邮件详情以取得 body_html */
	emails := make([]NormEmail, 0, len(rawItems))
	for _, item := range rawItems {
		/* 提取邮件 ID */
		mailID := ""
		if v, ok := item["id"]; ok {
			mailID = fmt.Sprintf("%v", v)
		}

		/* 获取邮件详情 */
		if mailID != "" {
			detail, detailErr := mailtempCcViewEmail(token, mailID)
			if detailErr == nil && detail != nil {
				/* 将详情中的 body_html 合入 */
				if htmlBody, ok := detail["body_html"]; ok {
					item["html"] = htmlBody
					item["body_html"] = htmlBody
				}
			}
		}

		/* 映射 sender_email → from 以便归一化正确识别 */
		if _, ok := item["from"]; !ok {
			if senderEmail, ok := item["sender_email"]; ok {
				item["from"] = senderEmail
			} else if sender, ok := item["sender"]; ok {
				item["from"] = sender
			}
		}

		/* 映射 received_at → date */
		if _, ok := item["date"]; !ok {
			if receivedAt, ok := item["received_at"]; ok {
				item["date"] = receivedAt
			}
		}

		item["to"] = email
		emails = append(emails, NormalizeMap(item, email))
	}

	return emails, nil
}

/* mailtempCcViewEmail 获取单封邮件详情
 * POST api.php body: action=view&id={id}&inbox={username}
 * 返回 JSON 对象 {id, subject, sender, sender_email, received_at, body_html, advertisement}
 */
func mailtempCcViewEmail(inbox, mailID string) (map[string]interface{}, error) {
	form := url.Values{}
	form.Set("action", "view")
	form.Set("id", mailID)
	form.Set("inbox", inbox)

	req, err := http.NewRequest("POST", mailtempCcAPIURL, strings.NewReader(form.Encode()))
	if err != nil {
		return nil, fmt.Errorf("mailtemp-cc: 创建查看邮件请求失败: %w", err)
	}
	mailtempCcHeaders(req)

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, fmt.Errorf("mailtemp-cc: 请求查看邮件失败: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("mailtemp-cc: 读取邮件详情响应失败: %w", err)
	}

	if err := CheckHTTPStatus(resp, "mailtemp-cc view"); err != nil {
		return nil, err
	}

	var detail map[string]interface{}
	if err := json.Unmarshal(body, &detail); err != nil {
		return nil, fmt.Errorf("mailtemp-cc: 解析邮件详情失败: %w", err)
	}

	return detail, nil
}
