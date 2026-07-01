package provider

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/url"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

const ockitoBase = "https://ockito.com/web-api"

func ockitoAnyString(v any) string {
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

func ockitoJSON(method, u string, headers map[string]string, body []byte) (int, map[string]any, error) {
	var reader io.Reader
	if body != nil {
		reader = bytes.NewReader(body)
	}
	req, err := http.NewRequest(method, u, reader)
	if err != nil {
		return 0, nil, err
	}
	req.Header.Set("Accept", "application/json")
	if body != nil {
		req.Header.Set("Content-Type", "application/json")
	}
	for k, v := range headers {
		req.Header.Set(k, v)
	}

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return 0, nil, err
	}
	defer resp.Body.Close()

	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return resp.StatusCode, nil, err
	}
	var out map[string]any
	if len(raw) > 0 {
		if err := json.Unmarshal(raw, &out); err != nil {
			return resp.StatusCode, nil, fmt.Errorf("ockito invalid JSON: %s HTTP %d", u, resp.StatusCode)
		}
	} else {
		out = map[string]any{}
	}
	return resp.StatusCode, out, nil
}

func ockitoEncodeToken(accessToken, refreshToken string) string {
	raw, _ := json.Marshal(map[string]string{
		"access_token":  accessToken,
		"refresh_token": refreshToken,
	})
	return string(raw)
}

func ockitoDecodeToken(token string) (string, string, error) {
	value := strings.TrimSpace(token)
	if value == "" || !strings.HasPrefix(value, "{") {
		return "", "", fmt.Errorf("ockito: invalid session token")
	}
	var payload map[string]any
	if err := json.Unmarshal([]byte(value), &payload); err != nil {
		return "", "", fmt.Errorf("ockito: invalid session token")
	}
	accessToken := ockitoAnyString(payload["access_token"])
	refreshToken := ockitoAnyString(payload["refresh_token"])
	if accessToken == "" || refreshToken == "" {
		return "", "", fmt.Errorf("ockito: invalid session token")
	}
	return accessToken, refreshToken, nil
}

func ockitoRefreshAccessToken(refreshToken string) (string, error) {
	status, data, err := ockitoJSON(http.MethodPost, ockitoBase+"/grefresh", map[string]string{
		"Authorization": "Bearer " + refreshToken,
		"X-PASSTHROUGH": "Y",
	}, nil)
	if err != nil {
		return "", err
	}
	if status < 200 || status >= 300 {
		return "", fmt.Errorf("ockito grefresh http %d", status)
	}
	accessToken := ockitoAnyString(data["access_token"])
	if accessToken == "" {
		return "", fmt.Errorf("ockito: invalid refresh response")
	}
	return accessToken, nil
}

func ockitoFetchBearerJSON(u string, accessTokenState *string, refreshToken string) (map[string]any, error) {
	status, data, err := ockitoJSON(http.MethodGet, u, map[string]string{
		"Authorization": "Bearer " + *accessTokenState,
	}, nil)
	if err != nil {
		return nil, err
	}
	if status == 401 {
		refreshed, err := ockitoRefreshAccessToken(refreshToken)
		if err != nil {
			return nil, err
		}
		*accessTokenState = refreshed
		status, data, err = ockitoJSON(http.MethodGet, u, map[string]string{
			"Authorization": "Bearer " + refreshed,
		}, nil)
		if err != nil {
			return nil, err
		}
	}
	if status < 200 || status >= 300 {
		return nil, fmt.Errorf("ockito http %d", status)
	}
	return data, nil
}

func ockitoFlattenInboxRow(raw map[string]any, recipient string) map[string]any {
	return map[string]any{
		"id":          ockitoAnyString(raw["uid"]),
		"from":        ockitoAnyString(raw["sender"]),
		"to":          recipient,
		"subject":     ockitoAnyString(raw["subject"]),
		"text":        ockitoAnyString(raw["snippet"]),
		"html":        ockitoAnyString(raw["html"]),
		"date":        raw["timestamp"],
		"is_read":     false,
		"attachments": []any{},
	}
}

