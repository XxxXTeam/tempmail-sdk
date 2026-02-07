package tempemail

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"time"
)

const (
	linshiEmailBaseURL = "https://www.linshi-email.com/api/v1"
	linshiEmailAPIKey  = "552562b8524879814776e52bc8de5c9f"
)

type linshiEmailGenerateResponse struct {
	Data struct {
		Email   string `json:"email"`
		Expired int64  `json:"expired"`
	} `json:"data"`
	Status string `json:"status"`
}

type linshiEmailEmailsResponse struct {
	List   []json.RawMessage `json:"list"`
	Status string            `json:"status"`
}

func linshiEmailGenerate() (*EmailInfo, error) {
	reqBody := []byte("{}")

	req, err := http.NewRequest("POST", linshiEmailBaseURL+"/email/"+linshiEmailAPIKey, bytes.NewReader(reqBody))
	if err != nil {
		return nil, err
	}

	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36")
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Origin", "https://www.linshi-email.com")
	req.Header.Set("Referer", "https://www.linshi-email.com/")
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

	var result linshiEmailGenerateResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, err
	}

	if result.Status != "ok" {
		return nil, fmt.Errorf("failed to generate email")
	}

	return &EmailInfo{
		Channel:   ChannelLinshiEmail,
		Email:     result.Data.Email,
		ExpiresAt: result.Data.Expired,
	}, nil
}

func linshiEmailGetEmails(email string) ([]Email, error) {
	encodedEmail := url.QueryEscape(email)
	timestamp := time.Now().UnixMilli()

	reqURL := fmt.Sprintf("%s/refreshmessage/%s/%s?t=%d", linshiEmailBaseURL, linshiEmailAPIKey, encodedEmail, timestamp)

	req, err := http.NewRequest("GET", reqURL, nil)
	if err != nil {
		return nil, err
	}

	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36")
	req.Header.Set("Referer", "https://www.linshi-email.com/")
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

	var result linshiEmailEmailsResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, err
	}

	if result.Status != "ok" {
		return nil, fmt.Errorf("failed to get emails")
	}

	return normalizeRawEmails(result.List, email)
}
