package provider

import (
	"fmt"
	"html"
	"io"
	"regexp"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

/*
 * mohmal.com -- https://www.mohmal.com
 * HTML 页面解析模式，基于 connect.sid session cookie
 * 创建邮箱: GET /en/create/random（跟随重定向到 /en/inbox），从 HTML 中提取 data-email 属性
 * 获取邮件: GET /en/inbox，解析 #inbox-table tbody tr 中的邮件行
 * 查看邮件: GET /en/message/{id}，从 HTML 中提取邮件内容
 * 续期: GET /en/extend
 * 删除: GET /en/logout
 */

const mohmalBase = "https://www.mohmal.com"

var (
	/* 匹配 data-email="xxx@domain" 属性 */
	mohmalDataEmailRe = regexp.MustCompile(`(?i)data-email=["']([^"']+)["']`)

	/* 匹配 #inbox-table 中的邮件行 <tr> */
	mohmalTrRe = regexp.MustCompile(`(?is)<tr[^>]*>\s*<td[^>]*>([\s\S]*?)</tr>`)

	/* 匹配邮件链接 /en/message/{id} */
	mohmalMsgHrefRe = regexp.MustCompile(`(?i)href=["'](/en/message/([^"']+))["']`)

	/* 从邮件行中提取 <td> 内容 */
	mohmalTdRe = regexp.MustCompile(`(?is)<td[^>]*>([\s\S]*?)</td>`)

	/* 从邮件详情页中提取邮件正文 */
	mohmalBodyRe = regexp.MustCompile(`(?is)<div[^>]+class=["'][^"']*mail-body[^"']*["'][^>]*>([\s\S]*?)</div>`)

	/* 备用: 匹配 .message-body 或 #message-body */
	mohmalBodyRe2 = regexp.MustCompile(`(?is)<div[^>]+(?:class|id)=["'][^"']*message[_-]?body[^"']*["'][^>]*>([\s\S]*?)</div>`)

	/* 从邮件详情页中提取发件人 */
	mohmalDetailFromRe = regexp.MustCompile(`(?is)<span[^>]+class=["'][^"']*from[^"']*["'][^>]*>([\s\S]*?)</span>`)

	/* 从邮件详情页中提取主题 */
	mohmalDetailSubjectRe = regexp.MustCompile(`(?is)<span[^>]+class=["'][^"']*subject[^"']*["'][^>]*>([\s\S]*?)</span>`)

	/* 从邮件详情页中提取时间 */
	mohmalDetailDateRe = regexp.MustCompile(`(?is)<span[^>]+class=["'][^"']*date[^"']*["'][^>]*>([\s\S]*?)</span>`)

	/* 去除 HTML 标签 */
	mohmalStripTagRe = regexp.MustCompile(`<[^>]+>`)
)

/* mohmalBrowserHeaders 设置浏览器模拟请求头 */
func mohmalBrowserHeaders(req *http.Request, referer string) {
	req.Header.Set("User-Agent", GetCurrentUA())
	req.Header.Set("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8")
	req.Header.Set("Accept-Language", "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7")
	req.Header.Set("Cache-Control", "no-cache")
	req.Header.Set("DNT", "1")
	req.Header.Set("Pragma", "no-cache")
	req.Header.Set("Upgrade-Insecure-Requests", "1")
	if referer != "" {
		req.Header.Set("Referer", referer)
	}
}

/* mohmalStripTags 移除 HTML 标签并修剪空白 */
func mohmalStripTags(s string) string {
	return strings.TrimSpace(mohmalStripTagRe.ReplaceAllString(s, " "))
}

/* mohmalMergeCookies 合并已有 cookie 字符串和响应中的 Set-Cookie */
func mohmalMergeCookies(existing string, cookies []*http.Cookie) string {
	m := make(map[string]string)
	/* 解析已有的 cookie 字符串 */
	for _, part := range strings.Split(existing, ";") {
		part = strings.TrimSpace(part)
		if part == "" {
			continue
		}
		idx := strings.Index(part, "=")
		if idx > 0 && idx < len(part)-1 {
			m[strings.TrimSpace(part[:idx])] = strings.TrimSpace(part[idx+1:])
		}
	}
	/* 合并新的 cookie */
	for _, c := range cookies {
		if c != nil && c.Name != "" {
			m[c.Name] = c.Value
		}
	}
	parts := make([]string, 0, len(m))
	for k, v := range m {
		parts = append(parts, k+"="+v)
	}
	return strings.Join(parts, "; ")
}

/*
 * MohmalGenerate 创建 mohmal.com 临时邮箱
 * 流程:
 *   1. GET /en/create/random（跟随重定向到 /en/inbox），获取 connect.sid session cookie
 *   2. 从 /en/inbox 页面 HTML 中提取 data-email 属性获得邮箱地址
 * token: connect.sid cookie 字符串
 */
func MohmalGenerate() (*CreatedMailbox, error) {
	client := HTTPClient()

	/* 第一步: GET /en/create/random，跟随重定向到 /en/inbox */
	createURL := mohmalBase + "/en/create/random"
	req, err := http.NewRequest("GET", createURL, nil)
	if err != nil {
		return nil, err
	}
	mohmalBrowserHeaders(req, mohmalBase+"/en")

	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	if err := CheckHTTPStatus(resp, "mohmal create/random"); err != nil {
		return nil, err
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	/* 收集 session cookies */
	cookieHdr := mohmalMergeCookies("", resp.Cookies())

	/* 从页面 HTML 中提取 data-email 属性 */
	htmlStr := string(body)
	emailAddr := mohmalExtractEmail(htmlStr)

	/* 若重定向后的页面未包含邮箱，尝试单独请求 /en/inbox */
	if emailAddr == "" {
		inboxURL := mohmalBase + "/en/inbox"
		req2, err := http.NewRequest("GET", inboxURL, nil)
		if err != nil {
			return nil, err
		}
		mohmalBrowserHeaders(req2, mohmalBase+"/en")
		req2.Header.Set("Cookie", cookieHdr)

		resp2, err := client.Do(req2)
		if err != nil {
			return nil, err
		}
		defer resp2.Body.Close()

		if err := CheckHTTPStatus(resp2, "mohmal inbox"); err != nil {
			return nil, err
		}

		body2, err := io.ReadAll(resp2.Body)
		if err != nil {
			return nil, err
		}
		cookieHdr = mohmalMergeCookies(cookieHdr, resp2.Cookies())
		htmlStr = string(body2)
		emailAddr = mohmalExtractEmail(htmlStr)
	}

	if emailAddr == "" {
		return nil, fmt.Errorf("mohmal: 未能从页面中提取邮箱地址")
	}

	return &CreatedMailbox{
		Channel: "mohmal",
		Email:   emailAddr,
		Token:   cookieHdr,
	}, nil
}

/* mohmalExtractEmail 从 HTML 中提取 data-email 属性值 */
func mohmalExtractEmail(htmlStr string) string {
	m := mohmalDataEmailRe.FindStringSubmatch(htmlStr)
	if len(m) > 1 {
		return strings.TrimSpace(html.UnescapeString(m[1]))
	}
	return ""
}

/*
 * MohmalGetEmails 获取 mohmal.com 邮件列表
 * 流程:
 *   1. GET /en/inbox，从 HTML 中解析 #inbox-table 邮件行
 *   2. 对每封邮件 GET /en/message/{id} 获取详情
 * token: connect.sid cookie 字符串
 */
func MohmalGetEmails(email string, token string) ([]NormEmail, error) {
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("mohmal: 邮箱地址为空")
	}
	if token == "" {
		return nil, fmt.Errorf("mohmal: session cookie 为空")
	}

	client := HTTPClient()
	inboxURL := mohmalBase + "/en/inbox"

	/* 请求收件箱页面 */
	req, err := http.NewRequest("GET", inboxURL, nil)
	if err != nil {
		return nil, err
	}
	mohmalBrowserHeaders(req, mohmalBase+"/en")
	req.Header.Set("Cookie", token)

	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	if err := CheckHTTPStatus(resp, "mohmal inbox"); err != nil {
		return nil, err
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	htmlStr := string(body)

	/* 解析邮件行: 从 #inbox-table tbody tr 中提取邮件链接 */
	msgMatches := mohmalMsgHrefRe.FindAllStringSubmatch(htmlStr, -1)
	if len(msgMatches) == 0 {
		return []NormEmail{}, nil
	}

	/* 去重邮件 ID */
	type msgRef struct {
		path string
		id   string
	}
	seen := make(map[string]struct{})
	var msgs []msgRef
	for _, m := range msgMatches {
		if len(m) < 3 {
			continue
		}
		id := m[2]
		if _, ok := seen[id]; ok {
			continue
		}
		seen[id] = struct{}{}
		msgs = append(msgs, msgRef{path: m[1], id: id})
	}

	/* 同时从收件箱行中提取摘要信息作为 fallback */
	trBlocks := mohmalTrRe.FindAllStringSubmatch(htmlStr, -1)
	rowData := make(map[string]map[string]string)
	for _, tr := range trBlocks {
		if len(tr) < 2 {
			continue
		}
		row := tr[0]
		/* 检查该行是否包含邮件链接 */
		hrefMatch := mohmalMsgHrefRe.FindStringSubmatch(row)
		if len(hrefMatch) < 3 {
			continue
		}
		msgID := hrefMatch[2]

		/* 提取 <td> 列内容 */
		tdMatches := mohmalTdRe.FindAllStringSubmatch(row, -1)
		data := make(map[string]string)
		for i, td := range tdMatches {
			if len(td) < 2 {
				continue
			}
			val := mohmalStripTags(td[1])
			switch i {
			case 0:
				data["from"] = val
			case 1:
				data["subject"] = val
			case 2:
				data["date"] = val
			}
		}
		rowData[msgID] = data
	}

	/* 逐封获取邮件详情 */
	emails := make([]NormEmail, 0, len(msgs))
	for _, msg := range msgs {
		raw := mohmalFetchDetail(client, token, msg.id, email)

		/* 使用收件箱行数据补充缺失字段 */
		if rd, ok := rowData[msg.id]; ok {
			if raw["from"] == nil || raw["from"] == "" {
				raw["from"] = rd["from"]
			}
			if raw["subject"] == nil || raw["subject"] == "" {
				raw["subject"] = rd["subject"]
			}
			if raw["date"] == nil || raw["date"] == "" {
				raw["date"] = rd["date"]
			}
		}

		emails = append(emails, NormalizeMap(raw, email))
	}

	return emails, nil
}

/* mohmalFetchDetail 获取单封邮件详情页 */
func mohmalFetchDetail(client interface {
	Do(*http.Request) (*http.Response, error)
}, cookie string, id string, recipient string) map[string]interface{} {
	raw := map[string]interface{}{"id": id, "to": recipient}

	detailURL := mohmalBase + "/en/message/" + id
	req, err := http.NewRequest("GET", detailURL, nil)
	if err != nil {
		return raw
	}
	mohmalBrowserHeaders(req, mohmalBase+"/en/inbox")
	req.Header.Set("Cookie", cookie)

	resp, err := client.Do(req)
	if err != nil {
		return raw
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return raw
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return raw
	}

	page := string(body)

	/* 提取发件人 */
	if m := mohmalDetailFromRe.FindStringSubmatch(page); len(m) > 1 {
		raw["from"] = strings.TrimSpace(mohmalStripTags(html.UnescapeString(m[1])))
	}

	/* 提取主题 */
	if m := mohmalDetailSubjectRe.FindStringSubmatch(page); len(m) > 1 {
		raw["subject"] = strings.TrimSpace(html.UnescapeString(mohmalStripTags(m[1])))
	}

	/* 提取时间 */
	if m := mohmalDetailDateRe.FindStringSubmatch(page); len(m) > 1 {
		raw["date"] = strings.TrimSpace(html.UnescapeString(mohmalStripTags(m[1])))
	}

	/* 提取邮件正文 HTML */
	if m := mohmalBodyRe.FindStringSubmatch(page); len(m) > 1 {
		raw["html"] = strings.TrimSpace(m[1])
	} else if m := mohmalBodyRe2.FindStringSubmatch(page); len(m) > 1 {
		raw["html"] = strings.TrimSpace(m[1])
	}

	return raw
}
