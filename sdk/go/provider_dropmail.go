package tempemail

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/url"
)

const dropmailBaseURL = "https://dropmail.me/api/graphql/MY_TOKEN"

const dropmailCreateSessionQuery = `mutation {introduceSession {id, expiresAt, addresses{id, address}}}`

const dropmailGetMailsQuery = `query ($id: ID!) {
  session(id:$id) {
    mails {
      id, rawSize, fromAddr, toAddr, receivedAt,
      text, headerFrom, headerSubject, html
    }
  }
}`

type dropmailGraphQLResponse struct {
	Data   json.RawMessage `json:"data"`
	Errors []struct {
		Message string `json:"message"`
	} `json:"errors"`
}

type dropmailSession struct {
	ID        string `json:"id"`
	ExpiresAt string `json:"expiresAt"`
	Addresses []struct {
		ID      string `json:"id"`
		Address string `json:"address"`
	} `json:"addresses"`
}

type dropmailIntroduceSessionResponse struct {
	IntroduceSession dropmailSession `json:"introduceSession"`
}

type dropmailSessionQueryResponse struct {
	Session struct {
		Mails []map[string]interface{} `json:"mails"`
	} `json:"session"`
}

// dropmailGraphQLRequest 执行 GraphQL 请求
func dropmailGraphQLRequest(query string, variables map[string]interface{}) (json.RawMessage, error) {
	form := url.Values{}
	form.Set("query", query)
	if variables != nil {
		varsJSON, _ := json.Marshal(variables)
		form.Set("variables", string(varsJSON))
	}

	resp, err := http.PostForm(dropmailBaseURL, form)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	if resp.StatusCode >= 400 {
		return nil, fmt.Errorf("GraphQL request failed: %d", resp.StatusCode)
	}

	var result dropmailGraphQLResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, err
	}

	if len(result.Errors) > 0 {
		return nil, fmt.Errorf("GraphQL error: %s", result.Errors[0].Message)
	}

	return result.Data, nil
}

// dropmailGenerate 创建临时邮箱
// GraphQL mutation: introduceSession
func dropmailGenerate() (*EmailInfo, error) {
	data, err := dropmailGraphQLRequest(dropmailCreateSessionQuery, nil)
	if err != nil {
		return nil, err
	}

	var resp dropmailIntroduceSessionResponse
	if err := json.Unmarshal(data, &resp); err != nil {
		return nil, err
	}

	session := resp.IntroduceSession
	if session.ID == "" || len(session.Addresses) == 0 {
		return nil, fmt.Errorf("failed to generate email")
	}

	return &EmailInfo{
		Channel:   ChannelDropmail,
		Email:     session.Addresses[0].Address,
		Token:     session.ID,
		ExpiresAt: session.ExpiresAt,
	}, nil
}

// dropmailFlattenMessage 将 dropmail 的邮件格式扁平化
func dropmailFlattenMessage(mail map[string]interface{}, recipientEmail string) map[string]interface{} {
	flat := map[string]interface{}{
		"id":          mail["id"],
		"from":        mail["fromAddr"],
		"to":          mail["toAddr"],
		"subject":     mail["headerSubject"],
		"text":        mail["text"],
		"html":        mail["html"],
		"received_at": mail["receivedAt"],
		"attachments": []interface{}{},
	}

	if flat["to"] == nil || flat["to"] == "" {
		flat["to"] = recipientEmail
	}

	return flat
}

// dropmailGetEmails 获取邮件列表
// GraphQL query: session(id) { mails {...} }
func dropmailGetEmails(token string, email string) ([]Email, error) {
	data, err := dropmailGraphQLRequest(dropmailGetMailsQuery, map[string]interface{}{
		"id": token,
	})
	if err != nil {
		return nil, err
	}

	var resp dropmailSessionQueryResponse
	if err := json.Unmarshal(data, &resp); err != nil {
		return nil, err
	}

	mails := resp.Session.Mails
	if len(mails) == 0 {
		return []Email{}, nil
	}

	emails := make([]Email, 0, len(mails))
	for _, mail := range mails {
		flat := dropmailFlattenMessage(mail, email)
		emails = append(emails, normalizeRawEmail(flat, email))
	}
	return emails, nil
}
