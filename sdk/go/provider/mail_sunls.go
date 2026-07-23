package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"math/rand"
	"net/url"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

/**
 * MailSunls — https://mail.sunls.de
 * 纯 GET 无认证，随机生成用户名 + 从 API 获取的域名
 * 获取域名列表: GET https://mail.sunls.de/api/domain
 * 获取邮件列表: GET https://mail.sunls.de/api/fetch?to={email}
 * 获取单封邮件: GET https://mail.sunls.de/api/fetch/{id}
 */

const mailSunlsBase = "https://mail.sunls.de"

/* mailSunlsRandomLocal 生成随机用户名 */
func mailSunlsRandomLocal(n int) string {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	b := make([]byte, n)
	for i := range b {
		b[i] = chars[rand.Intn(len(chars))]
	}
	return string(b)
}

/* mailSunlsDefaultHeaders 设置通用请求头 */
func mailSunlsDefaultHeaders(req *http.Request) {
	req.Header.Set("Accept", "application/json, text/plain, */*")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
	req.Header.Set("Cache-Control", "no-cache")
	req.Header.Set("DNT", "1")
	req.Header.Set("Pragma", "no-cache")
	req.Header.Set("Referer", "https://mail.sunls.de/")
	req.Header.Set("User-Agent", getCurrentUA())
}

/* mailSunlsFetchDomains 获取可用域名列表 */
func mailSunlsFetchDomains() ([]string, error) {
	req, err := http.NewRequest("GET", mailSunlsBase+"/api/domain", nil)
	if err != nil {
		return nil, err
	}
	mailSunlsDefaultHeaders(req)

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
		return nil, fmt.Errorf("mail-sunls domains: %d", resp.StatusCode)
	}

	var domains []string
	if err := json.Unmarshal(body, &domains); err != nil {
		return nil, err
	}

	/* 过滤空字符串 */
	var result []string
	for _, d := range domains {
		s := strings.TrimSpace(d)
		if s != "" {
			result = append(result, s)
		}
	}
	if len(result) == 0 {
		return nil, fmt.Errorf("mail-sunls: no domains available")
	}
	return result, nil
}

/*
 * MailSunlsGenerate 创建 mail.sunls.de 临时邮箱
 * 1. GET /api/domain 获取可用域名列表
 * 2. 随机选取域名，生成 randomLocal(10) + "@" + 域名
 * 无需 token，无需 session
 */
func MailSunlsGenerate(channel ...string) (*CreatedMailbox, error) {
	domains, err := mailSunlsFetchDomains()
	if err != nil {
		return nil, err
	}
	domain := domains[rand.Intn(len(domains))]
	user := mailSunlsRandomLocal(10)
	email := user + "@" + domain

	ch := "mail-sunls"
	if len(channel) > 0 && channel[0] != "" {
		ch = channel[0]
	}
	return &CreatedMailbox{Channel: ch, Email: email, Token: ""}, nil
}

/*
 * mailSunlsFetchDetail 通过详情接口获取单封邮件完整正文
 * GET https://mail.sunls.de/api/fetch/{id}
 * 返回单封邮件的完整 map（含 text/html 字段），失败时返回 nil
 */
func mailSunlsFetchDetail(id string) map[string]interface{} {
	id = strings.TrimSpace(id)
	if id == "" {
		return nil
	}
	u := fmt.Sprintf("%s/api/fetch/%s", mailSunlsBase, url.PathEscape(id))
	req, err := http.NewRequest("GET", u, nil)
	if err != nil {
		return nil
	}
	mailSunlsDefaultHeaders(req)

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil
	}
	defer resp.Body.Close()
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil
	}
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil
	}
	var detail map[string]interface{}
	if err := json.Unmarshal(body, &detail); err != nil {
		return nil
	}
	return detail
}

/*
 * mailSunlsExtractID 从列表条目中提取邮件 ID
 * 支持多种字段名（id / _id / mail_id）和多种类型（string / float64）
 */
func mailSunlsExtractID(row map[string]interface{}) string {
	for _, key := range []string{"id", "_id", "mail_id", "messageId", "message_id"} {
		if v, ok := row[key]; ok && v != nil {
			switch val := v.(type) {
			case string:
				s := strings.TrimSpace(val)
				if s != "" {
					return s
				}
			case float64:
				return fmt.Sprintf("%.0f", val)
			case json.Number:
				return val.String()
			}
		}
	}
	return ""
}

/*
 * MailSunlsGetEmails 获取 mail.sunls.de 邮件列表
 * 流程:
 *   1. GET /api/fetch?to={email} 拉取邮件列表（含元数据）
 *   2. 对每封邮件 GET /api/fetch/{id} 拉取详情，用详情覆盖列表字段（text/html）
 * 详情接口失败时回退到列表数据，不阻断整体流程
 */
func MailSunlsGetEmails(email string) ([]NormEmail, error) {
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("mail-sunls: empty email")
	}

	u := fmt.Sprintf("%s/api/fetch?to=%s", mailSunlsBase, url.QueryEscape(email))
	req, err := http.NewRequest("GET", u, nil)
	if err != nil {
		return nil, err
	}
	mailSunlsDefaultHeaders(req)

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
		return nil, fmt.Errorf("mail-sunls fetch: %d", resp.StatusCode)
	}

	var rawMails []map[string]interface{}
	if err := json.Unmarshal(body, &rawMails); err != nil {
		return nil, err
	}

	/* 对每封邮件调用详情接口，用详情字段覆盖列表字段 */
	for i, row := range rawMails {
		id := mailSunlsExtractID(row)
		if id == "" {
			continue
		}
		detail := mailSunlsFetchDetail(id)
		if detail == nil {
			continue
		}
		/* 详情响应字段合并到列表条目：详情优先（含完整 text/html） */
		for k, v := range detail {
			if v == nil {
				continue
			}
			rawMails[i][k] = v
		}
	}

	return normEmailsFromMaps(rawMails, email), nil
}
