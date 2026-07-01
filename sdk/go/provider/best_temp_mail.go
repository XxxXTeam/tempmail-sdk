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
 * BestTempMail -- https://best-temp-mail.com
 * 纯 JSON REST API，无需认证，无需 Session
 * 创建邮箱: POST /api/v3/createEmail  body: {"intToken":"<uuid>"}
 * 获取邮件: POST /api/v3/getEmailList  body: {"address":"...","id":"...","intToken":"...","update_tag":"..."}
 */

const bestTempMailAPIBase = "https://best-temp-mail.com/api/v3"

/* bestTempMailToken 存储在 token 字段中的 JSON 结构 */
type bestTempMailToken struct {
	IntToken  string `json:"intToken"`
	ID        string `json:"id"`
	UpdateTag string `json:"update_tag"`
}

/* bestTempMailGenUUID 生成 UUID v4 格式字符串 */
func bestTempMailGenUUID() string {
	return fmt.Sprintf("%08x-%04x-%04x-%04x-%012x",
		rand.Uint32(),
		rand.Uint32()&0xffff,
		(rand.Uint32()&0x0fff)|0x4000,
		(rand.Uint32()&0x3fff)|0x8000,
		rand.Uint64()&0xffffffffffff,
	)
}

/* bestTempMailHeaders 设置请求头 */
func bestTempMailHeaders(req *http.Request) {
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Accept", "application/json, text/plain, */*")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
	req.Header.Set("Referer", "https://best-temp-mail.com/")
	req.Header.Set("Origin", "https://best-temp-mail.com")
	req.Header.Set("User-Agent", getCurrentUA())
}

/*
 * BestTempMailGenerate 创建 best-temp-mail.com 临时邮箱
 * 流程:
 *   1. 客户端生成 UUID 作为 intToken
 *   2. POST /api/v3/createEmail {"intToken":"<uuid>"}
 *   3. 解析响应中的 address、id、update_tag
 *   4. 将 intToken + id + update_tag 序列化为 JSON 存入 token
 */
func BestTempMailGenerate(channel ...string) (*CreatedMailbox, error) {
	intToken := bestTempMailGenUUID()

	/* 构造请求体 */
	reqBody, err := json.Marshal(map[string]string{"intToken": intToken})
	if err != nil {
		return nil, fmt.Errorf("best-temp-mail: 序列化请求体失败: %w", err)
	}

	req, err := http.NewRequest("POST", bestTempMailAPIBase+"/createEmail", strings.NewReader(string(reqBody)))
	if err != nil {
		return nil, fmt.Errorf("best-temp-mail: 创建请求失败: %w", err)
	}
	bestTempMailHeaders(req)

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, fmt.Errorf("best-temp-mail: 请求创建邮箱失败: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("best-temp-mail: 读取响应失败: %w", err)
	}

	if err := CheckHTTPStatus(resp, "best-temp-mail createEmail"); err != nil {
		return nil, err
	}

	/* 解析响应: {"data":{"address":"...","id":"...","update_tag":"..."},"status":"success","t":1} */
	var result struct {
		Status string `json:"status"`
		Data   struct {
			Address   string `json:"address"`
			ID        string `json:"id"`
			UpdateTag string `json:"update_tag"`
		} `json:"data"`
	}
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, fmt.Errorf("best-temp-mail: 解析响应失败: %w", err)
	}

	if result.Status != "success" || result.Data.Address == "" {
		return nil, fmt.Errorf("best-temp-mail: 创建邮箱失败, status=%s, body=%s", result.Status, string(body))
	}

	/* 序列化 token */
	tokenData := bestTempMailToken{
		IntToken:  intToken,
		ID:        result.Data.ID,
		UpdateTag: result.Data.UpdateTag,
	}
	tokenJSON, err := json.Marshal(tokenData)
	if err != nil {
		return nil, fmt.Errorf("best-temp-mail: 序列化 token 失败: %w", err)
	}

	ch := "best-temp-mail"
	if len(channel) > 0 && channel[0] != "" {
		ch = channel[0]
	}

	return &CreatedMailbox{
		Channel: ch,
		Email:   result.Data.Address,
		Token:   string(tokenJSON),
	}, nil
}

/*
 * BestTempMailGetEmails 获取 best-temp-mail.com 邮件列表
 * 流程:
 *   1. 从 token JSON 中解析出 intToken、id、update_tag
 *   2. POST /api/v3/getEmailList {"address":"...","id":"...","intToken":"...","update_tag":"..."}
 *   3. 解析响应中的邮件列表并归一化
 */
func BestTempMailGetEmails(email, token string) ([]NormEmail, error) {
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("best-temp-mail: 邮箱地址为空")
	}

	/* 解析 token */
	var tkn bestTempMailToken
	if err := json.Unmarshal([]byte(token), &tkn); err != nil {
		return nil, fmt.Errorf("best-temp-mail: 解析 token 失败: %w", err)
	}

	/* 构造请求体 */
	reqBody, err := json.Marshal(map[string]string{
		"address":    email,
		"id":         tkn.ID,
		"intToken":   tkn.IntToken,
		"update_tag": tkn.UpdateTag,
	})
	if err != nil {
		return nil, fmt.Errorf("best-temp-mail: 序列化请求体失败: %w", err)
	}

	req, err := http.NewRequest("POST", bestTempMailAPIBase+"/getEmailList", strings.NewReader(string(reqBody)))
	if err != nil {
		return nil, fmt.Errorf("best-temp-mail: 创建请求失败: %w", err)
	}
	bestTempMailHeaders(req)

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, fmt.Errorf("best-temp-mail: 请求获取邮件失败: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("best-temp-mail: 读取响应失败: %w", err)
	}

	if err := CheckHTTPStatus(resp, "best-temp-mail getEmailList"); err != nil {
		return nil, err
	}

	/* 解析响应: {"data":{"hasNewEmail":false},"status":"success","t":0}
	 *          {"data":{"hasNewEmail":true,"emails":[...]},"status":"success","t":N}
	 */
	var result struct {
		Status string `json:"status"`
		Data   struct {
			HasNewEmail bool                     `json:"hasNewEmail"`
			Emails      []map[string]interface{} `json:"emails"`
		} `json:"data"`
	}
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, fmt.Errorf("best-temp-mail: 解析邮件列表失败: %w", err)
	}

	if !result.Data.HasNewEmail || len(result.Data.Emails) == 0 {
		return []NormEmail{}, nil
	}

	return normEmailsFromMaps(result.Data.Emails, email), nil
}
