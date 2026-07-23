package provider

import (
	"encoding/base64"
	"encoding/json"
	"fmt"
	"html"
	"io"
	"regexp"
	"sort"
	"strings"

	http "github.com/bogdanfinn/fhttp"
	tls_client "github.com/bogdanfinn/tls-client"
	gohtml "golang.org/x/net/html"
)

/*
 * haribu.net：Tempail 类模式的临时邮箱服务
 * 创建邮箱：GET https://haribu.net → 从 HTML 中提取 <input id="eposta_adres" value="xxx@yevme.com">
 * 获取邮件：GET https://haribu.net → 解析页面中的邮件列表 <li class="mail"> 条目
 * 辅助 API：GET https://haribu.net/en/api-kontrol/ → 检查新邮件（返回 JSON）
 * 需要 session cookies 维持会话
 */

const haribuBase = "https://haribu.net"

/* haribu session 令牌前缀，用于区分不同渠道的 token 格式 */
const haribuTokPrefix = "haribu1:"

/* haribuSess 存储 haribu 会话信息，序列化后作为 token */
type haribuSess struct {
	CookieHdr string `json:"c"`
}

var (
	/* 匹配邮箱地址输入框：<input id="eposta_adres" value="xxx@yyy.com"> */
	haribuEmailInputRe = regexp.MustCompile(`(?i)<input[^>]+id\s*=\s*["']eposta_adres["'][^>]+value\s*=\s*["']([^"']+)["']`)
	/* 备选匹配顺序（value 在 id 前面的情况） */
	haribuEmailInputRe2 = regexp.MustCompile(`(?i)<input[^>]+value\s*=\s*["']([^"']+@[^"']+)["'][^>]+id\s*=\s*["']eposta_adres["']`)
	/* 匹配邮件列表条目 <li class="mail"> ... </li> */
	haribuMailItemRe = regexp.MustCompile(`(?is)<li\s+class\s*=\s*["']mail["'][^>]*>([\s\S]*?)</li>`)
	/* 匹配发件人 */
	haribuFromRe = regexp.MustCompile(`(?is)<span\s+class\s*=\s*["']mail_gonderen["'][^>]*>([\s\S]*?)</span>`)
	/* 匹配邮件主题 */
	haribuSubjectRe = regexp.MustCompile(`(?is)<span\s+class\s*=\s*["']mail_konu["'][^>]*>([\s\S]*?)</span>`)
	/* 匹配邮件时间 */
	haribuDateRe = regexp.MustCompile(`(?is)<span\s+class\s*=\s*["']mail_zaman["'][^>]*>([\s\S]*?)</span>`)
	/* 匹配邮件详情链接 */
	haribuMailLinkRe = regexp.MustCompile(`(?i)href\s*=\s*["']([^"']*(?:mail|read|view)[^"']*)["']`)
)

/*
 * haribuExtractBodyHTML 使用 HTML 解析器提取邮件正文 div 的完整内部 HTML，
 * 避免非贪婪正则在嵌套 div 时截断正文。支持多种 class/id 名称。
 */
func haribuExtractBodyHTML(page string) string {
	doc, err := gohtml.Parse(strings.NewReader(page))
	if err != nil {
		return ""
	}
	targets := []string{"mail_icerik", "icerik", "mail-content", "message-body"}
	var result string
	var walk func(*gohtml.Node)
	walk = func(n *gohtml.Node) {
		if result != "" {
			return
		}
		if n.Type == gohtml.ElementNode && n.Data == "div" {
			for _, a := range n.Attr {
				if a.Key == "class" || a.Key == "id" {
					for _, target := range targets {
						if strings.Contains(a.Val, target) {
							var sb strings.Builder
							for c := n.FirstChild; c != nil; c = c.NextSibling {
								gohtml.Render(&sb, c)
							}
							result = strings.TrimSpace(sb.String())
							return
						}
					}
				}
			}
		}
		for c := n.FirstChild; c != nil; c = c.NextSibling {
			walk(c)
		}
	}
	walk(doc)
	return result
}

/* haribuHTTPClient 获取 haribu 专用 HTTP 客户端（无 cookie jar，手动管理 cookie） */
func haribuHTTPClient() tls_client.HttpClient {
	if HTTPClientNoCookieJar != nil {
		return HTTPClientNoCookieJar()
	}
	return HTTPClient()
}

