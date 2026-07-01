package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"math/rand"
	"net/url"
	"regexp"
	"strings"
	"time"

	http "github.com/bogdanfinn/fhttp"
)

const tempmail365Base = "https://tempmail365.cn/tempemail.php"

/* tempmail365 可用域名列表 */
var tempmail365Domains = []string{"fengyou.cc", "shop345.com", "nutemail.com", "qvrf.cn"}

/* tempmail365 随机生成 8 位字母数字用户名 */
func tempmail365RandomUser() string {
	const charset = "abcdefghijklmnopqrstuvwxyz0123456789"
	b := make([]byte, 8)
	for i := range b {
		b[i] = charset[rand.Intn(len(charset))]
	}
	return string(b)
}

type tempmail365ConfigResp struct {
	Domains []string `json:"domains"`
}

type tempmail365CreateResp struct {
	Success bool `json:"success"`
}

type tempmail365FetchResp struct {
	Content string `json:"content"`
}

/* 从 HTML content 中提取发件人 */
var tempmail365FromRe = regexp.MustCompile(`(?:发件人|From)\s*[:：]\s*(.+?)(?:<br|<BR|\n|$)`)

/* 从 HTML content 中提取主题 */
var tempmail365SubjectRe = regexp.MustCompile(`(?:主题|Subject)\s*[:：]\s*(.+?)(?:<br|<BR|\n|$)`)

/*
 * Tempmail365Generate 创建 tempmail365 临时邮箱
 * 流程：获取域名列表 → 随机选域名 → 随机用户名 → create_email
 */
func Tempmail365Generate(channel ...string) (*CreatedMailbox, error) {
	/* 获取域名列表 */
	configURL := tempmail365Base + "?action=get_config"
	req, err := http.NewRequest("GET", configURL, nil)
	if err != nil {
		return nil, err
	}
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
		return nil, fmt.Errorf("tempmail365 get_config: %d", resp.StatusCode)
	}
	var cr tempmail365ConfigResp
	if err := json.Unmarshal(body, &cr); err != nil {
		return nil, err
	}

	/* 使用 API 返回的域名列表，如果为空则回退到硬编码域名 */
	domains := cr.Domains
	if len(domains) == 0 {
		domains = tempmail365Domains
	}

	/* 随机选择域名 */
	domain := domains[rand.Intn(len(domains))]
	user := tempmail365RandomUser()
	email := user + "@" + domain

	/* 创建邮箱 */
	createURL := fmt.Sprintf("%s?action=create_email&email=%s&domain=%s",
		tempmail365Base, url.QueryEscape(email), url.QueryEscape(domain))
	req2, err := http.NewRequest("GET", createURL, nil)
	if err != nil {
		return nil, err
	}
	resp2, err := HTTPClient().Do(req2)
	if err != nil {
		return nil, err
	}
	defer resp2.Body.Close()
	body2, err := io.ReadAll(resp2.Body)
	if err != nil {
		return nil, err
	}
	if resp2.StatusCode < 200 || resp2.StatusCode >= 300 {
		return nil, fmt.Errorf("tempmail365 create_email: %d", resp2.StatusCode)
	}
	var createResp tempmail365CreateResp
	if err := json.Unmarshal(body2, &createResp); err != nil {
		return nil, err
	}
	if !createResp.Success {
		return nil, fmt.Errorf("tempmail365: 创建邮箱失败")
	}

	ch := "tempmail365"
	if len(channel) > 0 && channel[0] != "" {
		ch = channel[0]
	}
	return &CreatedMailbox{Channel: ch, Email: email, Token: ""}, nil
}

/*
 * Tempmail365GetEmails 获取 tempmail365 邮件列表
 * 流程：fetch_mail → 判断是否有邮件 → 提取发件人/主题 → 构造邮件对象 → normalize
 */
func Tempmail365GetEmails(email string) ([]NormEmail, error) {
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("tempmail365: 邮箱地址为空")
	}

	fetchURL := fmt.Sprintf("%s?action=fetch_mail&email=%s",
		tempmail365Base, url.QueryEscape(email))
	req, err := http.NewRequest("GET", fetchURL, nil)
	if err != nil {
		return nil, err
	}
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
		return nil, fmt.Errorf("tempmail365 fetch_mail: %d", resp.StatusCode)
	}
	var fr tempmail365FetchResp
	if err := json.Unmarshal(body, &fr); err != nil {
		return nil, err
	}

	/* 无邮件时返回空列表 */
	if fr.Content == "无邮件" || strings.TrimSpace(fr.Content) == "" {
		return []NormEmail{}, nil
	}

	/* 从 HTML content 中提取发件人和主题 */
	from := ""
	if m := tempmail365FromRe.FindStringSubmatch(fr.Content); len(m) > 1 {
		from = strings.TrimSpace(m[1])
	}
	subject := ""
	if m := tempmail365SubjectRe.FindStringSubmatch(fr.Content); len(m) > 1 {
		subject = strings.TrimSpace(m[1])
	}

	/* 构造邮件 map 并通过 normalize 处理 */
	raw := map[string]interface{}{
		"from":    from,
		"subject": subject,
		"html":    fr.Content,
		"date":    time.Now().UTC().Format(time.RFC3339),
	}
	normalized := NormalizeMap(raw, email)
	return []NormEmail{normalized}, nil
}
