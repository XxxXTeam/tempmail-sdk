package provider

import (
	"encoding/base64"
	"encoding/json"
	"fmt"
	"io"
	"net/url"
	"regexp"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

/* tempemail.info 临时邮箱服务商
 * 流程：GET / 获取 PHPSESSID 会话 Cookie，并从 HTML 中解析 base64 编码的邮箱地址
 *       POST /template/checker.php（last_id 增量）获取邮件列表（JSON 数组）
 *       GET /view/{date} 获取单封邮件 HTML 正文
 * token 存储会话 Cookie 串，用于后续请求绑定收件箱
 */

const tempemailInfoBaseURL = "https://tempemail.info"

/* 匹配首页 HTML 中的 var emailEncoded = "base64..." */
var tempemailInfoEmailRe = regexp.MustCompile(`var\s+emailEncoded\s*=\s*"([^"]+)"`)

/* tempemailInfoSetHeaders 设置通用请求头 */
func tempemailInfoSetHeaders(req *http.Request) {
	req.Header.Set("User-Agent", getCurrentUA())
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
	req.Header.Set("Referer", tempemailInfoBaseURL+"/")
	req.Header.Set("Origin", tempemailInfoBaseURL)
}

/* TempemailInfoGenerate 创建 tempemail.info 临时邮箱
 * 1. GET / 获取 PHPSESSID 会话 Cookie
 * 2. 从 HTML 中正则提取 emailEncoded 并 base64 解码得到邮箱地址
 * token 存储会话 Cookie 串，供后续 checker.php / view 请求使用
 */
func TempemailInfoGenerate() (*CreatedMailbox, error) {
	req, err := http.NewRequest("GET", tempemailInfoBaseURL+"/", nil)
	if err != nil {
		return nil, fmt.Errorf("tempemail-info: 创建首页请求失败: %w", err)
	}
	tempemailInfoSetHeaders(req)
	req.Header.Set("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8")

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, fmt.Errorf("tempemail-info: 获取首页失败: %w", err)
	}
	defer resp.Body.Close()

	if err := CheckHTTPStatus(resp, "tempemail-info home"); err != nil {
		return nil, err
	}

	/* 提取会话 Cookie，绑定后续请求的收件箱 */
	cookieHdr := moaktMergeCookies("", resp.Cookies())
	if cookieHdr == "" {
		return nil, fmt.Errorf("tempemail-info: 未获取到会话 Cookie")
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("tempemail-info: 读取首页响应失败: %w", err)
	}

	/* 从 HTML 中正则提取 base64 编码的邮箱地址 */
	m := tempemailInfoEmailRe.FindSubmatch(body)
	if m == nil {
		return nil, fmt.Errorf("tempemail-info: 未在页面中找到 emailEncoded")
	}
	decoded, err := base64.StdEncoding.DecodeString(string(m[1]))
	if err != nil {
		return nil, fmt.Errorf("tempemail-info: 邮箱地址 base64 解码失败: %w", err)
	}
	email := strings.TrimSpace(string(decoded))
	if email == "" || !strings.Contains(email, "@") {
		return nil, fmt.Errorf("tempemail-info: 解码出的邮箱地址无效")
	}

	return &CreatedMailbox{
		Channel: "tempemail-info",
		Email:   email,
		Token:   cookieHdr,
	}, nil
}

/* TempemailInfoGetEmails 获取 tempemail.info 邮件列表
 * 1. POST /template/checker.php（body: last_id=0）获取邮件列表 JSON 数组
 * 2. 对每封邮件 GET /view/{date} 获取 HTML 正文
 */
func TempemailInfoGetEmails(email, token string) ([]NormEmail, error) {
	cookieHdr := strings.TrimSpace(token)
	if cookieHdr == "" {
		return nil, fmt.Errorf("tempemail-info: 缺少会话 Cookie")
	}

	form := url.Values{}
	form.Set("last_id", "0")

	req, err := http.NewRequest("POST", tempemailInfoBaseURL+"/template/checker.php", strings.NewReader(form.Encode()))
	if err != nil {
		return nil, fmt.Errorf("tempemail-info: 创建获取邮件请求失败: %w", err)
	}
	tempemailInfoSetHeaders(req)
	req.Header.Set("Accept", "application/json, text/javascript, */*; q=0.01")
	req.Header.Set("X-Requested-With", "XMLHttpRequest")
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	req.Header.Set("Cookie", cookieHdr)

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, fmt.Errorf("tempemail-info: 获取邮件列表失败: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("tempemail-info: 读取邮件列表响应失败: %w", err)
	}

	if err := CheckHTTPStatus(resp, "tempemail-info checker"); err != nil {
		return nil, err
	}

	/* checker.php 返回邮件对象数组，字段：id / name / from / subject / date / read
	 * 其中 date 既是显示日期，又是 /view/{date} 正文接口的路径键
	 */
	var rows []struct {
		ID      json.Number `json:"id"`
		Name    string      `json:"name"`
		From    string      `json:"from"`
		Subject string      `json:"subject"`
		Date    string      `json:"date"`
		Read    bool        `json:"read"`
	}
	if err := json.Unmarshal(body, &rows); err != nil {
		return nil, fmt.Errorf("tempemail-info: 解析邮件列表失败: %w", err)
	}

	if len(rows) == 0 {
		return []NormEmail{}, nil
	}

	emails := make([]NormEmail, 0, len(rows))
	for _, row := range rows {
		/* 构造发件人地址：有显示名则格式化为 "name <email>" */
		fromAddr := row.From
		if row.Name != "" && row.Name != row.From {
			fromAddr = fmt.Sprintf("%s <%s>", row.Name, row.From)
		}

		/* GET /view/{date} 获取邮件正文 */
		htmlBody := tempemailInfoFetchBody(cookieHdr, row.Date)

		flat := map[string]interface{}{
			"id":      row.ID.String(),
			"from":    fromAddr,
			"to":      email,
			"subject": row.Subject,
			"date":    row.Date,
			"html":    htmlBody,
			"isRead":  row.Read,
		}
		emails = append(emails, NormalizeMap(flat, email))
	}

	return emails, nil
}

/* tempemailInfoFetchBody 获取单封邮件的 HTML 正文
 * date 作为路径段需进行 URL 编码
 */
func tempemailInfoFetchBody(cookieHdr, date string) string {
	if strings.TrimSpace(date) == "" {
		return ""
	}

	req, err := http.NewRequest("GET", tempemailInfoBaseURL+"/view/"+url.PathEscape(date), nil)
	if err != nil {
		return ""
	}
	tempemailInfoSetHeaders(req)
	req.Header.Set("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8")
	req.Header.Set("Cookie", cookieHdr)

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return ""
	}
	defer resp.Body.Close()

	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		io.Copy(io.Discard, resp.Body)
		return ""
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return ""
	}
	return string(body)
}
