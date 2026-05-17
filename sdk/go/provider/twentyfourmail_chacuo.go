package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"math/rand"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

// 24mail.chacuo.net：POST 注册/刷新同一接口，HTTP only。

const chacuoBaseURL = "http://24mail.chacuo.net"

var chacuoDomains = []string{"chacuo.net", "027168.com"}

func chacuoRandomLocal(length int) string {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	buf := make([]byte, length)
	for i := range buf {
		buf[i] = chars[rand.Intn(len(chars))]
	}
	return string(buf)
}

func chacuoSetHeaders(req *http.Request) {
	req.Header.Set("Accept", "application/json, text/javascript, */*; q=0.01")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6")
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded; charset=UTF-8")
	req.Header.Set("Origin", chacuoBaseURL)
	req.Header.Set("Referer", chacuoBaseURL+"/")
	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0")
	req.Header.Set("x-requested-with", "XMLHttpRequest")
}

// TwentyfourmailChacuoGenerate 创建 24mail-chacuo 临时邮箱
func TwentyfourmailChacuoGenerate() (*CreatedMailbox, error) {
	name := chacuoRandomLocal(10)
	domain := chacuoDomains[rand.Intn(len(chacuoDomains))]

	body := fmt.Sprintf("data=%s%%40%s&type=refresh&arg=", name, domain)
	req, err := http.NewRequest("POST", chacuoBaseURL+"/", strings.NewReader(body))
	if err != nil {
		return nil, err
	}
	chacuoSetHeaders(req)

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if err := CheckHTTPStatus(resp, "24mail-chacuo generate"); err != nil {
		return nil, err
	}
	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	var data struct {
		Status int    `json:"status"`
		Info   string `json:"info"`
	}
	if err := json.Unmarshal(raw, &data); err != nil {
		return nil, fmt.Errorf("24mail-chacuo: 返回非 JSON: %s", string(raw[:min(len(raw), 120)]))
	}
	if data.Status != 1 {
		return nil, fmt.Errorf("24mail-chacuo: 创建失败 status=%d info=%s", data.Status, data.Info)
	}

	email := fmt.Sprintf("%s@%s", name, domain)
	return &CreatedMailbox{
		Channel: "24mail-chacuo",
		Email:   email,
		Token:   email,
	}, nil
}

// TwentyfourmailChacuoGetEmails 获取 24mail-chacuo 邮件列表
func TwentyfourmailChacuoGetEmails(email string) ([]NormEmail, error) {
	atIdx := strings.Index(email, "@")
	name := email
	domain := chacuoDomains[0]
	if atIdx > 0 {
		name = email[:atIdx]
		domain = email[atIdx+1:]
	}

	body := fmt.Sprintf("data=%s%%40%s&type=refresh&arg=", name, domain)
	req, err := http.NewRequest("POST", chacuoBaseURL+"/", strings.NewReader(body))
	if err != nil {
		return nil, err
	}
	chacuoSetHeaders(req)

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if err := CheckHTTPStatus(resp, "24mail-chacuo getEmails"); err != nil {
		return nil, err
	}
	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	var data struct {
		Status int `json:"status"`
		Data   []struct {
			List []struct {
				MID     string `json:"MID"`
				FROM    string `json:"FROM"`
				SUBJECT string `json:"SUBJECT"`
				CONTENT string `json:"CONTENT"`
				TIME    string `json:"TIME"`
			} `json:"list"`
		} `json:"data"`
	}
	if err := json.Unmarshal(raw, &data); err != nil {
		return nil, err
	}
	if data.Status != 1 || len(data.Data) == 0 {
		return []NormEmail{}, nil
	}

	list := data.Data[0].List
	emails := make([]NormEmail, 0, len(list))
	for _, item := range list {
		flat := map[string]interface{}{
			"id":      item.MID,
			"from":    item.FROM,
			"to":      email,
			"subject": item.SUBJECT,
			"body":    item.CONTENT,
			"date":    item.TIME,
			"isRead":  false,
		}
		emails = append(emails, NormalizeMap(flat, email))
	}
	return emails, nil
}
