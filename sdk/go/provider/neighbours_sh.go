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

const (
	neighboursShBase   = "https://neighbours.sh/api/v1"
	neighboursShDomain = "neighbours.sh"
)

/* neighboursShListResponse 收件箱列表响应：{"success":true,"data":[{"uid":N,...}],"count":N} */
type neighboursShListResponse struct {
	Success bool `json:"success"`
	Data    []struct {
		UID     *int64 `json:"uid"`
		Subject string `json:"subject"`
		Date    string `json:"date"`
	} `json:"data"`
}

/* neighboursShDetailResponse 单邮件详情响应：{"success":true,"data":{...}} */
type neighboursShDetailResponse struct {
	Success bool `json:"success"`
	Data    *struct {
		UID  *int64 `json:"uid"`
		From struct {
			Value []struct {
				Address string `json:"address"`
				Name    string `json:"name"`
			} `json:"value"`
			Text string `json:"text"`
		} `json:"from"`
		To struct {
			Text string `json:"text"`
		} `json:"to"`
		Subject     string        `json:"subject"`
		Text        string        `json:"text"`
		HTML        string        `json:"html"`
		Date        string        `json:"date"`
		Attachments []interface{} `json:"attachments"`
	} `json:"data"`
}

/*
 * neighboursShRandomUsername 生成随机用户名，前缀 sdk + 10 位小写字母数字
 */
func neighboursShRandomUsername() string {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	var b strings.Builder
	b.WriteString("sdk")
	for i := 0; i < 10; i++ {
		b.WriteByte(chars[rand.Intn(len(chars))])
	}
	return b.String()
}

/*
 * neighboursShGetJSON 使用 SDK 共享客户端发起 GET 请求并读取响应体
 */
func neighboursShGetJSON(u string) ([]byte, int, error) {
	req, err := http.NewRequest(http.MethodGet, u, nil)
	if err != nil {
		return nil, 0, err
	}
	req.Header.Set("Accept", "application/json")
	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, 0, err
	}
	defer resp.Body.Close()
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, resp.StatusCode, err
	}
	return body, resp.StatusCode, nil
}

/*
 * NeighboursShGenerate 创建 neighbours.sh 临时邮箱
 * 公共收件箱模式，任意用户名即可收信，无需 API 调用，Token 存邮箱地址本身
 */
func NeighboursShGenerate() (*CreatedMailbox, error) {
	email := neighboursShRandomUsername() + "@" + neighboursShDomain
	return &CreatedMailbox{
		Channel: "neighbours-sh",
		Email:   email,
		Token:   email,
	}, nil
}

/*
 * neighboursShFlatten 将 neighbours.sh 邮件详情映射为标准化中间 map
 */
func neighboursShFlatten(detail *neighboursShDetailResponse, recipient string) map[string]any {
	d := detail.Data
	from := ""
	if len(d.From.Value) > 0 {
		from = d.From.Value[0].Address
	}
	if strings.TrimSpace(from) == "" {
		from = d.From.Text
	}
	to := d.To.Text
	if strings.TrimSpace(to) == "" {
		to = recipient
	}
	id := ""
	if d.UID != nil {
		id = fmt.Sprintf("%d", *d.UID)
	}
	return map[string]any{
		"id":          id,
		"from":        from,
		"to":          to,
		"subject":     d.Subject,
		"text":        d.Text,
		"html":        d.HTML,
		"date":        d.Date,
		"attachments": d.Attachments,
	}
}

/*
 * NeighboursShGetEmails 获取 neighbours.sh 邮件列表
 * API: GET /inbox/{address} 取列表拿 uid，再逐个 GET /inbox/{address}/{uid} 取完整正文
 * @param token - 邮箱地址
 * @param email - 邮箱地址
 */
func NeighboursShGetEmails(token, email string) ([]NormEmail, error) {
	address := strings.TrimSpace(token)
	if address == "" {
		address = strings.TrimSpace(email)
	}
	if address == "" {
		return nil, fmt.Errorf("neighbours-sh: 缺少邮箱地址")
	}

	listURL := fmt.Sprintf("%s/inbox/%s", neighboursShBase, url.PathEscape(address))
	body, status, err := neighboursShGetJSON(listURL)
	if err != nil {
		return nil, err
	}
	if status < 200 || status >= 300 {
		return nil, fmt.Errorf("neighbours-sh: 获取邮件列表失败 http %d", status)
	}
	var list neighboursShListResponse
	if err := json.Unmarshal(body, &list); err != nil {
		return nil, err
	}

	out := make([]NormEmail, 0, len(list.Data))
	for _, row := range list.Data {
		if row.UID == nil {
			continue
		}
		/* 详情接口返回完整正文 */
		detailURL := fmt.Sprintf("%s/inbox/%s/%d", neighboursShBase, url.PathEscape(address), *row.UID)
		detailBody, detailStatus, err := neighboursShGetJSON(detailURL)
		if err != nil || detailStatus < 200 || detailStatus >= 300 {
			continue
		}
		var detail neighboursShDetailResponse
		if err := json.Unmarshal(detailBody, &detail); err != nil {
			continue
		}
		if detail.Data == nil {
			continue
		}
		out = append(out, NormalizeMap(neighboursShFlatten(&detail, address), address))
	}
	return out, nil
}
