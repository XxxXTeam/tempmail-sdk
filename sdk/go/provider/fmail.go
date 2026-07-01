package provider

import (
	"encoding/json"
	"fmt"
	"io"
	"net/url"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

const fmailBase = "https://fmail.men"

func fmailAnyString(v any) string {
	switch x := v.(type) {
	case string:
		return strings.TrimSpace(x)
	case fmt.Stringer:
		return strings.TrimSpace(x.String())
	case nil:
		return ""
	default:
		return strings.TrimSpace(fmt.Sprint(v))
	}
}

func fmailJSON(method, u string) (map[string]any, error) {
	req, err := http.NewRequest(method, u, nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("Accept", "application/json")
	req.Header.Set("User-Agent", GetCurrentUA())

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
		return nil, fmt.Errorf("fmail http %d", resp.StatusCode)
	}

	var out map[string]any
	if err := json.Unmarshal(raw, &out); err != nil {
		return nil, err
	}
	return out, nil
}

func fmailNormalizeDomain(domain *string) *string {
	if domain == nil {
		return nil
	}
	value := strings.TrimSpace(*domain)
	value = strings.TrimPrefix(value, "@")
	if value == "" {
		return nil
	}
	return &value
}

func fmailFlattenMessage(raw map[string]any, recipient string) map[string]any {
	attachments := raw["attachments"]
	if attachments == nil {
		attachments = []any{}
	}
	from := fmailAnyString(raw["sender"])
	if from == "" {
		from = fmailAnyString(raw["from"])
	}
	text := fmailAnyString(raw["body_text"])
	if text == "" {
		text = fmailAnyString(raw["text"])
	}
	if text == "" {
		text = fmailAnyString(raw["snippet"])
	}
	html := fmailAnyString(raw["body_html"])
	if html == "" {
		html = fmailAnyString(raw["html"])
	}
	date := fmailAnyString(raw["received_at"])
	if date == "" {
		date = fmailAnyString(raw["timestamp"])
	}
	if date == "" {
		date = fmailAnyString(raw["date"])
	}
	id := fmailAnyString(raw["id"])
	if id == "" {
		id = fmailAnyString(raw["uid"])
	}
	if id == "" {
		id = fmailAnyString(raw["token"])
	}
	to := fmailAnyString(raw["recipient"])
	if to == "" {
		to = fmailAnyString(raw["to"])
	}
	if to == "" {
		to = recipient
	}
	return map[string]any{
		"id":          id,
		"from":        from,
		"to":          to,
		"subject":     fmailAnyString(raw["subject"]),
		"text":        text,
		"html":        html,
		"date":        date,
		"is_read":     raw["is_read"],
		"attachments": attachments,
	}
}

func FmailGenerate(domain *string) (*CreatedMailbox, error) {
	selected := fmailNormalizeDomain(domain)
	u := fmailBase + "/v1/random"
	if selected != nil {
		u += "?domain=" + url.QueryEscape(*selected)
	}
	data, err := fmailJSON(http.MethodGet, u)
	if err != nil {
		return nil, err
	}

	email := fmailAnyString(data["address"])
	if email == "" {
		username := fmailAnyString(data["username"])
		dom := fmailAnyString(data["domain"])
		if username != "" && dom != "" {
			email = username + "@" + dom
		}
	}
	if email == "" || !strings.Contains(email, "@") {
		return nil, fmt.Errorf("fmail: invalid random response")
	}

	return &CreatedMailbox{
		Channel: "fmail",
		Email:   email,
	}, nil
}

func FmailGetEmails(email string) ([]NormEmail, error) {
	address := strings.TrimSpace(email)
	if address == "" {
		return nil, fmt.Errorf("fmail: invalid email")
	}
	parts := strings.SplitN(address, "@", 2)
	if len(parts) != 2 || strings.TrimSpace(parts[0]) == "" || strings.TrimSpace(parts[1]) == "" {
		return nil, fmt.Errorf("fmail: invalid email")
	}

	u := fmt.Sprintf("%s/v1/inbox/%s?domain=%s&limit=50", fmailBase, url.PathEscape(strings.TrimSpace(parts[0])), url.QueryEscape(strings.TrimSpace(parts[1])))
	data, err := fmailJSON(http.MethodGet, u)
	if err != nil {
		return nil, err
	}

	rows, _ := data["emails"].([]any)
	out := make([]NormEmail, 0, len(rows))
	for _, row := range rows {
		raw, ok := row.(map[string]any)
		if !ok {
			continue
		}
		token := fmailAnyString(raw["token"])
		if token == "" {
			token = fmailAnyString(raw["id"])
		}
		if token == "" {
			out = append(out, NormalizeMap(fmailFlattenMessage(raw, address), address))
			continue
		}

		detail, err := fmailJSON(http.MethodGet, fmailBase+"/v1/email/"+url.PathEscape(token))
		if err != nil {
			out = append(out, NormalizeMap(fmailFlattenMessage(raw, address), address))
			continue
		}
		if nested, ok := detail["email"].(map[string]any); ok {
			detail = nested
		}
		out = append(out, NormalizeMap(fmailFlattenMessage(detail, address), address))
	}

	return out, nil
}
