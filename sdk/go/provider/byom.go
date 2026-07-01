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
 * Byom — https://byom.de
 * 纯 GET 无认证，直接生成随机用户名 + @byom.de
 * 获取邮件: GET https://api.byom.de/mails/{username}
 */

const byomDomain = "byom.de"
const byomAPIBase = "https://api.byom.de"

/* byomRandomLocal 生成随机用户名 */
func byomRandomLocal(n int) string {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	b := make([]byte, n)
	for i := range b {
		b[i] = chars[rand.Intn(len(chars))]
	}
	return string(b)
}

/* byomDefaultHeaders 设置请求头 */
func byomDefaultHeaders(req *http.Request) {
	req.Header.Set("Accept", "application/json, text/plain, */*")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
	req.Header.Set("Cache-Control", "no-cache")
	req.Header.Set("DNT", "1")
	req.Header.Set("Pragma", "no-cache")
	req.Header.Set("Referer", "https://byom.de/")
	req.Header.Set("User-Agent", getCurrentUA())
}

/*
 * ByomGenerate 创建 byom.de 临时邮箱
 * 无需 API 调用，直接生成随机用户名 + "@byom.de"
 */
func ByomGenerate(channel ...string) (*CreatedMailbox, error) {
	user := byomRandomLocal(10)
	email := user + "@" + byomDomain
	ch := "byom"
	if len(channel) > 0 && channel[0] != "" {
		ch = channel[0]
	}
	return &CreatedMailbox{Channel: ch, Email: email, Token: ""}, nil
}

/*
 * ByomGetEmails 获取 byom.de 邮件列表
 * GET https://api.byom.de/mails/{username}
 */
func ByomGetEmails(email string) ([]NormEmail, error) {
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("byom: empty email")
	}
	/* 提取 @ 前面的用户名部分 */
	parts := strings.SplitN(email, "@", 2)
	username := parts[0]

	u := fmt.Sprintf("%s/mails/%s", byomAPIBase, username)
	req, err := http.NewRequest("GET", u, nil)
	if err != nil {
		return nil, err
	}
	byomDefaultHeaders(req)

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
		return nil, fmt.Errorf("byom mails: %d", resp.StatusCode)
	}

	var rawMails []map[string]interface{}
	if err := json.Unmarshal(body, &rawMails); err != nil {
		return nil, err
	}
	return normEmailsFromMaps(rawMails, email), nil
}
