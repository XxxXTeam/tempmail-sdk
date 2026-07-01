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
 * Pleasenospam — https://pleasenospam.email
 * 无需 API 调用即可创建邮箱，直接生成随机用户名 + @pleasenospam.email
 * 获取邮件: GET https://pleasenospam.email/{email}.json
 * 返回 JSON 数组，from 字段为数组类型，取 from[0] 作为发件人
 */

const pleasenospamDomain = "pleasenospam.email"
const pleasenospamBaseURL = "https://pleasenospam.email"

/* pleasenospamRandomLocal 生成随机用户名 */
func pleasenospamRandomLocal(n int) string {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	b := make([]byte, n)
	for i := range b {
		b[i] = chars[rand.Intn(len(chars))]
	}
	return string(b)
}

/* pleasenospamDefaultHeaders 设置请求头 */
func pleasenospamDefaultHeaders(req *http.Request) {
	req.Header.Set("Accept", "application/json, text/plain, */*")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
	req.Header.Set("Cache-Control", "no-cache")
	req.Header.Set("DNT", "1")
	req.Header.Set("Pragma", "no-cache")
	req.Header.Set("Referer", pleasenospamBaseURL+"/")
	req.Header.Set("User-Agent", getCurrentUA())
}

/*
 * PleasenospamGenerate 创建 pleasenospam.email 临时邮箱
 * 无需 API 调用，直接生成随机用户名 + "@pleasenospam.email"
 */
func PleasenospamGenerate(channel ...string) (*CreatedMailbox, error) {
	user := pleasenospamRandomLocal(10)
	email := user + "@" + pleasenospamDomain
	ch := "pleasenospam"
	if len(channel) > 0 && channel[0] != "" {
		ch = channel[0]
	}
	return &CreatedMailbox{Channel: ch, Email: email, Token: ""}, nil
}

/*
 * pleasenospamExtractFrom 从 from 字段提取发件人地址
 * pleasenospam 的 from 字段是字符串数组，取第一个元素作为发件人
 * 兼容 from 为普通字符串的情况
 */
func pleasenospamExtractFrom(raw map[string]interface{}) string {
	v, ok := raw["from"]
	if !ok || v == nil {
		return ""
	}
	/* from 为数组时取第一个元素 */
	if arr, ok := v.([]interface{}); ok && len(arr) > 0 {
		if s, ok := arr[0].(string); ok {
			return s
		}
	}
	/* from 为字符串时直接返回 */
	if s, ok := v.(string); ok {
		return s
	}
	return ""
}

/*
 * PleasenospamGetEmails 获取 pleasenospam.email 邮件列表
 * GET https://pleasenospam.email/{email}.json
 * 返回 JSON 数组，空邮箱返回 []
 */
func PleasenospamGetEmails(email string) ([]NormEmail, error) {
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("pleasenospam: empty email")
	}

	u := fmt.Sprintf("%s/%s.json", pleasenospamBaseURL, email)
	req, err := http.NewRequest("GET", u, nil)
	if err != nil {
		return nil, err
	}
	pleasenospamDefaultHeaders(req)

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
		return nil, fmt.Errorf("pleasenospam mails: %d", resp.StatusCode)
	}

	var rawMails []map[string]interface{}
	if err := json.Unmarshal(body, &rawMails); err != nil {
		return nil, err
	}

	/* 将 from 数组提取为字符串，写回 map 以便 normEmailsFromMaps 正确归一化 */
	for _, m := range rawMails {
		fromStr := pleasenospamExtractFrom(m)
		if fromStr != "" {
			m["from"] = fromStr
		}
		/* receivedDate 为秒级 Unix 时间戳，映射到 date 字段以便归一化 */
		if rd, ok := m["receivedDate"]; ok && rd != nil {
			m["date"] = rd
		}
	}

	return normEmailsFromMaps(rawMails, email), nil
}
