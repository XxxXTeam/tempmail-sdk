package tempemail

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/url"
)

const tempmailLolBaseURL = "https://api.tempmail.lol/v2"

type tempmailLolGenerateRequest struct {
	Domain  *string `json:"domain"`
	Captcha *string `json:"captcha"`
}

type tempmailLolGenerateResponse struct {
	Address string `json:"address"`
	Token   string `json:"token"`
}

type tempmailLolEmailsResponse struct {
	Emails  []Email `json:"emails"`
	Expired bool    `json:"expired"`
}

func tempmailLolGenerate(domain *string) (*EmailInfo, error) {
	reqBody, _ := json.Marshal(tempmailLolGenerateRequest{Domain: domain, Captcha: nil})

	req, err := http.NewRequest("POST", tempmailLolBaseURL+"/inbox/create", bytes.NewReader(reqBody))
	if err != nil {
		return nil, err
	}

	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36")
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Origin", "https://tempmail.lol")
	req.Header.Set("DNT", "1")

	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	var result tempmailLolGenerateResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, err
	}

	if result.Address == "" || result.Token == "" {
		return nil, fmt.Errorf("failed to generate email")
	}

	return &EmailInfo{
		Channel: ChannelTempmailLol,
		Email:   result.Address,
		Token:   result.Token,
	}, nil
}

func tempmailLolGetEmails(token string) ([]Email, error) {
	encodedToken := url.QueryEscape(token)

	req, err := http.NewRequest("GET", tempmailLolBaseURL+"/inbox?token="+encodedToken, nil)
	if err != nil {
		return nil, err
	}

	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36")
	req.Header.Set("Origin", "https://tempmail.lol")
	req.Header.Set("DNT", "1")

	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	var result tempmailLolEmailsResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, err
	}

	return result.Emails, nil
}
