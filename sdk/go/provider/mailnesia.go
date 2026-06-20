package provider

import (
	"fmt"
	"html"
	"io"
	"math/rand"
	"net/url"
	"regexp"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

const mailnesiaBase = "https://mailnesia.com"
const mailnesiaDomain = "mailnesia.com"

var (
	mailnesiaRowRe    = regexp.MustCompile(`(?is)<tr\s+id="([^"]+)"[^>]*class="[^"]*\bemailheader\b[^"]*"[^>]*>(.*?)</tr>`)
	mailnesiaTimeRe   = regexp.MustCompile(`(?is)<time\s+datetime="([^"]+)"`)
	mailnesiaAnchorRe = regexp.MustCompile(`(?is)<a\b[^>]*class="email"[^>]*>(.*?)</a>`)
	mailnesiaTagRe    = regexp.MustCompile(`(?is)<[^>]+>`)
)

func mailnesiaRandomLocal() string {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	b := make([]byte, 19)
	copy(b, "sdk")
	for i := 3; i < len(b); i++ {
		b[i] = chars[rand.Intn(len(chars))]
	}
	return string(b)
}

func mailnesiaLocalPart(email string) string {
	parts := strings.Split(strings.TrimSpace(email), "@")
	if len(parts) == 0 {
		return ""
	}
	return strings.TrimSpace(parts[0])
}

func mailnesiaMailboxURL(local string) string {
	return fmt.Sprintf("%s/mailbox/%s", mailnesiaBase, url.PathEscape(local))
}

func mailnesiaDetailURL(local, id string) string {
	return fmt.Sprintf("%s/%s", mailnesiaMailboxURL(local), url.PathEscape(id))
}

func mailnesiaGetText(u string) (string, error) {
	req, err := http.NewRequest("GET", u, nil)
	if err != nil {
		return "", err
	}
	req.Header.Set("Accept", "text/html,*/*")
	resp, err := HTTPClient().Do(req)
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", err
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return "", fmt.Errorf("mailnesia: http %d", resp.StatusCode)
	}
	return string(body), nil
}

func mailnesiaCleanText(raw string) string {
	stripped := mailnesiaTagRe.ReplaceAllString(raw, " ")
	return strings.Join(strings.Fields(html.UnescapeString(stripped)), " ")
}

func mailnesiaParseRows(page string) []map[string]any {
	matches := mailnesiaRowRe.FindAllStringSubmatch(page, -1)
	rows := make([]map[string]any, 0, len(matches))
	for _, m := range matches {
		id := strings.TrimSpace(m[1])
		rowHTML := m[2]
		date := ""
		if dm := mailnesiaTimeRe.FindStringSubmatch(rowHTML); len(dm) == 2 {
			date = html.UnescapeString(strings.TrimSpace(dm[1]))
		}
		anchors := mailnesiaAnchorRe.FindAllStringSubmatch(rowHTML, -1)
		if len(anchors) < 3 {
			continue
		}
		rows = append(rows, map[string]any{
			"id":      id,
			"date":    date,
			"from":    mailnesiaCleanText(anchors[0][1]),
			"to":      mailnesiaCleanText(anchors[1][1]),
			"subject": mailnesiaCleanText(anchors[2][1]),
		})
	}
	return rows
}

func mailnesiaExtractDivByID(page, id, nextID string) string {
	needle := `id="` + id + `"`
	idAt := strings.Index(page, needle)
	if idAt < 0 {
		return ""
	}
	openEnd := strings.Index(page[idAt:], ">")
	if openEnd < 0 {
		return ""
	}
	start := idAt + openEnd + 1
	end := -1
	if nextID != "" {
		nextNeedle := `<div id="` + nextID + `"`
		if n := strings.Index(page[start:], nextNeedle); n >= 0 {
			end = start + n
		}
	}
	if end < 0 {
		if n := strings.Index(page[start:], "</div>"); n >= 0 {
			end = start + n + len("</div>")
		}
	}
	if end < 0 {
		return ""
	}
	content := strings.TrimSpace(page[start:end])
	content = strings.TrimSpace(strings.TrimSuffix(content, "</div>"))
	return content
}

func mailnesiaExtractPlain(page, id string) string {
	re := regexp.MustCompile(`(?is)<div\s+id="text_plain_` + regexp.QuoteMeta(id) + `"[^>]*>\s*<pre>(.*?)</pre>\s*</div>`)
	m := re.FindStringSubmatch(page)
	if len(m) != 2 {
		return ""
	}
	return strings.TrimSpace(html.UnescapeString(m[1]))
}

func mailnesiaFetchDetail(local string, row map[string]any) map[string]any {
	id := strings.TrimSpace(fmt.Sprint(row["id"]))
	if id == "" {
		return row
	}
	page, err := mailnesiaGetText(mailnesiaDetailURL(local, id))
	if err != nil {
		return row
	}
	flat := make(map[string]any, len(row)+2)
	for k, v := range row {
		flat[k] = v
	}
	flat["text"] = mailnesiaExtractPlain(page, id)
	flat["html"] = mailnesiaExtractDivByID(page, "text_html_"+id, "text_plain_"+id)
	return flat
}

func MailnesiaGenerate() (*CreatedMailbox, error) {
	local := mailnesiaRandomLocal()
	if _, err := mailnesiaGetText(mailnesiaMailboxURL(local)); err != nil {
		return nil, err
	}
	return &CreatedMailbox{Channel: "mailnesia", Email: local + "@" + mailnesiaDomain}, nil
}

func MailnesiaGetEmails(email string) ([]NormEmail, error) {
	local := mailnesiaLocalPart(email)
	if local == "" {
		return nil, fmt.Errorf("mailnesia: empty email")
	}
	page, err := mailnesiaGetText(mailnesiaMailboxURL(local))
	if err != nil {
		return nil, err
	}
	rows := mailnesiaParseRows(page)
	out := make([]NormEmail, 0, len(rows))
	for _, row := range rows {
		out = append(out, NormalizeMap(mailnesiaFetchDetail(local, row), email))
	}
	return out, nil
}
