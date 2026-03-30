package tempemail

import (
	"fmt"
	"html"
	"io"
	"regexp"
	"strings"
	"time"

	http "github.com/bogdanfinn/fhttp"
)

const linshiyouOrigin = "https://linshiyou.com"

var linshiyouHeaders = map[string]string{
	"accept-language":    "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
	"cache-control":      "no-cache",
	"dnt":                "1",
	"pragma":             "no-cache",
	"priority":           "u=1, i",
	"referer":            linshiyouOrigin + "/",
	"sec-ch-ua":          `"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"`,
	"sec-ch-ua-mobile":   "?0",
	"sec-ch-ua-platform": `"Windows"`,
	"sec-fetch-dest":     "empty",
	"sec-fetch-mode":     "cors",
	"sec-fetch-site":     "same-origin",
}

func setLinshiyouHeaders(req *http.Request) {
	for k, v := range linshiyouHeaders {
		req.Header.Set(k, v)
	}
	req.Header.Set("User-Agent", GetCurrentUA())
}

var (
	reLinshiyouListID   = regexp.MustCompile(`id="tmail-email-list-([a-f0-9]+)"`)
	reLinshiyouDivClass = regexp.MustCompile(`(?i)<div class="([^"]+)"[^>]*>([\s\S]*?)</div>`)
	reLinshiyouSrcdoc   = regexp.MustCompile(`(?i)\ssrcdoc="([^"]*)"`)
	reLinshiyouDownload = regexp.MustCompile(`href="(/api/download\?id=[^"]+)"`)
)

func linshiyouStripTags(s string) string {
	re := regexp.MustCompile(`<[^>]+>`)
	t := re.ReplaceAllString(s, " ")
	return strings.Join(strings.Fields(t), " ")
}

func linshiyouPickDiv(htmlFragment, className string) string {
	matches := reLinshiyouDivClass.FindAllStringSubmatch(htmlFragment, -1)
	for _, m := range matches {
		if len(m) >= 3 && m[1] == className {
			inner := m[2]
			return html.UnescapeString(strings.TrimSpace(linshiyouStripTags(inner)))
		}
	}
	return ""
}

func linshiyouParseDate(s string) string {
	s = strings.TrimSpace(s)
	if s == "" {
		return ""
	}
	layouts := []string{"2006-01-02 15:04:05", "2006-01-02T15:04:05", time.RFC3339}
	for _, layout := range layouts {
		if d, err := time.Parse(layout, s); err == nil {
			return d.UTC().Format(time.RFC3339)
		}
	}
	return ""
}

func linshiyouExtractSrcdoc(bodyPart string) string {
	m := reLinshiyouSrcdoc.FindStringSubmatch(bodyPart)
	if len(m) < 2 {
		return ""
	}
	return html.UnescapeString(m[1])
}

func linshiyouParseMailSegments(raw, recipientEmail string) []Email {
	raw = strings.TrimSpace(raw)
	if raw == "" {
		return nil
	}
	parts := strings.Split(raw, "<-----TMAILNEXTMAIL----->")
	var out []Email
	for _, seg := range parts {
		seg = strings.TrimSpace(seg)
		if seg == "" {
			continue
		}
		chop := strings.SplitN(seg, "<-----TMAILCHOPPER----->", 2)
		listPart := chop[0]
		bodyPart := ""
		if len(chop) > 1 {
			bodyPart = chop[1]
		}
		idm := reLinshiyouListID.FindStringSubmatch(listPart)
		if len(idm) < 2 {
			continue
		}
		id := idm[1]

		fromList := linshiyouPickDiv(listPart, "name")
		subjectList := linshiyouPickDiv(listPart, "subject")
		previewList := linshiyouPickDiv(listPart, "body")

		fromBody := linshiyouPickDiv(bodyPart, "tmail-email-sender")
		timeBody := linshiyouPickDiv(bodyPart, "tmail-email-time")
		titleBody := linshiyouPickDiv(bodyPart, "tmail-email-title")
		htmlBody := linshiyouExtractSrcdoc(bodyPart)

		from := fromBody
		if from == "" {
			from = fromList
		}
		subject := titleBody
		if subject == "" {
			subject = subjectList
		}
		text := previewList
		if text == "" {
			text = strings.TrimSpace(linshiyouStripTags(htmlBody))
		}

		var attachments []EmailAttachment
		if dm := reLinshiyouDownload.FindStringSubmatch(bodyPart); len(dm) >= 2 {
			attachments = append(attachments, EmailAttachment{
				URL: linshiyouOrigin + dm[1],
			})
		}

		out = append(out, Email{
			ID:          id,
			From:        from,
			To:          recipientEmail,
			Subject:     subject,
			Text:        text,
			HTML:        htmlBody,
			Date:        linshiyouParseDate(timeBody),
			IsRead:      false,
			Attachments: attachments,
		})
	}
	return out
}

func linshiyouGenerate() (*EmailInfo, error) {
	req, err := http.NewRequest("GET", linshiyouOrigin+"/api/user?user", nil)
	if err != nil {
		return nil, err
	}
	setLinshiyouHeaders(req)

	client := HTTPClient()
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	if err := checkHTTPStatus(resp, "linshiyou generate"); err != nil {
		return nil, err
	}

	var nexus string
	for _, c := range resp.Cookies() {
		if c.Name == "NEXUS_TOKEN" {
			nexus = c.Value
			break
		}
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	email := strings.TrimSpace(string(body))

	if nexus == "" {
		return nil, fmt.Errorf("linshiyou: no NEXUS_TOKEN in Set-Cookie")
	}
	if email == "" || !strings.Contains(email, "@") {
		return nil, fmt.Errorf("linshiyou: invalid email in response body")
	}

	token := fmt.Sprintf("NEXUS_TOKEN=%s; tmail-emails=%s", nexus, email)

	return &EmailInfo{
		Channel: ChannelLinshiyou,
		Email:   email,
		token:   token,
	}, nil
}

func linshiyouGetEmails(token string, email string) ([]Email, error) {
	req, err := http.NewRequest("GET", linshiyouOrigin+"/api/mail?unseen=1", nil)
	if err != nil {
		return nil, err
	}
	setLinshiyouHeaders(req)
	req.Header.Set("Cookie", token)
	req.Header.Set("X-Requested-With", "XMLHttpRequest")

	client := HTTPClient()
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	if err := checkHTTPStatus(resp, "linshiyou get emails"); err != nil {
		return nil, err
	}

	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	return linshiyouParseMailSegments(string(raw), email), nil
}