func ockitoFlattenMessage(raw map[string]any, recipient string) map[string]any {
	obj, _ := raw["obj"].(map[string]any)
	if obj == nil {
		obj = raw
	}
	from := ockitoAnyString(obj["sender_email"])
	if from == "" {
		from = ockitoAnyString(obj["SenderEmail"])
	}
	if from == "" {
		from = ockitoAnyString(obj["from_"])
	}
	if from == "" {
		from = ockitoAnyString(obj["From"])
	}
	if from == "" {
		from = ockitoAnyString(obj["from"])
	}
	if from == "" {
		from = ockitoAnyString(obj["sender_name"])
	}
	if from == "" {
		from = ockitoAnyString(obj["SenderName"])
	}
	to := ockitoAnyString(obj["to"])
	if to == "" {
		to = ockitoAnyString(obj["To"])
	}
	if to == "" {
		to = recipient
	}
	return map[string]any{
		"id":          ockitoAnyString(raw["uid"]),
		"from":        from,
		"to":          to,
		"subject":     ockitoAnyString(obj["subject"]),
		"text":        ockitoAnyString(obj["text"]),
		"html":        ockitoAnyString(obj["html"]),
		"date":        obj["date"],
		"is_read":     false,
		"attachments": []any{},
	}
}

func OckitoGenerate() (*CreatedMailbox, error) {
	status, login, err := ockitoJSON(http.MethodPost, ockitoBase+"/gtoken", nil, []byte("{}"))
	if err != nil {
		return nil, err
	}
	if status < 200 || status >= 300 {
		return nil, fmt.Errorf("ockito gtoken http %d", status)
	}

	accessToken := ockitoAnyString(login["access_token"])
	refreshToken := ockitoAnyString(login["refresh_token"])
	if accessToken == "" || refreshToken == "" {
		return nil, fmt.Errorf("ockito: invalid token response")
	}

	status, emailResp, err := ockitoJSON(http.MethodGet, ockitoBase+"/email", map[string]string{
		"Authorization": "Bearer " + accessToken,
	}, nil)
	if err != nil {
		return nil, err
	}
	if status < 200 || status >= 300 {
		return nil, fmt.Errorf("ockito email http %d", status)
	}

	email := ockitoAnyString(emailResp["email"])
	if email == "" {
		if nested, ok := emailResp["email"].(map[string]any); ok {
			email = ockitoAnyString(nested["email"])
		}
	}
	if email == "" || !strings.Contains(email, "@") {
		return nil, fmt.Errorf("ockito: invalid email response")
	}

	return &CreatedMailbox{
		Channel: "ockito",
		Email:   email,
		Token:   ockitoEncodeToken(accessToken, refreshToken),
	}, nil
}

func OckitoGetEmails(token, email string) ([]NormEmail, error) {
	accessToken, refreshToken, err := ockitoDecodeToken(token)
	if err != nil {
		return nil, err
	}
	address := strings.TrimSpace(email)
	if address == "" {
		return nil, fmt.Errorf("ockito: empty email")
	}

	data, err := ockitoFetchBearerJSON(
		ockitoBase+"/retrieve/"+url.PathEscape(address)+"/emails",
		&accessToken,
		refreshToken,
	)
	if err != nil {
		return nil, err
	}
	rows, _ := data["inbox"].([]any)
	out := make([]NormEmail, 0, len(rows))
	for _, row := range rows {
		raw, ok := row.(map[string]any)
		if !ok {
			continue
		}
		uid := ockitoAnyString(raw["uid"])
		if uid == "" {
			out = append(out, NormalizeMap(ockitoFlattenInboxRow(raw, address), address))
			continue
		}

		detail, err := ockitoFetchBearerJSON(
			ockitoBase+"/retrieve/"+url.PathEscape(address)+"/"+url.PathEscape(uid),
			&accessToken,
			refreshToken,
		)
		if err != nil {
			out = append(out, NormalizeMap(ockitoFlattenInboxRow(raw, address), address))
			continue
		}
		out = append(out, NormalizeMap(ockitoFlattenMessage(detail, address), address))
	}
	return out, nil
}
