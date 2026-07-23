package main

import (
	"crypto/tls"
	"fmt"
	"math/rand"
	"net/smtp"
	"strings"
	"time"
)

/*
 * SMTP 发信配置（腾讯企业邮，隐式 TLS 465）
 * 该账号由用户提供，用于向临时邮箱渠道投递真实测试邮件以验证收信能力。
 */
const (
	smtpHost = "smtp.exmail.qq.com"
	smtpPort = 465
	smtpUser = "supper@openel.top"
	smtpPass = "PKZT5rgvUvGdgcxe"
	smtpFrom = "supper@openel.top"
)

/*
 * payloadKind 测试载荷类型
 * text  —— 纯文本 + 验证码，验证最基础的正文与验证码提取
 * html  —— 复杂大 HTML（表格 + 内联 CSS + 内联图片 + 长文），验证大/复杂正文是否被截断
 * attach—— 带附件，验证附件字段是否被正确归一化
 */
type payloadKind string

const (
	kindText   payloadKind = "text"
	kindHTML   payloadKind = "html"
	kindAttach payloadKind = "attach"
)

/*
 * builtMessage 单封待发送邮件的构造结果
 * token 为该封邮件的唯一标记，用于收信端按 subject 精确匹配；
 * sentHTMLLen 记录发送的 HTML 字节长度，用于收信端判断是否被截断；
 * code 为验证码，sentinel 为 HTML 末尾哨兵。
 */
type builtMessage struct {
	kind        payloadKind
	token       string
	subject     string
	code        string
	sentinel    string
	rawMIME     []byte
	sentHTMLLen int
	sentText    string
}

/* randToken 生成 10 位十六进制唯一标记 */
func randToken(rng *rand.Rand) string {
	const hexc = "0123456789abcdef"
	b := make([]byte, 10)
	for i := range b {
		b[i] = hexc[rng.Intn(16)]
	}
	return string(b)
}

/*
 * buildMessage 根据渠道地址与载荷类型构造一封完整 MIME 邮件
 * @param to 收件人（临时邮箱地址）
 * @param kind 载荷类型
 * @param rng 随机源（调用方持有，避免全局锁）
 */
