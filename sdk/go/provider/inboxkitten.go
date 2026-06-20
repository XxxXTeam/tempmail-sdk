package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"math/rand"
	"net/url"
	"regexp"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

const inboxkittenBase = "https://inboxkitten.com/api/v1/mail"
const inboxkittenDomain = "inboxkitten.com"

type inboxkittenListItem struct {
	Storage struct {
		Region string `json:"region"`
		Key    string `json:"key"`
	} `json:"storage"`
	Message struct {
		Headers struct {
			From    string `json:"from"`
			Subject string `json:"subject"`
		} `json:"headers"`
	} `json:"message"`
	Envelope struct {
		Sender  string `json:"sender"`
		Targets string `json:"targets"`
	} `json:"envelope"`
	Recipient string  `json:"recipient"`
	Timestamp float64 `json:"timestamp"`
}

type inboxkittenMeta struct {
	Name       string `json:"name"`
	Subject    string `json:"subject"`
	Recipients string `json:"recipients"`
}

func inboxkittenLocal() string {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	var b strings.Builder
	b.WriteString("sdk")
	for i := 0; i < 16; i++ {
		b.WriteByte(chars[rand.Intn(len(chars))])
	}
	return b.String()
}

func inboxkittenGet(u string, accept string) ([]byte, error) {
	req, err := http.NewRequest("GET", u, nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("Accept", accept)
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
		return nil, fmt.Errorf("inboxkitten: http %d", resp.StatusCode)
	}
	return body, nil
}

func inboxkittenHTMLText(html string) string {
	reScript := regexp.MustCompile(`(?is)<script[\s\S]*?</script>`)
	reTag := regexp.MustCompile(`(?s)<[^>]+>`)
	text := reScript.ReplaceAllString(html, " ")
	text = reTag.ReplaceAllString(text, " ")
	text = strings.NewReplacer(
		"&nbsp;", " ",
		"&amp;", "&",
		"&lt;", "<",
		"&gt;", ">",
		"&quot;", `"`,
		"&#39;", "'",
	).Replace(text)
	return strings.Join(strings.Fields(text), " ")
}

func InboxkittenGenerate() (*CreatedMailbox, error) {
	local := inboxkittenLocal()
	return &CreatedMailbox{Channel: "inboxkitten", Email: local + "@" + inboxkittenDomain}, nil
}

func inboxkittenFetchDetail(row inboxkittenListItem, recipient string) map[string]any {
	raw := map[string]any{
		"id":        row.Storage.Key,
		"messageId": row.Storage.Key,
		"from":      row.Message.Headers.From,
		"to":        recipient,
		"subject":   row.Message.Headers.Subject,
		"timestamp": row.Timestamp,
	}
	if raw["from"] == "" {
		raw["from"] = row.Envelope.Sender
	}
	if row.Recipient != "" {
		raw["to"] = row.Recipient
	} else if row.Envelope.Targets != "" {
		raw["to"] = row.Envelope.Targets
	}
	if row.Storage.Region == "" || row.Storage.Key == "" {
		return raw
	}

	q := url.Values{}
	q.Set("region", row.Storage.Region)
	q.Set("key", row.Storage.Key)
	metaBody, metaErr := inboxkittenGet(inboxkittenBase+"/getKey?"+q.Encode(), "application/json")
	htmlBody, htmlErr := inboxkittenGet(inboxkittenBase+"/getHtml?"+q.Encode(), "text/html,*/*")
	if metaErr == nil {
		var meta inboxkittenMeta
		if json.Unmarshal(metaBody, &meta) == nil {
			if meta.Name != "" {
				raw["from"] = meta.Name
			}
			if meta.Recipients != "" {
				raw["to"] = meta.Recipients
			}
			if meta.Subject != "" {
				raw["subject"] = meta.Subject
			}
		}
	}
	if htmlErr == nil {
		html := string(htmlBody)
		raw["html"] = html
		raw["text"] = inboxkittenHTMLText(html)
	}
	return raw
}

func InboxkittenGetEmails(email string) ([]NormEmail, error) {
	parts := strings.Split(strings.TrimSpace(email), "@")
	local := parts[0]
	if local == "" {
		return nil, fmt.Errorf("inboxkitten: empty email")
	}

	q := url.Values{}
	q.Set("recipient", local)
	body, err := inboxkittenGet(inboxkittenBase+"/list?"+q.Encode(), "application/json")
	if err != nil {
		return nil, err
	}
	var rows []inboxkittenListItem
	if err := json.Unmarshal(body, &rows); err != nil {
		return nil, err
	}
	out := make([]NormEmail, 0, len(rows))
	for _, row := range rows {
		out = append(out, NormalizeMap(inboxkittenFetchDetail(row, email), email))
	}
	return out, nil
}