/* haribuSetHeaders 设置 haribu 请求的通用 HTTP 头 */
func haribuSetHeaders(req *http.Request, referer string) {
	req.Header.Set("User-Agent", getCurrentUA())
	req.Header.Set("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
	req.Header.Set("Cache-Control", "no-cache")
	req.Header.Set("DNT", "1")
	req.Header.Set("Pragma", "no-cache")
	req.Header.Set("Upgrade-Insecure-Requests", "1")
	if referer != "" {
		req.Header.Set("Referer", referer)
	}
}

/* haribuCookieMap 解析 Cookie 头字符串为 map */
func haribuCookieMap(hdr string) map[string]string {
	m := make(map[string]string)
	for _, part := range strings.Split(hdr, ";") {
		part = strings.TrimSpace(part)
		if part == "" {
			continue
		}
		i := strings.Index(part, "=")
		if i <= 0 || i >= len(part)-1 {
			continue
		}
		k := strings.TrimSpace(part[:i])
		v := strings.TrimSpace(part[i+1:])
		if k != "" {
			m[k] = v
		}
	}
	return m
}

/* haribuMergeCookies 合并已有 cookie 和响应中的新 cookie */
func haribuMergeCookies(prev string, cookies []*http.Cookie) string {
	m := haribuCookieMap(prev)
	for _, c := range cookies {
		if c == nil || c.Name == "" {
			continue
		}
		m[c.Name] = c.Value
	}
	keys := make([]string, 0, len(m))
	for k := range m {
		keys = append(keys, k)
	}
	sort.Strings(keys)
	parts := make([]string, 0, len(keys))
	for _, k := range keys {
		parts = append(parts, k+"="+m[k])
	}
	return strings.Join(parts, "; ")
}

/* haribuEncodeSess 将会话信息编码为 token 字符串 */
func haribuEncodeSess(s *haribuSess) (string, error) {
	b, err := json.Marshal(s)
	if err != nil {
		return "", err
	}
	return haribuTokPrefix + base64.StdEncoding.EncodeToString(b), nil
}

/* haribuDecodeSess 从 token 字符串解码会话信息 */
func haribuDecodeSess(tok string) (*haribuSess, error) {
	if !strings.HasPrefix(tok, haribuTokPrefix) {
		return nil, fmt.Errorf("haribu: 无效的会话令牌")
	}
	raw, err := base64.StdEncoding.DecodeString(tok[len(haribuTokPrefix):])
	if err != nil {
		return nil, fmt.Errorf("haribu: 无效的会话令牌")
	}
	var s haribuSess
	if err := json.Unmarshal(raw, &s); err != nil {
		return nil, fmt.Errorf("haribu: 无效的会话令牌")
	}
	if s.CookieHdr == "" {
		return nil, fmt.Errorf("haribu: 会话令牌中缺少 cookie")
	}
	return &s, nil
}

/* haribuExtractEmail 从 HTML 中提取邮箱地址（匹配 id="eposta_adres" 的 input 元素） */
func haribuExtractEmail(htmlStr string) (string, error) {
	/* 尝试标准顺序：id 在 value 前 */
	if m := haribuEmailInputRe.FindStringSubmatch(htmlStr); len(m) >= 2 {
		addr := strings.TrimSpace(html.UnescapeString(m[1]))
		if addr != "" && strings.Contains(addr, "@") {
			return addr, nil
		}
	}
	/* 备选顺序：value 在 id 前 */
	if m := haribuEmailInputRe2.FindStringSubmatch(htmlStr); len(m) >= 2 {
		addr := strings.TrimSpace(html.UnescapeString(m[1]))
		if addr != "" && strings.Contains(addr, "@") {
			return addr, nil
		}
	}
	return "", fmt.Errorf("haribu: 未找到邮箱地址（eposta_adres）")
}

/* haribuStripTags 移除 HTML 标签，返回纯文本 */
func haribuStripTags(s string) string {
	re := regexp.MustCompile(`<[^>]+>`)
	return strings.TrimSpace(re.ReplaceAllString(s, " "))
}

/*
 * HaribuGenerate 创建 haribu 临时邮箱
 * 流程：GET haribu.net → 获取 session cookie → 从 HTML 提取邮箱地址
 */
func HaribuGenerate() (*CreatedMailbox, error) {
	client := haribuHTTPClient()

	/* 第一步：GET 首页获取 session cookie 和邮箱地址 */
	req, err := http.NewRequest("GET", haribuBase, nil)
	if err != nil {
		return nil, err
	}
	haribuSetHeaders(req, "")

	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if err := CheckHTTPStatus(resp, "haribu home"); err != nil {
		return nil, err
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	cookieHdr := haribuMergeCookies("", resp.Cookies())
	htmlStr := string(body)

	/* 从 HTML 中提取邮箱地址 */
	emailAddr, err := haribuExtractEmail(htmlStr)
	if err != nil {
		return nil, err
	}

	/* 编码会话信息为 token */
	tok, err := haribuEncodeSess(&haribuSess{CookieHdr: cookieHdr})
	if err != nil {
		return nil, err
	}

	return &CreatedMailbox{
		Channel: "haribu",
		Email:   emailAddr,
		Token:   tok,
	}, nil
}

/*
 * HaribuGetEmails 获取 haribu 邮件列表
 * 流程：先调用 api-kontrol 检查新邮件 → GET 首页解析邮件列表 → 提取各封邮件详情
 */
func HaribuGetEmails(email, token string) ([]NormEmail, error) {
	sess, err := haribuDecodeSess(token)
	if err != nil {
		return nil, err
	}
	client := haribuHTTPClient()

	/* 调用 kontrol API 触发新邮件检查 */
	kontrolURL := haribuBase + "/en/api-kontrol/"
	reqK, err := http.NewRequest("GET", kontrolURL, nil)
	if err == nil {
		haribuSetHeaders(reqK, haribuBase)
		reqK.Header.Set("Cookie", sess.CookieHdr)
		reqK.Header.Set("X-Requested-With", "XMLHttpRequest")
		respK, errK := client.Do(reqK)
		if errK == nil {
			io.ReadAll(respK.Body)
			respK.Body.Close()
		}
	}

	/* GET 首页获取收件箱页面 */
	req, err := http.NewRequest("GET", haribuBase, nil)
	if err != nil {
		return nil, err
	}
	haribuSetHeaders(req, haribuBase)
	req.Header.Set("Cookie", sess.CookieHdr)

	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if err := CheckHTTPStatus(resp, "haribu inbox"); err != nil {
		return nil, err
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	htmlStr := string(body)

	/* 解析邮件列表中的 <li class="mail"> 条目 */
	items := haribuMailItemRe.FindAllStringSubmatch(htmlStr, -1)
	if len(items) == 0 {
		return []NormEmail{}, nil
	}

	out := make([]NormEmail, 0, len(items))
	for i, item := range items {
		if len(item) < 2 {
			continue
		}
		content := item[1]
		raw := map[string]interface{}{
			"id": fmt.Sprintf("haribu-%d", i),
			"to": email,
		}

		/* 提取发件人 */
		if fm := haribuFromRe.FindStringSubmatch(content); len(fm) >= 2 {
			raw["from"] = strings.TrimSpace(html.UnescapeString(haribuStripTags(fm[1])))
		}
		/* 提取主题 */
		if sm := haribuSubjectRe.FindStringSubmatch(content); len(sm) >= 2 {
			raw["subject"] = strings.TrimSpace(html.UnescapeString(haribuStripTags(sm[1])))
		}
		/* 提取时间 */
		if dm := haribuDateRe.FindStringSubmatch(content); len(dm) >= 2 {
			raw["date"] = strings.TrimSpace(html.UnescapeString(haribuStripTags(dm[1])))
		}

		/* 尝试获取邮件正文：如果有详情链接则请求详情页 */
		if lm := haribuMailLinkRe.FindStringSubmatch(content); len(lm) >= 2 {
			detailURL := lm[1]
			if !strings.HasPrefix(detailURL, "http") {
				detailURL = haribuBase + "/" + strings.TrimPrefix(detailURL, "/")
			}
			htmlBody := haribuFetchDetail(client, detailURL, sess.CookieHdr)
			if htmlBody != "" {
				raw["html"] = htmlBody
			}
		}

		out = append(out, NormalizeMap(raw, email))
	}
	return out, nil
}

/* haribuFetchDetail 获取单封邮件的详情页正文 */
func haribuFetchDetail(client tls_client.HttpClient, detailURL, cookieHdr string) string {
	req, err := http.NewRequest("GET", detailURL, nil)
	if err != nil {
		return ""
	}
	haribuSetHeaders(req, haribuBase)
	req.Header.Set("Cookie", cookieHdr)

	resp, err := client.Do(req)
	if err != nil {
		return ""
	}
	defer resp.Body.Close()
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return ""
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return ""
	}

	/* 使用 HTML 解析器提取完整正文 */
	if htmlBody := haribuExtractBodyHTML(string(body)); htmlBody != "" {
		return htmlBody
	}
	return ""
}
