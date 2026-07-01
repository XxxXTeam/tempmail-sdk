package provider

import (
	"fmt"
	"io"
	"regexp"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

/*
 * lroid.com — https://lroid.com
 * HTML 页面解析模式，基于 session cookies
 * 创建邮箱: GET https://lroid.com → 从 HTML 中提取 <input id="eposta_adres" value="xxx@yevme.com">
 * 获取邮件: GET https://lroid.com → 解析页面中的邮件列表 <li class="mail">
 * 域名: yevme.com
 */

const lroidBase = "https://lroid.com"

var (
	/* 匹配 <input id="eposta_adres" value="xxx@yevme.com"> */
	lroidEmailRe = regexp.MustCompile(`(?i)<input[^>]+id=["']eposta_adres["'][^>]+value=["']([^"']+)["']`)
	/* 备用：value 在 id 之前的情况 */
	lroidEmailRe2 = regexp.MustCompile(`(?i)<input[^>]+value=["']([^"']+@[^"']+)["'][^>]+id=["']eposta_adres["']`)

	/* 匹配邮件列表中的 <li class="mail"> 块 */
	lroidMailLiRe = regexp.MustCompile(`(?is)<li\s+class="mail"[^>]*>([\s\S]*?)</li>`)
	/* 从邮件块中提取链接（邮件 ID） */
	lroidMailHrefRe = regexp.MustCompile(`(?i)href=["']/?([^"']+)["']`)
	/* 从邮件块中提取发件人 */
	lroidMailFromRe = regexp.MustCompile(`(?is)<span\s+class=["']mail_from["'][^>]*>([\s\S]*?)</span>`)
	/* 从邮件块中提取主题 */
	lroidMailSubjectRe = regexp.MustCompile(`(?is)<span\s+class=["']mail_sub["'][^>]*>([\s\S]*?)</span>`)
	/* 从邮件块中提取时间 */
	lroidMailDateRe = regexp.MustCompile(`(?is)<span\s+class=["']mail_date["'][^>]*>([\s\S]*?)</span>`)
)

/* lroidBrowserHeaders 设置浏览器模拟请求头 */
func lroidBrowserHeaders(req *http.Request) {
	req.Header.Set("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
	req.Header.Set("Cache-Control", "no-cache")
	req.Header.Set("DNT", "1")
	req.Header.Set("Pragma", "no-cache")
	req.Header.Set("Upgrade-Insecure-Requests", "1")
	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36")
}

/* lroidMergeCookies 合并响应中的 Set-Cookie 为 Cookie 字符串 */
func lroidMergeCookies(existing string, cookies []*http.Cookie) string {
	parts := []string{}
	if existing != "" {
		parts = append(parts, existing)
	}
	for _, c := range cookies {
		parts = append(parts, c.Name+"="+c.Value)
	}
	return strings.Join(parts, "; ")
}

/* lroidExtractEmail 从 HTML 中提取邮箱地址 */
func lroidExtractEmail(html string) string {
	if m := lroidEmailRe.FindStringSubmatch(html); len(m) > 1 {
		return strings.TrimSpace(m[1])
	}
	if m := lroidEmailRe2.FindStringSubmatch(html); len(m) > 1 {
		return strings.TrimSpace(m[1])
	}
	return ""
}

/*
 * LroidGenerate 创建 lroid.com 临时邮箱
 * 流程: GET https://lroid.com → 从 HTML 中提取自动分配的邮箱地址，保存 session cookies
 */
func LroidGenerate() (*CreatedMailbox, error) {
	req, err := http.NewRequest("GET", lroidBase, nil)
	if err != nil {
		return nil, err
	}
	lroidBrowserHeaders(req)

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	/* 收集 session cookies */
	cookie := lroidMergeCookies("", resp.Cookies())

	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("lroid: 首页请求失败 %d", resp.StatusCode)
	}

	html := string(raw)
	emailAddr := lroidExtractEmail(html)
	if emailAddr == "" {
		return nil, fmt.Errorf("lroid: 未能从页面中提取邮箱地址")
	}

	/* token 中保存 session cookies，后续获取邮件时需要 */
	return &CreatedMailbox{
		Channel: "lroid",
		Email:   emailAddr,
		Token:   cookie,
	}, nil
}

/*
 * LroidGetEmails 获取 lroid.com 邮件列表
 * 使用 session cookies 重新访问首页，解析邮件列表
 * token: session cookies 字符串
 */
func LroidGetEmails(token string, email string) ([]NormEmail, error) {
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("lroid: 邮箱地址为空")
	}
	if token == "" {
		return nil, fmt.Errorf("lroid: session cookie 为空")
	}

	/* 携带 session cookies 访问首页获取邮件列表 */
	req, err := http.NewRequest("GET", lroidBase, nil)
	if err != nil {
		return nil, err
	}
	lroidBrowserHeaders(req)
	req.Header.Set("Cookie", token)

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("lroid: 获取邮件列表失败 %d", resp.StatusCode)
	}

	html := string(raw)

	/* 解析 <li class="mail"> 块 */
	mailBlocks := lroidMailLiRe.FindAllStringSubmatch(html, -1)
	if len(mailBlocks) == 0 {
		return []NormEmail{}, nil
	}

	emails := make([]NormEmail, 0, len(mailBlocks))
	for i, block := range mailBlocks {
		if len(block) < 2 {
			continue
		}
		content := block[1]

		/* 提取邮件 ID（从 href 链接中获取） */
		mailID := fmt.Sprintf("%d", i+1)
		if m := lroidMailHrefRe.FindStringSubmatch(content); len(m) > 1 {
			mailID = strings.TrimSpace(m[1])
		}

		/* 提取发件人 */
		from := ""
		if m := lroidMailFromRe.FindStringSubmatch(content); len(m) > 1 {
			from = strings.TrimSpace(m[1])
		}

		/* 提取主题 */
		subject := ""
		if m := lroidMailSubjectRe.FindStringSubmatch(content); len(m) > 1 {
			subject = strings.TrimSpace(m[1])
		}

		/* 提取时间 */
		date := ""
		if m := lroidMailDateRe.FindStringSubmatch(content); len(m) > 1 {
			date = strings.TrimSpace(m[1])
		}

		/* 尝试获取邮件正文 */
		htmlBody, textBody := lroidFetchMailBody(token, mailID)

		flat := map[string]interface{}{
			"id":      mailID,
			"from":    from,
			"to":      email,
			"subject": subject,
			"date":    date,
			"html":    htmlBody,
			"text":    textBody,
			"isRead":  false,
		}
		emails = append(emails, NormalizeMap(flat, email))
	}
	return emails, nil
}