func buildMessage(to string, kind payloadKind, rng *rand.Rand) builtMessage {
	token := randToken(rng)
	code := fmt.Sprintf("%06d", rng.Intn(1000000))
	sentinel := "END-SENTINEL-" + token
	subject := "TempMailVerify " + string(kind) + " " + token
	date := time.Now().Format(time.RFC1123Z)

	msg := builtMessage{kind: kind, token: token, subject: subject, code: code, sentinel: sentinel}

	headers := func(contentType string) string {
		var b strings.Builder
		b.WriteString("From: TempMail Verify <" + smtpFrom + ">\r\n")
		b.WriteString("To: <" + to + ">\r\n")
		b.WriteString("Subject: " + subject + "\r\n")
		b.WriteString("Date: " + date + "\r\n")
		b.WriteString("MIME-Version: 1.0\r\n")
		b.WriteString("Message-ID: <" + token + "@openel.top>\r\n")
		b.WriteString(contentType)
		return b.String()
	}

	switch kind {
	case kindText:
		text := fmt.Sprintf("这是一封纯文本验证邮件。\r\n验证码: %s-%s\r\n请忽略本邮件。token=%s\r\n", code, token, token)
		msg.sentText = text
		body := headers("Content-Type: text/plain; charset=UTF-8\r\n\r\n") + text
		msg.rawMIME = []byte(body)

	case kindHTML:
		htmlBody := buildLargeHTML(token, code, sentinel)
		textAlt := fmt.Sprintf("复杂 HTML 邮件的纯文本替代。验证码 %s token=%s\r\n", code, token)
		msg.sentText = textAlt
		msg.sentHTMLLen = len(htmlBody)
		boundary := "b_" + token
		var b strings.Builder
		b.WriteString(headers("Content-Type: multipart/alternative; boundary=\"" + boundary + "\"\r\n\r\n"))
		b.WriteString("--" + boundary + "\r\n")
		b.WriteString("Content-Type: text/plain; charset=UTF-8\r\n\r\n")
		b.WriteString(textAlt + "\r\n")
		b.WriteString("--" + boundary + "\r\n")
		b.WriteString("Content-Type: text/html; charset=UTF-8\r\n\r\n")
		b.WriteString(htmlBody + "\r\n")
		b.WriteString("--" + boundary + "--\r\n")
		msg.rawMIME = []byte(b.String())

	case kindAttach:
		text := fmt.Sprintf("带附件的验证邮件。验证码 %s token=%s\r\n", code, token)
		msg.sentText = text
		htmlBody := fmt.Sprintf("<html><body><p>带附件验证邮件 code=%s token=%s</p></body></html>", code, token)
		msg.sentHTMLLen = len(htmlBody)
		boundary := "mix_" + token
		altBoundary := "alt_" + token
		attName := "verify-" + token + ".txt"
		attContent := "附件内容 token=" + token + " code=" + code + "\r\n" + strings.Repeat("ATTACH-LINE\r\n", 20)
		attB64 := b64Wrap(attContent)
		var b strings.Builder
		b.WriteString(headers("Content-Type: multipart/mixed; boundary=\"" + boundary + "\"\r\n\r\n"))
		b.WriteString("--" + boundary + "\r\n")
		b.WriteString("Content-Type: multipart/alternative; boundary=\"" + altBoundary + "\"\r\n\r\n")
		b.WriteString("--" + altBoundary + "\r\n")
		b.WriteString("Content-Type: text/plain; charset=UTF-8\r\n\r\n")
		b.WriteString(text + "\r\n")
		b.WriteString("--" + altBoundary + "\r\n")
		b.WriteString("Content-Type: text/html; charset=UTF-8\r\n\r\n")
		b.WriteString(htmlBody + "\r\n")
		b.WriteString("--" + altBoundary + "--\r\n")
		b.WriteString("--" + boundary + "\r\n")
		b.WriteString("Content-Type: text/plain; charset=UTF-8; name=\"" + attName + "\"\r\n")
		b.WriteString("Content-Transfer-Encoding: base64\r\n")
		b.WriteString("Content-Disposition: attachment; filename=\"" + attName + "\"\r\n\r\n")
		b.WriteString(attB64 + "\r\n")
		b.WriteString("--" + boundary + "--\r\n")
		msg.rawMIME = []byte(b.String())
	}
	return msg
}

/* htmlRows 控制大 HTML 的表格行数（体积），由命令行 -htmlrows 注入，默认 400 行≈112KB */
var htmlRows = 400

/*
 * buildLargeHTML 构造复杂 HTML 正文（体积由 htmlRows 控制）
 * 含内联 CSS、内联 base64 图片、大表格与长段落，末尾放置可见哨兵，
 * 用于验证渠道读信 API 是否会截断/丢弃复杂 HTML。
 */
