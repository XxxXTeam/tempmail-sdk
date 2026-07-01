package provider

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/url"

	http "github.com/bogdanfinn/fhttp"
)

const emailnatorBase = "https://www.emailnator.com"

var emailnatorOptions = []string{"plusGmail", "dotGmail", "googleMail"}

type emailnatorSession struct {
	XSRFToken string `json:"xsrfToken"`
	Cookie    string `json:"cookie"`
}

type emailnatorGenerateResponse struct {
	Email []string `json:"email"`
}

type emailnatorMessageRow struct {
	MessageID string `json:"messageID"`
	From      string `json:"from"`
	Subject   string `json:"subject"`
	Time      string `json:"time"`
}

type emailnatorListResponse struct {
	MessageData []emailnatorMessageRow `json:"messageData"`
}

func emailnatorInitSession() (emailnatorSession, error) {
	req, err := http.NewRequest("GET", emailnatorBase, nil)
	if err != nil {
		return emailnatorSession{}, err
	}
	req.Header.Set("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8")
	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36")

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return emailnatorSession{}, err
	}
	defer resp.Body.Close()
	if err := CheckHTTPStatus(resp, "emailnator home"); err != nil {
		return emailnatorSession{}, err
	}

	cookie := moaktMergeCookies("", resp.Cookies())
	var xsrf string
	for _, c := range resp.Cookies() {
		if c.Name == "XSRF-TOKEN" {
			xsrf = c.Value
			if decoded, err := url.QueryUnescape(xsrf); err == nil {
				xsrf = decoded
			}
			break
		}
	}
	if cookie == "" || xsrf == "" {
		return emailnatorSession{}, fmt.Errorf("emailnator: missing session cookie or XSRF token")
	}
	return emailnatorSession{XSRFToken: xsrf, Cookie: cookie}, nil
}

func emailnatorPost(session emailnatorSession, path string, body any) ([]byte, error) {
	reqBody, err := json.Marshal(body)
	if err != nil {
		return nil, err
	}
	req, err := http.NewRequest("POST", emailnatorBase+path, bytes.NewReader(reqBody))
	if err != nil {
		return nil, err
	}
	req.Header.Set("Accept", "application/json, text/plain, */*")
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Origin", emailnatorBase)
	req.Header.Set("Referer", emailnatorBase+"/")
	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36")
	req.Header.Set("X-Requested-With", "XMLHttpRequest")
	req.Header.Set("X-XSRF-TOKEN", session.XSRFToken)
	req.Header.Set("Cookie", session.Cookie)

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
		return nil, fmt.Errorf("emailnator %s: %d %s", path, resp.StatusCode, string(raw))
	}
	return raw, nil
}

func emailnatorEncodeSession(session emailnatorSession) string {
	raw, _ := json.Marshal(session)
	return string(raw)
}

func emailnatorDecodeSession(token string) (emailnatorSession, error) {
	var session emailnatorSession
	if err := json.Unmarshal([]byte(token), &session); err != nil {
		return session, fmt.Errorf("emailnator: invalid session token: %w", err)
	}
	if session.Cookie == "" || session.XSRFToken == "" {
		return session, fmt.Errorf("emailnator: invalid session token")
	}
	return session, nil
}

func EmailnatorGenerate() (*CreatedMailbox, error) {
	session, err := emailnatorInitSession()
	if err != nil {
		return nil, err
	}
	raw, err := emailnatorPost(session, "/generate-email", map[string][]string{"email": emailnatorOptions})
	if err != nil {
		return nil, err
	}
	var data emailnatorGenerateResponse
	if err := json.Unmarshal(raw, &data); err != nil {
		return nil, err
	}
	email := ""
	for _, item := range data.Email {
		if item != "" {
			email = item
			break
		}
	}
	if email == "" {
		return nil, fmt.Errorf("emailnator: empty email response")
	}
	return &CreatedMailbox{
		Channel: "emailnator",
		Email:   email,
		Token:   emailnatorEncodeSession(session),
	}, nil
}

func emailnatorFetchDetail(session emailnatorSession, email string, messageID string) string {
	raw, err := emailnatorPost(session, "/message-list", map[string]string{"email": email, "messageID": messageID})
	if err != nil {
		return ""
	}
	var html string
	if err := json.Unmarshal(raw, &html); err == nil {
		return html
	}
	return string(raw)
}

func EmailnatorGetEmails(token string, email string) ([]NormEmail, error) {
	session, err := emailnatorDecodeSession(token)
	if err != nil {
		return nil, err
	}
	raw, err := emailnatorPost(session, "/message-list", map[string]string{"email": email})
	if err != nil {
		return nil, err
	}
	var data emailnatorListResponse
	if err := json.Unmarshal(raw, &data); err != nil {
		return nil, err
	}
	out := make([]NormEmail, 0, len(data.MessageData))
	for _, row := range data.MessageData {
		if row.MessageID == "" || len(row.MessageID) >= 3 && row.MessageID[:3] == "ADS" {
			continue
		}
		out = append(out, NormalizeMap(map[string]interface{}{
			"id":          row.MessageID,
			"from":        row.From,
			"to":          email,
			"subject":     row.Subject,
			"html":        emailnatorFetchDetail(session, email, row.MessageID),
			"date":        row.Time,
			"isRead":      false,
			"attachments": []interface{}{},
		}, email))
	}
	return out, nil
}
