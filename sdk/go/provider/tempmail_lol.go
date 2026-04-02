package provider

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/url"

	http "github.com/bogdanfinn/fhttp"
)

const tempmailLolBaseURL = "https://api.tempmail.lol/v2"

type TempmailLolGenerateRequest struct {
	Domain  *string `json:"domain"`
	Captcha *string `json:"captcha"`
}

type TempmailLolGenerateResponse struct {
	Address string `json:"address"`
	Token   string `json:"token"`
}

type tempmailLolEmailsResponse struct {
	Emails  []json.RawMessage `json:"emails"`
	Expired bool              `json:"expired"`
}

func TempmailLolGenerate(domain *string) (*CreatedMailbox, error) {
	reqBody, _ := json.Marshal(TempmailLolGenerateRequest{Domain: domain, Captcha: nil})

	req, err := http.NewRequest("POST", tempmailLolBaseURL+"/inbox/create", bytes.NewReader(reqBody))
	if err != nil {
		return nil, err
	}

	req.Header.Set("User-Agent", GetCurrentUA())
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Origin", "https://tempmail.lol")
	req.Header.Set("DNT", "1")

	client := HTTPClient()
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	if err := CheckHTTPStatus(resp, "tempmail-lol generate"); err != nil {
		return nil, err
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	var result TempmailLolGenerateResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, err
	}

	if result.Address == "" || result.Token == "" {
		return nil, fmt.Errorf("failed to generate email")
	}

	info := &CreatedMailbox{Channel: "tempmail-lol", Email: result.Address, Token: result.Token}
	return info, nil
}

func TempmailLolGetEmails(token string, recipientEmail string) ([]NormEmail, error) {
	encodedToken := url.QueryEscape(token)

	req, err := http.NewRequest("GET", tempmailLolBaseURL+"/inbox?token="+encodedToken, nil)
	if err != nil {
		return nil, err
	}

	req.Header.Set("User-Agent", GetCurrentUA())
	req.Header.Set("Origin", "https://tempmail.lol")
	req.Header.Set("DNT", "1")

	client := HTTPClient()
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	if err := CheckHTTPStatus(resp, "tempmail-lol get emails"); err != nil {
		return nil, err
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	var result tempmailLolEmailsResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, err
	}

	return NormalizeRawMessages(result.Emails, recipientEmail)
}