func buildLargeHTML(token, code, sentinel string) string {
	var b strings.Builder
	b.WriteString("<!DOCTYPE html><html><head><meta charset=\"UTF-8\">")
	b.WriteString("<style>body{font-family:Arial;color:#222}table{border-collapse:collapse;width:100%}")
	b.WriteString("td,th{border:1px solid #ccc;padding:6px}.hl{background:#f5f5f5}</style></head><body>")
	b.WriteString("<h1>复杂 HTML 验证邮件</h1>")
	b.WriteString(fmt.Sprintf("<p>验证码 <b>%s</b> token=%s</p>", code, token))
	// 内联小图（1x1 png data uri）
	b.WriteString("<img alt=\"px\" src=\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==\">")
	b.WriteString("<table><thead><tr><th>#</th><th>列A</th><th>列B</th><th>列C</th><th>说明</th></tr></thead><tbody>")
	// 生成足够多行让整体 >50KB
	for i := 0; i < htmlRows; i++ {
		cls := ""
		if i%2 == 0 {
			cls = " class=\"hl\""
		}
		b.WriteString(fmt.Sprintf("<tr%s><td>%d</td><td>数据单元格 %d-A</td><td>数据单元格 %d-B 内容内容内容</td><td>%s</td><td>这是第 %d 行的较长说明文字，用于撑大 HTML 体积并模拟真实营销邮件中的复杂结构与长文本内容。</td></tr>", cls, i, i, i, token, i))
	}
	b.WriteString("</tbody></table>")
	for i := 0; i < 30; i++ {
		b.WriteString(fmt.Sprintf("<p>段落 %d：这是一段用于填充体积的长文本，包含 token=%s，反复出现以确保正文足够大且结构复杂，从而检验读信端对大 HTML 的完整保留能力。</p>", i, token))
	}
	/*
	 * 哨兵放入可见文本节点（而非 HTML 注释）：
	 * 部分渠道会对 HTML 做 sanitize 清洗并移除注释，若哨兵在注释里会被误删，
	 * 导致"正文完整却判为截断"的假阳性。放在可见 <div> 文本中可被 sanitize 保留。
	 */
	b.WriteString("<div id=\"tail\">" + sentinel + "</div></body></html>")
	return b.String()
}

/* b64Wrap 将内容 base64 编码并按 76 列换行（RFC 2045） */
func b64Wrap(s string) string {
	const b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
	data := []byte(s)
	var out strings.Builder
	for i := 0; i < len(data); i += 3 {
		var n uint32
		var pad int
		n = uint32(data[i]) << 16
		if i+1 < len(data) {
			n |= uint32(data[i+1]) << 8
		} else {
			pad++
		}
		if i+2 < len(data) {
			n |= uint32(data[i+2])
		} else {
			pad++
		}
		out.WriteByte(b64[(n>>18)&63])
		out.WriteByte(b64[(n>>12)&63])
		if pad < 2 {
			out.WriteByte(b64[(n>>6)&63])
		} else {
			out.WriteByte('=')
		}
		if pad < 1 {
			out.WriteByte(b64[n&63])
		} else {
			out.WriteByte('=')
		}
	}
	// 按 76 列折行
	raw := out.String()
	var wrapped strings.Builder
	for i := 0; i < len(raw); i += 76 {
		end := i + 76
		if end > len(raw) {
			end = len(raw)
		}
		wrapped.WriteString(raw[i:end])
		wrapped.WriteString("\r\n")
	}
	return wrapped.String()
}

/*
 * sendMail 通过腾讯企业邮 SMTP（隐式 TLS 465）发送一封已构造好的 MIME 邮件
 * 标准库 net/smtp 不直接支持 465 隐式 TLS，故手动 tls.Dial 后再建立 SMTP 会话。
 */
func sendMail(to string, raw []byte) error {
	addr := fmt.Sprintf("%s:%d", smtpHost, smtpPort)
	conn, err := tls.Dial("tcp", addr, &tls.Config{ServerName: smtpHost})
	if err != nil {
		return fmt.Errorf("tls dial: %w", err)
	}
	defer conn.Close()

	c, err := smtp.NewClient(conn, smtpHost)
	if err != nil {
		return fmt.Errorf("smtp client: %w", err)
	}
	defer c.Close()

	auth := smtp.PlainAuth("", smtpUser, smtpPass, smtpHost)
	if err := c.Auth(auth); err != nil {
		return fmt.Errorf("auth: %w", err)
	}
	if err := c.Mail(smtpFrom); err != nil {
		return fmt.Errorf("mail from: %w", err)
	}
	if err := c.Rcpt(to); err != nil {
		return fmt.Errorf("rcpt to: %w", err)
	}
	w, err := c.Data()
	if err != nil {
		return fmt.Errorf("data: %w", err)
	}
	if _, err := w.Write(raw); err != nil {
		return fmt.Errorf("write: %w", err)
	}
	if err := w.Close(); err != nil {
		return fmt.Errorf("data close: %w", err)
	}
	return c.Quit()
}
