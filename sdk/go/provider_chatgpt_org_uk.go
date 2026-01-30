package tempemail

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/url"
)

const chatgptOrgUkBaseURL = "https://mail.chatgpt.org.uk/api"

type chatgptOrgUkGenerateResponse struct {
	Success bool `json:"success"`
	Data    struct {
		Email string `json:"email"`
	} `json:"data"`
}

type chatgptOrgUkEmailsResponse struct {
	Success bool `json:"success"`
	Data    struct {
		Emails []Email `json:"emails"`
		Count  int     `json:"count"`
	} `json:"data"`
}

func chatgptOrgUkGenerate() (*EmailInfo, error) {
	req, err := http.NewRequest("GET", chatgptOrgUkBaseURL+"/generate-email", nil)
	if err != nil {
		return nil, err
	}

	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36")
	req.Header.Set("Accept", "*/*")
	req.Header.Set("Referer", "https://mail.chatgpt.org.uk/")
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

	var result chatgptOrgUkGenerateResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, err
	}

	if !result.Success {
		return nil, fmt.Errorf("failed to generate email")
	}

	return &EmailInfo{
		Channel: ChannelChatgptOrgUk,
		Email:   result.Data.Email,
	}, nil
}

func chatgptOrgUkGetEmails(email string) ([]Email, error) {
	encodedEmail := url.QueryEscape(email)

	req, err := http.NewRequest("GET", chatgptOrgUkBaseURL+"/emails?email="+encodedEmail, nil)
	if err != nil {
		return nil, err
	}

	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36")
	req.Header.Set("Accept", "*/*")
	req.Header.Set("Referer", "https://mail.chatgpt.org.uk/")
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

	var result chatgptOrgUkEmailsResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, err
	}

	if !result.Success {
		return nil, fmt.Errorf("failed to get emails")
	}

	return result.Data.Emails, nil
}
