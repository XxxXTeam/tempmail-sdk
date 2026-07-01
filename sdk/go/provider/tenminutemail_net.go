package provider

import (
	"encoding/hex"
	"fmt"
	"io"
	"regexp"
	"strings"
	"time"

	http "github.com/bogdanfinn/fhttp"
)

/* 10minutemail.net 临时邮箱服务商
 * 流程：GET / 获取 session cookie（PHPSESSID）+ 从 HTML 提取随机分配的邮箱地址
 *       GET /mailbox.ajax.php?_={毫秒时间戳} 获取邮件列表（返回 HTML 表格，非 JSON）
 *       GET /readmail.html?mid={id} 获取单封邮件整页 HTML，从中提取正文片段
 * token 无需存储额外信息，session 由 tls-client 的 cookie jar 自动维护
 */

const tenminutemailNetBaseURL = "https://10minutemail.net"

var (
	/* tenminutemailNetEmailRe 从首页 HTML 的邮箱输入框提取地址（value="xxx@xxx.com"） */
	tenminutemailNetEmailRe = regexp.MustCompile(`value="([^"]+@[^"]+)"`)
	/* tenminutemailNetRowRe 从邮件列表表格逐行匹配 <tr>...</tr> */
	tenminutemailNetRowRe = regexp.MustCompile(`(?is)<tr[^>]*>(.*?)</tr>`)
	/* tenminutemailNetMidRe 从行内链接提取邮件 ID（readmail.html?mid=xxx） */
	tenminutemailNetMidRe = regexp.MustCompile(`(?i)readmail\.html\?mid=([^'"&]+)`)
	/* tenminutemailNetCellRe 逐个匹配行内 <td>...</td> 单元格 */
	tenminutemailNetCellRe = regexp.MustCompile(`(?is)<td[^>]*>(.*?)</td>`)
	/* tenminutemailNetTitleRe 从单元格提取 span 的 title 属性（收件时间，格式如 2026-06-29 21:59:54 UTC） */
	tenminutemailNetTitleRe = regexp.MustCompile(`(?i)title="([^"]+)"`)
	/* tenminutemailNetCFRe 提取 Cloudflare 邮箱保护混淆数据（data-cfemail="十六进制"） */
	tenminutemailNetCFRe = regexp.MustCompile(`(?i)data-cfemail="([0-9a-fA-F]+)"`)
	/* tenminutemailNetTagRe 去除 HTML 标签用于提取纯文本 */
	tenminutemailNetTagRe = regexp.MustCompile(`(?s)<[^>]+>`)
)

/* tenminutemailNetBrowserHeaders 设置浏览器模拟请求头（用于首页 HTML） */
func tenminutemailNetBrowserHeaders(req *http.Request) {
	req.Header.Set("User-Agent", getCurrentUA())
	req.Header.Set("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8")
	req.Header.Set("Accept-Language", "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7")
}

/* tenminutemailNetAjaxHeaders 设置 AJAX 请求头（用于 mailbox.ajax.php） */
func tenminutemailNetAjaxHeaders(req *http.Request) {
	req.Header.Set("User-Agent", getCurrentUA())
	req.Header.Set("Accept", "*/*")
	req.Header.Set("Accept-Language", "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7")
	req.Header.Set("X-Requested-With", "XMLHttpRequest")
	req.Header.Set("Referer", tenminutemailNetBaseURL+"/")
}

/* TenminutemailNetGenerate 创建 10minutemail.net 临时邮箱
 * 流程:
 *   1. GET / 获取 session cookie（PHPSESSID 由 cookie jar 自动保存）
 *   2. 从首页 HTML 的输入框正则提取随机分配的邮箱地址（value="xxx@xxx.com"）
 * token: 空字符串（session 由 cookie jar 维护，无需额外令牌）
 */
func TenminutemailNetGenerate() (*CreatedMailbox, error) {
	client := HTTPClient()

	req, err := http.NewRequest("GET", tenminutemailNetBaseURL+"/", nil)
	if err != nil {
		return nil, fmt.Errorf("10minutemail-net: 创建首页请求失败: %w", err)
	}
	tenminutemailNetBrowserHeaders(req)

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("10minutemail-net: 获取首页失败: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("10minutemail-net: 读取首页响应失败: %w", err)
	}

	if err := CheckHTTPStatus(resp, "10minutemail-net home"); err != nil {
		return nil, err
	}

	/* 从 HTML 中提取邮箱地址 */
	m := tenminutemailNetEmailRe.FindStringSubmatch(string(body))
	if len(m) < 2 {
		return nil, fmt.Errorf("10minutemail-net: 未能从首页提取邮箱地址")
	}
	emailAddr := strings.TrimSpace(m[1])
	if emailAddr == "" || !strings.Contains(emailAddr, "@") {
		return nil, fmt.Errorf("10minutemail-net: 获取到的邮箱地址无效: %q", emailAddr)
	}

	return &CreatedMailbox{
		Channel: "10minutemail-net",
		Email:   emailAddr,
		Token:   "",
	}, nil
}

