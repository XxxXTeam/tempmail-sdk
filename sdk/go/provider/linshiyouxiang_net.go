package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"regexp"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

/**
 * LinshiyouxiangNet — https://www.linshiyouxiang.net
 * 创建邮箱: GET / 获取 temp_mail cookie，从 HTML 正则提取 tempMailGlobal(邮箱) 和 mailCodeGlobal(校验 code)
 * 获取邮件: POST /get-messages  body: {"email":"...","code":"..."}
 * token 存储 mailCodeGlobal 的值（HMAC 哈希，后续请求用于校验）
 */

const linshiyouxiangNetBase = "https://www.linshiyouxiang.net"

/* linshiyouxiangNetEmailRe 提取首页 HTML 中的 tempMailGlobal（邮箱地址） */
var linshiyouxiangNetEmailRe = regexp.MustCompile(`tempMailGlobal\s*=\s*'([^']+)'`)

/* linshiyouxiangNetCodeRe 提取首页 HTML 中的 mailCodeGlobal（校验 code） */
var linshiyouxiangNetCodeRe = regexp.MustCompile(`mailCodeGlobal\s*=\s*'([^']+)'`)

/* linshiyouxiangNetHeaders 设置请求头 */
func linshiyouxiangNetHeaders(req *http.Request) {
	req.Header.Set("Accept", "application/json, text/javascript, */*; q=0.01")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
	req.Header.Set("Referer", linshiyouxiangNetBase+"/")
	req.Header.Set("Origin", linshiyouxiangNetBase)
	req.Header.Set("User-Agent", getCurrentUA())
}

/*
 * LinshiyouxiangNetGenerate 创建 linshiyouxiang.net 临时邮箱
 * 流程:
 *   1. GET / 获取 temp_mail cookie 及首页 HTML
 *   2. 从 HTML 正则提取 tempMailGlobal（邮箱）和 mailCodeGlobal（校验 code）
 *   3. 邮箱来自 tempMailGlobal，token 存储 mailCodeGlobal
 */
func LinshiyouxiangNetGenerate(channel ...string) (*CreatedMailbox, error) {
	req, err := http.NewRequest("GET", linshiyouxiangNetBase+"/", nil)
	if err != nil {
		return nil, fmt.Errorf("linshiyouxiang-net: 创建请求失败: %w", err)
	}
	req.Header.Set("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
	req.Header.Set("User-Agent", getCurrentUA())

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, fmt.Errorf("linshiyouxiang-net: 请求首页失败: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("linshiyouxiang-net: 读取首页失败: %w", err)
	}

	if err := CheckHTTPStatus(resp, "linshiyouxiang-net index"); err != nil {
		return nil, err
	}

	html := string(body)

	/* 提取邮箱地址 */
	emailMatch := linshiyouxiangNetEmailRe.FindStringSubmatch(html)
	if len(emailMatch) < 2 {
		return nil, fmt.Errorf("linshiyouxiang-net: 未能从首页提取邮箱地址")
	}
	email := strings.TrimSpace(emailMatch[1])
	if email == "" {
		return nil, fmt.Errorf("linshiyouxiang-net: 提取的邮箱地址为空")
	}

	/* 提取校验 code */
	var code string
	if codeMatch := linshiyouxiangNetCodeRe.FindStringSubmatch(html); len(codeMatch) >= 2 {
		code = strings.TrimSpace(codeMatch[1])
	}

	ch := "linshiyouxiang-net"
	if len(channel) > 0 && channel[0] != "" {
		ch = channel[0]
	}

	return &CreatedMailbox{
		Channel: ch,
		Email:   email,
		Token:   code,
	}, nil
}

/*
 * LinshiyouxiangNetGetEmails 获取 linshiyouxiang.net 邮件列表
 * 流程:
 *   1. POST /get-messages  body: {"email":"...","code":"<token>"}
 *   2. 解析响应 {"emails":null|[...],"success":true}
 *   3. 将邮件列表归一化
 */
func LinshiyouxiangNetGetEmails(email, token string) ([]NormEmail, error) {
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("linshiyouxiang-net: 邮箱地址为空")
	}

	/* 构造请求体 */
	reqBody, err := json.Marshal(map[string]string{
		"email": email,
		"code":  token,
	})
	if err != nil {
		return nil, fmt.Errorf("linshiyouxiang-net: 序列化请求体失败: %w", err)
	}

	req, err := http.NewRequest("POST", linshiyouxiangNetBase+"/get-messages", strings.NewReader(string(reqBody)))
	if err != nil {
		return nil, fmt.Errorf("linshiyouxiang-net: 创建请求失败: %w", err)
	}
	linshiyouxiangNetHeaders(req)
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-Requested-With", "XMLHttpRequest")

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, fmt.Errorf("linshiyouxiang-net: 请求获取邮件失败: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("linshiyouxiang-net: 读取响应失败: %w", err)
	}

	if err := CheckHTTPStatus(resp, "linshiyouxiang-net get-messages"); err != nil {
		return nil, err
	}

	/* 解析响应: {"emails":null,"success":true} 或 {"emails":[...],"success":true} */
	var result struct {
		Emails  []map[string]interface{} `json:"emails"`
		Success bool                     `json:"success"`
	}
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, fmt.Errorf("linshiyouxiang-net: 解析邮件列表失败: %w", err)
	}

	if len(result.Emails) == 0 {
		return []NormEmail{}, nil
	}

	return normEmailsFromMaps(result.Emails, email), nil
}
