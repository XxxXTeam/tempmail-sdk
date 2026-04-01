package tempemail

import (
	"fmt"
	"io"
	"net/mail"
	"regexp"
	"strconv"
	"strings"
	"time"

	fhttp "github.com/bogdanfinn/fhttp"
)

const (
	anonboxPageURL = "https://anonbox.net/en/"
	anonboxBase    = "https://anonbox.net"
)

var (
	anonboxMailLinkRe = regexp.MustCompile(`<a href="https://anonbox\.net/([^/]+)/([^"]+)">https://anonbox\.net/[^"]+</a>`)
	anonboxDdRe       = regexp.MustCompile(`(?is)<dd([^>]*)>([\s\S]*?)</dd>`)
	anonboxPRe        = regexp.MustCompile(`(?is)<p>([\s\S]*?)</p>`)
	anonboxTagRe      = regexp.MustCompile(`<[^>]+>`)
	anonboxExpiresRe  = regexp.MustCompile(`Your mail address is valid until:</dt>\s*<dd><p>([^<]+)</p>`)
	anonboxMboxSplit  = regexp.MustCompile(`\r?\n(?=From )`)
)

func anonboxStripTags(html string) string {
	s := anonboxTagRe.ReplaceAllString(html, "")
	s = strings.ReplaceAll(s, "&nbsp;", " ")
	s = strings.ReplaceAll(s, "&amp;", "&")
	s = strings.ReplaceAll(s, "&lt;", "<")
	s = strings.ReplaceAll(s, "&gt;", ">")
	return strings.TrimSpace(s)
}

func anonboxSimpleHash(s string) string {
	var h uint32
	for i := 0; i < len(s); i++ {
		h = h*31 + uint32(s[i])
	}
	return strconv.FormatUint(uint64(h), 36)
}

func anonboxParseEnPage(html string) (email string, token string, expires string, err error) {
	m := anonboxMailLinkRe.FindStringSubmatch(html)
	if m == nil {
		return "", "", "", fmt.Errorf("anonbox: mailbox link not found")
	}
	inbox, secret := m[1], m[2]
	token = inbox + "/" + secret

	var addressHTML string
	for _, sm := range anonboxDdRe.FindAllStringSubmatch(html, -1) {
		attrs, inner := sm[1], sm[2]
		if regexp.MustCompile(`display\s*:\s*none`).MatchString(attrs) {
			continue
		}
		pm := anonboxPRe.FindStringSubmatch(inner)
		if pm == nil {
			continue
		}
		pInner := pm[1]
		if !strings.Contains(pInner, "@") {
			continue
		}
		if strings.Contains(strings.ToLower(pInner), "googlemail.com") {
			continue
		}
		if !strings.Contains(strings.ToLower(pInner), "anonbox") {
			continue
		}
		addressHTML = pInner
		break
	}
	if addressHTML == "" {
		return "", "", "", fmt.Errorf("anonbox: address paragraph not found")
	}
	merged := anonboxStripTags(addressHTML)
	at := strings.Index(merged, "@")
	if at < 0 {
		return "", "", "", fmt.Errorf("anonbox: bad address")
	}
	local := strings.TrimSpace(merged[:at])
	if local == "" {
		return "", "", "", fmt.Errorf("anonbox: empty local part")
	}
	email = fmt.Sprintf("%s@%s.anonbox.net", local, inbox)
	if em := anonboxExpiresRe.FindStringSubmatch(html); len(em) > 1 {
		expires = strings.TrimSpace(em[1])
	}
	return email, token, expires, nil
}

func anonboxDecodeQP(body, headerBlock string) string {
	te := regexp.MustCompile(`(?i)^content-transfer-encoding:\s*([^\s]+)`).FindStringSubmatch(headerBlock)
	if len(te) < 2 || strings.ToLower(strings.TrimSpace(te[1])) != "quoted-printable" {
		return strings.TrimRight(body, "\r\n")
	}
	s := regexp.MustCompile(`=\r?\n`).ReplaceAllString(body, "")
	var b strings.Builder
	for i := 0; i < len(s); i++ {
		if s[i] == '=' && i+2 < len(s) {
			a, b0 := s[i+1], s[i+2]
			if ((a >= '0' && a <= '9') || (a >= 'A' && a <= 'F') || (a >= 'a' && a <= 'f')) &&
				((b0 >= '0' && b0 <= '9') || (b0 >= 'A' && b0 <= 'F') || (b0 >= 'a' && b0 <= 'f')) {
				v, _ := strconv.ParseUint(s[i+1:i+3], 16, 8)
				b.WriteByte(byte(v))
				i += 2
				continue
			}
		}
		b.WriteByte(s[i])
	}
	return strings.TrimRight(b.String(), "\r\n")
}