/* TenminutemailNetGetEmails 获取 10minutemail.net 邮件列表
 * 流程:
 *   1. GET /mailbox.ajax.php?_={毫秒时间戳} 获取邮件列表（HTML 表格，需带创建时的 PHPSESSID cookie）
 *   2. 逐行解析 <tr>，提取邮件 ID（mid）、发件人、主题、收件时间、已读状态
 *   3. 对每封邮件 GET /readmail.html?mid={id} 获取整页并提取正文 HTML 片段
 * 表格列顺序: 寄件人 | 主题 | 收件日期；未读行的 <tr> 带 font-weight: bold 样式
 */
func TenminutemailNetGetEmails(email, token string) ([]NormEmail, error) {
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("10minutemail-net: 邮箱地址为空")
	}

	client := HTTPClient()

	/* GET /mailbox.ajax.php?_={毫秒时间戳} 获取邮件列表 */
	listURL := fmt.Sprintf("%s/mailbox.ajax.php?_=%d", tenminutemailNetBaseURL, time.Now().UnixMilli())
	req, err := http.NewRequest("GET", listURL, nil)
	if err != nil {
		return nil, fmt.Errorf("10minutemail-net: 创建邮件列表请求失败: %w", err)
	}
	tenminutemailNetAjaxHeaders(req)

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("10minutemail-net: 获取邮件列表失败: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("10minutemail-net: 读取邮件列表响应失败: %w", err)
	}

	if err := CheckHTTPStatus(resp, "10minutemail-net mailbox"); err != nil {
		return nil, err
	}

	/* 逐行解析表格 <tr> */
	rows := tenminutemailNetRowRe.FindAllStringSubmatch(string(body), -1)
	emails := make([]NormEmail, 0, len(rows))
	for _, row := range rows {
		rowFull := row[0]  /* 整行（含 <tr> 标签，用于判断未读样式） */
		rowInner := row[1] /* 行内内容 */

		/* 跳过表头行（含 <th>） */
		if strings.Contains(strings.ToLower(rowInner), "<th") {
			continue
		}

		/* 提取邮件 ID */
		midMatch := tenminutemailNetMidRe.FindStringSubmatch(rowInner)
		if len(midMatch) < 2 {
			continue
		}
		mailID := strings.TrimSpace(midMatch[1])
		if mailID == "" {
			continue
		}

		/* 提取三个单元格：发件人 | 主题 | 收件日期 */
		cells := tenminutemailNetCellRe.FindAllStringSubmatch(rowInner, -1)
		var fromCell, subjectCell, dateCell string
		if len(cells) > 0 {
			fromCell = cells[0][1]
		}
		if len(cells) > 1 {
			subjectCell = cells[1][1]
		}
		if len(cells) > 2 {
			dateCell = cells[2][1]
		}

		fromAddr := tenminutemailNetExtractText(fromCell)
		subject := tenminutemailNetExtractText(subjectCell)

		/* 收件时间优先取 span 的 title 属性（UTC 绝对时间），否则取单元格文本 */
		date := ""
		if tm := tenminutemailNetTitleRe.FindStringSubmatch(dateCell); len(tm) >= 2 {
			date = strings.TrimSpace(tm[1])
		} else {
			date = tenminutemailNetExtractText(dateCell)
		}

		/* 未读状态：行 <tr> 样式含 font-weight: bold */
		isRead := !strings.Contains(strings.ToLower(rowFull), "font-weight: bold")

		/* 获取邮件正文 HTML */
		htmlBody := tenminutemailNetFetchBody(client, mailID)

		flat := map[string]interface{}{
			"id":      mailID,
			"from":    fromAddr,
			"to":      email,
			"subject": subject,
			"html":    htmlBody,
			"date":    date,
			"isRead":  isRead,
		}
		emails = append(emails, NormalizeMap(flat, email))
	}

	return emails, nil
}

/* tenminutemailNetFetchBody 获取单封邮件的正文 HTML 片段
 * GET /readmail.html?mid={id} 返回整页 HTML，正文位于 <div class="mailinhtml">...</div>
 * 失败或未找到正文时返回空字符串（normalize 会从主题等字段兜底）
 */