/* lroidFetchMailBody 获取单封邮件正文内容 */
func lroidFetchMailBody(cookie string, mailID string) (string, string) {
	if mailID == "" || cookie == "" {
		return "", ""
	}

	/* 清理 mailID，移除前导斜杠 */
	mailID = strings.TrimLeft(mailID, "/")

	u := fmt.Sprintf("%s/%s", lroidBase, mailID)
	req, err := http.NewRequest("GET", u, nil)
	if err != nil {
		return "", ""
	}
	lroidBrowserHeaders(req)
	req.Header.Set("Cookie", cookie)

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return "", ""
	}
	defer resp.Body.Close()

	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", ""
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return "", ""
	}

	body := string(raw)

	/*
	 * 尝试从 iframe 中提取内容
	 * 或直接从邮件详情页中提取 HTML 正文
	 */
	htmlBody := lroidExtractBodyFromPage(body)
	return htmlBody, ""
}

/* lroidExtractBodyFromPage 从邮件详情页中提取正文 HTML */
func lroidExtractBodyFromPage(page string) string {
	/* 尝试匹配 iframe 的 srcdoc 或内嵌内容 */
	iframeSrcdocRe := regexp.MustCompile(`(?is)<iframe[^>]+srcdoc=["']([\s\S]*?)["'][^>]*>`)
	if m := iframeSrcdocRe.FindStringSubmatch(page); len(m) > 1 {
		return strings.TrimSpace(m[1])
	}

	/* 尝试匹配邮件内容 div */
	contentDivRe := regexp.MustCompile(`(?is)<div\s+class=["']mail[_-]?content["'][^>]*>([\s\S]*?)</div>`)
	if m := contentDivRe.FindStringSubmatch(page); len(m) > 1 {
		return strings.TrimSpace(m[1])
	}

	/* 尝试匹配 message-body 或 email-body */
	bodyDivRe := regexp.MustCompile(`(?is)<div\s+class=["'](?:message|email)[_-]?body["'][^>]*>([\s\S]*?)</div>`)
	if m := bodyDivRe.FindStringSubmatch(page); len(m) > 1 {
		return strings.TrimSpace(m[1])
	}

	return ""
}
