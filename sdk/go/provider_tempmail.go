package tempemail

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/url"
)

const tempmailBaseURL = "https://api.tempmail.ing/api"

type tempmailGenerateRequest struct {
	Duration int `json:"duration"`
}

type tempmailGenerateResponse struct {
	Email struct {
		Address         string `json:"address"`
		ExpiresAt       string `json:"expiresAt"`
		CreatedAt       string `json:"createdAt"`
		DurationMinutes int    `json:"durationMinutes"`
	} `json:"email"`
	Success bool `json:"success"`
}

type tempmailEmailsResponse struct {
	Emails  []json.RawMessage `json:"emails"`
	Success bool              `json:"success"`
}

func tempmailGenerate(duration int) (*EmailInfo, error) {
	if duration <= 0 {
		duration = 30
	}

	reqBody, _ := json.Marshal(tempmailGenerateRequest{Duration: duration})

	req, err := http.NewRequest("POST", tempmailBaseURL+"/generate", bytes.NewReader(reqBody))
	if err != nil {
		return nil, err
	}

	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36")
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Referer", "https://tempmail.ing/")
	req.Header.Set("DNT", "1")

	client := HTTPClient()
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	var result tempmailGenerateResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, err
	}

	if !result.Success {
		return nil, fmt.Errorf("failed to generate email")
	}

	return &EmailInfo{
		Channel:   ChannelTempmail,
		Email:     result.Email.Address,
		ExpiresAt: result.Email.ExpiresAt,
		CreatedAt: result.Email.CreatedAt,
	}, nil
}

func tempmailGetEmails(email string) ([]Email, error) {
	encodedEmail := url.QueryEscape(email)

	req, err := http.NewRequest("GET", tempmailBaseURL+"/emails/"+encodedEmail, nil)
	if err != nil {
		return nil, err
	}

	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36")
	req.Header.Set("Referer", "https://tempmail.ing/")
	req.Header.Set("DNT", "1")

	client := HTTPClient()
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	var result tempmailEmailsResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, err
	}

	if !result.Success {
		return nil, fmt.Errorf("failed to get emails")
	}

	return normalizeRawEmails(result.Emails, email)
}