func tenminutemailNetFetchBody(client interface {
	Do(*http.Request) (*http.Response, error)
}, mailID string) string {
	if mailID == "" {
		return ""
	}

	reqURL := fmt.Sprintf("%s/readmail.html?mid=%s", tenminutemailNetBaseURL, mailID)
	req, err := http.NewRequest("GET", reqURL, nil)
	if err != nil {
		return ""
	}
	tenminutemailNetBrowserHeaders(req)
	req.Header.Set("Referer", tenminutemailNetBaseURL+"/")

	resp, err := client.Do(req)
	if err != nil {
		return ""
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return ""
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return ""
	}

	return tenminutemailNetExtractBody(string(body))
}

/* tenminutemailNetExtractBody 从整页 HTML 提取正文片段
 * 正文包裹于 <div class="mailinhtml"> ... </div>，其内部存在嵌套 div，
 * 故以紧随其后的 email-decode.min.js script 标签作为结束锚点，再回退裁掉尾部的两个闭合 </div>。
 */
func tenminutemailNetExtractBody(page string) string {
	const startMark = `class="mailinhtml"`
	si := strings.Index(page, startMark)
	if si < 0 {
		return ""
	}
	/* 跳到 mailinhtml 这个 div 的 '>' 之后 */
	gt := strings.IndexByte(page[si:], '>')
	if gt < 0 {
		return ""
	}
	contentStart := si + gt + 1

	rest := page[contentStart:]
	/* 结束锚点：mailinhtml 区块后紧跟的 cloudflare email-decode 脚本 */
	ei := strings.Index(rest, "email-decode.min.js")
	if ei < 0 {
		/* 兜底：退化为该 div 后第一个 </div> */
		di := strings.Index(rest, "</div>")
		if di < 0 {
			return strings.TrimSpace(rest)
		}
		return strings.TrimSpace(rest[:di])
	}

	segment := rest[:ei]
	/* segment 末尾形如 "...正文</div></div><script ..."，裁掉结尾的 <script 起始与两个包裹 </div> */
	if sIdx := strings.LastIndex(segment, "<script"); sIdx >= 0 {
		segment = segment[:sIdx]
	}
	segment = strings.TrimSpace(segment)
	/* 去掉 mailinhtml 与其外层 tab1 的两个闭合 div */
	for i := 0; i < 2; i++ {
		segment = strings.TrimSpace(segment)
		if strings.HasSuffix(segment, "</div>") {
			segment = strings.TrimSuffix(segment, "</div>")
		}
	}
	return strings.TrimSpace(segment)
}

/* tenminutemailNetExtractText 从单元格 HTML 提取纯文本发件人/主题
 * 优先解码 Cloudflare 邮箱保护混淆（__cf_email__ + data-cfemail），否则去标签取文本。
 */
func tenminutemailNetExtractText(cell string) string {
	/* 优先解码 Cloudflare 邮箱保护混淆 */
	if cf := tenminutemailNetCFRe.FindStringSubmatch(cell); len(cf) >= 2 {
		if decoded := tenminutemailNetCFDecode(cf[1]); decoded != "" {
			return decoded
		}
	}
	/* 去除标签后反转义 HTML 实体 */
	text := tenminutemailNetTagRe.ReplaceAllString(cell, "")
	text = strings.ReplaceAll(text, "&nbsp;", " ")
	text = strings.ReplaceAll(text, "&#160;", " ")
	text = strings.ReplaceAll(text, "&amp;", "&")
	text = strings.ReplaceAll(text, "&lt;", "<")
	text = strings.ReplaceAll(text, "&gt;", ">")
	text = strings.ReplaceAll(text, "&quot;", "\"")
	return strings.TrimSpace(text)
}

/* tenminutemailNetCFDecode 解码 Cloudflare 邮箱保护混淆字符串
 * 算法：首字节为异或密钥，其后每字节与密钥异或还原 ASCII。
 * 解码失败返回空字符串。
 */
func tenminutemailNetCFDecode(encoded string) string {
	data, err := hex.DecodeString(encoded)
	if err != nil || len(data) < 2 {
		return ""
	}
	key := data[0]
	out := make([]byte, 0, len(data)-1)
	for _, b := range data[1:] {
		out = append(out, b^key)
	}
	decoded := string(out)
	/* 解码结果应为可读邮箱地址 */
	if !strings.Contains(decoded, "@") {
		return ""
	}
	return decoded
}
