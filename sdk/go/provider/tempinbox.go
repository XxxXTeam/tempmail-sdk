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

const tempinboxBase = "https://endpoint.tempinbox.xyz"

/* tempinbox 可用域名列表 */
var tempinboxDomains = []string{"tempinbox.xyz", "thepiratebay.cloud", "cryptoblad.nl"}

func tempinboxDefaultHeaders(req *http.Request) {
	req.Header.Set("Accept", "application/json, text/plain, */*")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
	req.Header.Set("Cache-Control", "no-cache")
	req.Header.Set("DNT", "1")
	req.Header.Set("Pragma", "no-cache")
	req.Header.Set("Referer", "https://www.tempinbox.xyz/")
	req.Header.Set("sec-ch-ua", `"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"`)
	req.Header.Set("sec-ch-ua-mobile", "?0")
	req.Header.Set("sec-ch-ua-platform", `"Windows"`)
	req.Header.Set("sec-fetch-dest", "empty")
	req.Header.Set("sec-fetch-mode", "cors")
	req.Header.Set("sec-fetch-site", "cross-site")
	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0")
}

/* tempinboxPickDomain 选择域名：用户指定则校验后返回，否则随机选取 */
func tempinboxPickDomain(preferred *string) (string, error) {
	if preferred != nil {
		p := strings.TrimSpace(*preferred)
		if p != "" {
			pl := strings.ToLower(p)
			for _, d := range tempinboxDomains {
				if strings.ToLower(d) == pl {
					return d, nil
				}
			}
			return "", fmt.Errorf("tempinbox: domain not available: %s", pl)
		}
	}
	return tempinboxDomains[rand.Intn(len(tempinboxDomains))], nil
}

/* tempinboxRandomUser 生成随机用户名前缀（8-12 位小写字母+数字） */
func tempinboxRandomUser() string {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	length := 8 + rand.Intn(5) // 8~12
	b := make([]byte, length)
	for i := range b {
		b[i] = chars[rand.Intn(len(chars))]
	}
	return string(b)
}

/*
 * TempinboxGenerate 创建 tempinbox 临时邮箱
 * domain 为 nil 时使用 /email/Random 获取随机邮箱
 * domain 非 nil 时生成随机用户名并拼接指定域名调用 /email/{user}@{domain}
 */
func TempinboxGenerate(domain *string, channel ...string) (*CreatedMailbox, error) {
	var email string

	if domain != nil && strings.TrimSpace(*domain) != "" {
		/* 用户指定了域名，校验后构造自定义邮箱 */
		d, err := tempinboxPickDomain(domain)
		if err != nil {
			return nil, err
		}
		user := tempinboxRandomUser()
		addr := user + "@" + d
		u := fmt.Sprintf("%s/email/%s", tempinboxBase, url.PathEscape(addr))
		req, err := http.NewRequest("GET", u, nil)
		if err != nil {
			return nil, err
		}
		tempinboxDefaultHeaders(req)
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
			return nil, fmt.Errorf("tempinbox create email: %d %s", resp.StatusCode, string(body))
		}
		/* 响应为带引号的纯字符串，如 "user@domain" */
		email = strings.Trim(strings.TrimSpace(string(body)), `"`)
	} else {
		/* 未指定域名，调用 /email/Random 获取随机邮箱 */
		req, err := http.NewRequest("GET", tempinboxBase+"/email/Random", nil)
		if err != nil {
			return nil, err
		}
		tempinboxDefaultHeaders(req)
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
			return nil, fmt.Errorf("tempinbox random email: %d %s", resp.StatusCode, string(body))
		}
		/* 响应为带引号的纯字符串，如 "user@domain" */
		email = strings.Trim(strings.TrimSpace(string(body)), `"`)
	}

	if email == "" || !strings.Contains(email, "@") {
		return nil, fmt.Errorf("tempinbox: invalid email response")
	}

	ch := "tempinbox"
	if len(channel) > 0 && channel[0] != "" {
		ch = channel[0]
	}
	return &CreatedMailbox{Channel: ch, Email: email, Token: ""}, nil
}

/*
 * TempinboxGetEmails 获取 tempinbox 邮件列表
 * 调用 GET /messages/{email}，对返回的每封邮件通过 NormalizeMap 标准化
 */
func TempinboxGetEmails(email string) ([]NormEmail, error) {
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("tempinbox: empty email")
	}
	seg := url.PathEscape(email)
	u := fmt.Sprintf("%s/messages/%s", tempinboxBase, seg)
	req, err := http.NewRequest("GET", u, nil)
	if err != nil {
		return nil, err
	}
	tempinboxDefaultHeaders(req)
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
		return nil, fmt.Errorf("tempinbox messages: %d", resp.StatusCode)
	}
	var rawList []map[string]interface{}
	if err := json.Unmarshal(body, &rawList); err != nil {
		return nil, err
	}
	emails := make([]NormEmail, 0, len(rawList))
	for _, raw := range rawList {
		emails = append(emails, NormalizeMap(raw, email))
	}
	return emails, nil
}