func anonboxMboxBlockToRaw(block string, recipient string) map[string]interface{} {
	normalized := strings.ReplaceAll(block, "\r\n", "\n")
	lines := strings.Split(normalized, "\n")
	i := 0
	if len(lines) > 0 && strings.HasPrefix(lines[0], "From ") {
		i = 1
	}
	headers := make(map[string]string)
	var curKey string
	for ; i < len(lines); i++ {
		line := lines[i]
		if line == "" {
			i++
			break
		}
		if len(line) > 0 && (line[0] == ' ' || line[0] == '\t') && curKey != "" {
			headers[curKey] += " " + strings.TrimSpace(line)
			continue
		}
		idx := strings.Index(line, ":")
		if idx > 0 {
			curKey = strings.ToLower(strings.TrimSpace(line[:idx]))
			headers[curKey] = strings.TrimSpace(line[idx+1:])
		}
	}
	body := strings.Join(lines[i:], "\n")

	ct := strings.ToLower(headers["content-type"])
	text := ""
	html := ""
	if strings.Contains(ct, "multipart/") {
		bm := regexp.MustCompile(`(?i)boundary="?([^";\s]+)"?`).FindStringSubmatch(headers["content-type"])
		if len(bm) > 1 {
			boundary := regexp.QuoteMeta(bm[1])
			partRe := regexp.MustCompile("\r?\n--" + boundary + "(?:--)?\r?\n")
			parts := partRe.Split(body, -1)
			for _, part := range parts {
				part = strings.TrimSpace(part)
				if part == "" || part == "--" {
					continue
				}
				sep := strings.Index(part, "\n\n")
				if sep < 0 {
					continue
				}
				ph := part[:sep]
				pb := part[sep+2:]
				pctM := regexp.MustCompile(`(?i)^content-type:\s*([^\s;]+)`).FindStringSubmatch(ph)
				pct := ""
				if len(pctM) > 1 {
					pct = strings.ToLower(strings.TrimSpace(pctM[1]))
				}
				if pct == "text/plain" {
					text = anonboxDecodeQP(pb, ph)
				} else if pct == "text/html" {
					html = anonboxDecodeQP(pb, ph)
				}
			}
		}
	}
	if text == "" && html == "" {
		if strings.Contains(ct, "text/html") {
			html = anonboxDecodeQP(body, headers["content-type"])
		} else {
			text = anonboxDecodeQP(body, headers["content-type"])
		}
	}

	from := headers["from"]
	to := headers["to"]
	if to == "" {
		to = recipient
	}
	subj := headers["subject"]
	dateStr := headers["date"]
	if dateStr != "" {
		if t, err := mail.ParseDate(dateStr); err == nil {
			dateStr = t.UTC().Format("2006-01-02T15:04:05Z07:00")
		}
	}
	if dateStr == "" {
		dateStr = time.Now().UTC().Format(time.RFC3339)
	}
	id := headers["message-id"]
	if id == "" {
		end := 512
		if len(block) < end {
			end = len(block)
		}
		id = anonboxSimpleHash(block[:end])
	}

	return map[string]interface{}{
		"id":          id,
		"from":        from,
		"to":          to,
		"subject":     subj,
		"body_text":   text,
		"body_html":   html,
		"date":        dateStr,
		"isRead":      false,
		"attachments": []interface{}{},
	}
}

func anonboxGenerate() (*EmailInfo, error) {
	req, err := fhttp.NewRequest("GET", anonboxPageURL, nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("User-Agent", GetCurrentUA())
	req.Header.Set("Accept", "text/html,application/xhtml+xml")

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("anonbox: generate HTTP %d", resp.StatusCode)
	}
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	email, token, expires, err := anonboxParseEnPage(string(body))
	if err != nil {
		return nil, err
	}
	info := &EmailInfo{
		Channel: ChannelAnonbox,
		Email:   email,
		token:   token,
	}
	if expires != "" {
		info.ExpiresAt = expires
	}
	return info, nil
}

func anonboxGetEmails(token, email string) ([]Email, error) {
	if token == "" {
		return nil, fmt.Errorf("internal error: token missing for anonbox")
	}
	path := token
	if !strings.HasSuffix(path, "/") {
		path += "/"
	}
	url := anonboxBase + "/" + path

	req, err := fhttp.NewRequest("GET", url, nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("User-Agent", GetCurrentUA())
	req.Header.Set("Accept", "text/plain,*/*")

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if resp.StatusCode == 404 {
		return []Email{}, nil
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("anonbox: get emails HTTP %d", resp.StatusCode)
	}
	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	t := strings.TrimSpace(string(raw))
	if t == "" {
		return []Email{}, nil
	}
	blocks := anonboxMboxSplit.Split(t, -1)
	out := make([]Email, 0, len(blocks))
	for _, b := range blocks {
		b = strings.TrimSpace(b)
		if b == "" {
			continue
		}
		out = append(out, normalizeRawEmail(anonboxMboxBlockToRaw(b, email), email))
	}
	return out, nil
}
