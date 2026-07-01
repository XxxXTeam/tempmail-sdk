package provider

import (
	"encoding/xml"
	"fmt"
	"io"
	"math/rand"
	"regexp"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

/**
 * Eyepaste — https://eyepaste.com
 * RSS XML API，无认证
 * 创建邮箱: 无需 API，直接生成随机用户名 + "@eyepaste.com"
 * 获取邮件: GET https://www.eyepaste.com/inbox/{email}.rss → RSS 2.0 XML
 */

const eyepasteDomain = "eyepaste.com"
const eyepasteBase = "https://www.eyepaste.com"

/* eyepasteRandomLocal 生成随机用户名 */
func eyepasteRandomLocal(n int) string {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	b := make([]byte, n)
	for i := range b {
		b[i] = chars[rand.Intn(len(chars))]
	}
	return string(b)
}

/* eyepasteDefaultHeaders 设置请求头 */
func eyepasteDefaultHeaders(req *http.Request) {
	req.Header.Set("Accept", "application/rss+xml, application/xml, text/xml, */*")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
	req.Header.Set("Cache-Control", "no-cache")
	req.Header.Set("DNT", "1")
	req.Header.Set("Pragma", "no-cache")
	req.Header.Set("Referer", "https://www.eyepaste.com/")
	req.Header.Set("User-Agent", getCurrentUA())
}

/* RSS 2.0 XML 结构定义 */
type eyepasteRSS struct {
	XMLName xml.Name           `xml:"rss"`
	Channel eyepasteRSSChannel `xml:"channel"`
}

type eyepasteRSSChannel struct {
	Items []eyepasteRSSItem `xml:"item"`
}

type eyepasteRSSItem struct {
	Title       string `xml:"title"`
	Description string `xml:"description"`
}

/* 正则表达式，用于从 description 中提取邮件元信息 */
var eyepasteMetaRe = regexp.MustCompile(`(?i)From:\s*(.+?)\s+To:\s*(.+?)\s+Subject:\s*(.+?)\s+Date:\s*(.+?)(?:<br|$)`)

/*
 * eyepasteParseDescription 解析 RSS item 的 description 字段
 * description 结构: 第一个 <p> 包含 " From: {from} To: {to} Subject: {subject} Date: {date}"
 * 后续内容为邮件正文 HTML
 */
func eyepasteParseDescription(desc string) (from, to, subject, date, body string) {
	/* 尝试用正则提取元信息 */
	matches := eyepasteMetaRe.FindStringSubmatch(desc)
	if len(matches) >= 5 {
		from = strings.TrimSpace(matches[1])
		to = strings.TrimSpace(matches[2])
		subject = strings.TrimSpace(matches[3])
		date = strings.TrimSpace(matches[4])
	}

	/*
	 * 提取邮件正文：
	 * 找到第一个 </p> 之后的内容作为邮件正文
	 */
	idx := strings.Index(strings.ToLower(desc), "</p>")
	if idx >= 0 {
		body = strings.TrimSpace(desc[idx+4:])
	} else {
		/* 如果没有 </p>，尝试找 <br> 之后的内容 */
		brIdx := strings.Index(strings.ToLower(desc), "<br")
		if brIdx >= 0 {
			/* 跳过 <br> 或 <br/> 或 <br /> 标签 */
			rest := desc[brIdx:]
			closeIdx := strings.Index(rest, ">")
			if closeIdx >= 0 {
				body = strings.TrimSpace(rest[closeIdx+1:])
			}
		}
	}
	return
}

/*
 * EyepasteGenerate 创建 eyepaste.com 临时邮箱
 * 无需 API 调用，直接生成随机用户名 + "@eyepaste.com"
 */
func EyepasteGenerate(channel ...string) (*CreatedMailbox, error) {
	user := eyepasteRandomLocal(10)
	email := user + "@" + eyepasteDomain
	ch := "eyepaste"
	if len(channel) > 0 && channel[0] != "" {
		ch = channel[0]
	}
	return &CreatedMailbox{Channel: ch, Email: email, Token: ""}, nil
}

/*
 * EyepasteGetEmails 获取 eyepaste.com 邮件列表
 * GET https://www.eyepaste.com/inbox/{email}.rss → RSS 2.0 XML
 */
func EyepasteGetEmails(email string) ([]NormEmail, error) {
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("eyepaste: empty email")
	}

	u := fmt.Sprintf("%s/inbox/%s.rss", eyepasteBase, email)
	req, err := http.NewRequest("GET", u, nil)
	if err != nil {
		return nil, err
	}
	eyepasteDefaultHeaders(req)

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
		return nil, fmt.Errorf("eyepaste rss: %d", resp.StatusCode)
	}

	var rss eyepasteRSS
	if err := xml.Unmarshal(body, &rss); err != nil {
		return nil, fmt.Errorf("eyepaste: xml parse error: %w", err)
	}

	emails := make([]NormEmail, 0, len(rss.Channel.Items))
	for i, item := range rss.Channel.Items {
		from, to, subject, date, htmlBody := eyepasteParseDescription(item.Description)

		/* 如果从 description 中未提取到 subject，使用 RSS item 的 title */
		if subject == "" {
			subject = item.Title
		}

		/* 收件人地址回退为当前邮箱 */
		if to == "" {
			to = email
		}

		ne := NormEmail{
			ID:      fmt.Sprintf("%d", i+1),
			From:    from,
			To:      to,
			Subject: subject,
			HTML:    htmlBody,
			Date:    date,
		}
		emails = append(emails, ne)
	}
	return emails, nil
}
